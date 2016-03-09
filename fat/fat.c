#include "fat.h"

// Some formules are based on Microsofts Specification. Please read the full document to understand that math.
// Other small functions are just helper functions, code with extensive comments is the real magic
//
// Microsoft Extensible Firmware Initiative FAT32 File System Specification 
// http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc

typedef struct EntryState EntryState;
struct EntryState
{
    unsigned entryIndex;
    unsigned currentCluster;
    unsigned startCluster;
    unsigned currentSector;
    FatType fatType;

    enum 
    {
        EndOfTable = 1 << 0,
        LfnDirectoryEntry = 1 << 1,
        EndOfChain = 1 << 2
    } Status;
    enum Status flags;    
};
static EntryState _state;

void fat_getFileName(char* fileName, const fat_DirectoryEntry* entry)
{
    uint8_t x = 0;
    for (uint8_t i = 0; i < 8; ++i)
    {	              
        if (entry->fileName[i] == ' ')						
            break;

        fileName[x++] = tolower(entry->fileName[i]);
    }

    fileName[x++] = '.';
    for (uint8_t i = 0; i < 3; ++i)
    {
        if (entry->extension[i] == ' ')
            break;

        fileName[x++] = tolower(entry->extension[i]);
    }
}

static void UCS2ToUTF8(char* filename, const fat_LongFileName* lfn)
{
    assert(filename != NULL);
    assert(lfn != NULL);

    char* p = filename;
    for (uint8_t i = 0; i < 0x05; ++i)
        *p++ = *((char*)lfn->ucs2_1 + i * 2);

    for (uint8_t i = 0; i < 0x06; ++i)
        *p++ = *((char*)lfn->ucs2_2 + i * 2);

    for (uint8_t i = 0; i < 0x02; ++i)
        *p++ = *((char*)lfn->ucs2_3 + i * 2);
}

uint32_t fat_sectorsPerFat(const fat_BootSector * boot)
{
    assert(boot != NULL);

    return boot->sectorsPerFAT16 != 0
        ? boot->sectorsPerFAT16
        : ((fat32_BootSector*)boot->rest)->sectorsPerFAT32;						// finds sector in the fat32 part
}

uint32_t fat_totalFatSize(const fat_BootSector * boot)
{
    assert(boot != NULL);

    return fat_sectorsPerFat(boot) * boot->numberOfFATs * boot->bytesPerSector;
}

uint32_t fat_sectorToAddress(const fat_BootSector * boot, unsigned partitionOffset, uint32_t sector)
{
    assert(boot != NULL);

    return boot->bytesPerSector*sector + partitionOffset;
}

uint32_t fat_numberOfRootDirSectors(const fat_BootSector * boot)
{
    assert(boot != NULL);

    return (boot->rootEntries * 32 + boot->bytesPerSector - 1) / boot->bytesPerSector;
}

uint32_t fat_firstDataSector(const fat_BootSector * boot)
{
    assert(boot != NULL);

    uint32_t sectorsPerFat = fat_sectorsPerFat(boot);
    
    return boot->reservedSectors + (boot->numberOfFATs * sectorsPerFat);
}

uint32_t fat_firstSectorOfCluster(const fat_BootSector * boot, unsigned cluster)
{
    assert(boot != NULL);

    uint32_t firstDataSector = fat_firstDataSector(boot);

    return ((cluster - 2) * boot->sectorsPerCluster) + firstDataSector;
}

uint32_t fat_countOfSectors(const fat_BootSector * boot)
{
    assert(boot != NULL);

    return boot->totalSectors16 != 0
        ? boot->totalSectors16
        : boot->totalSectors32;
}

uint32_t fat_countOfClusters(const fat_BootSector * boot)
{
    assert(boot != NULL);

    uint32_t sectorsPerFat = fat_sectorsPerFat(boot);
    uint32_t totalSectors = fat_countOfSectors(boot);
    uint32_t rootDirSectors = fat_numberOfRootDirSectors(boot);
    uint32_t dataSectors = totalSectors - (boot->reservedSectors + (boot->numberOfFATs * sectorsPerFat) + rootDirSectors);

    return dataSectors / boot->sectorsPerCluster;
}

uint32_t fat_clusterSize(const fat_BootSector * boot)
{
    assert(boot != NULL);

    return boot->bytesPerSector * boot->sectorsPerCluster;
}

uint8_t fat_entriesPerCluster(const fat_BootSector * boot)
{
    assert(boot != NULL);

    uint32_t clusterSize = fat_clusterSize(boot);

    return clusterSize / sizeof(fat_DirectoryEntry);
}

FatType fat_getType(const fat_BootSector * boot)
{
    assert(boot != NULL);

    uint32_t countOfClusters = fat_countOfClusters(boot);

    return (countOfClusters < 4085)
        ? FAT12
        : (countOfClusters < 65525)
        ? FAT16
        : FAT32;
}

uint32_t fat_nextPartitionSector(fetchData_t fetchData, fat_BootSector* boot, uint8_t* eop)
{
    assert(fetchData != NULL);
    assert(boot != NULL);

    fat_MBR mbr;
    fetchData(0, sizeof(mbr), (char*)&mbr);

    static unsigned i = 0;                                                  // partition indexer (note static)
    uint32_t partitionOffset = 0;
    for (; i < 4; ++i)														// max 4 boot partitions
    {
        fat_PartitionEntry entry;                                           // read partition entry
        fetchData(offsetof(fat_MBR, partitionTable), sizeof(fat_PartitionEntry), (char*)&entry);	

        if ((entry.type | FAT_SUPPORTED_TYPES) == 0)						// check if it is supported
            continue;

        partitionOffset = entry.startSector * 512;							// calculate the offset (start of this partition)
        break;
    }

    if (partitionOffset > 0)
        fetchData(partitionOffset, sizeof(fat_BootSector), (char*)boot);	// read the actual bootsector
    
    if (i == 3)														        // reset the indexer
    {
        i = 0;
        if (eop)
            *eop = 1;                                                       // let know we reached the end
    }

    return partitionOffset;													// return the start of partition
}

uint32_t fat_nextClusterEntry(const fat_BootSector* boot, unsigned partitionOffset, unsigned cluster, fetchData_t fetch, uint8_t* eoc)
{
    assert(boot != NULL);
    assert(fetch != NULL);

    uint32_t sectorsPerFat = fat_sectorsPerFat(boot);
    FatType type = fat_getType(boot);

    assert( (type == FAT12 && cluster < 0x0FF8) ||
            (type == FAT16 && cluster < 0xFFF8) ||
            (type == FAT32 && cluster < 0x0FFFFFF8));

    unsigned fatOffset = (type == FAT12)
        ? cluster + (cluster / 2)
        : (type == FAT16)
            ? cluster * 2
            : cluster * 4;

    uint32_t thisFatSector = boot->reservedSectors + (fatOffset / boot->bytesPerSector);
    uint32_t thisFatEntry = fatOffset % boot->bytesPerSector;

    char* secBuf = malloc(boot->bytesPerSector);
    // TODO: General error
    assert(secBuf != NULL);

    uint32_t address = fat_sectorToAddress(boot, partitionOffset, thisFatSector);
    fetch(address, boot->bytesPerSector, secBuf);

    uint32_t clusterEntry = 0;
    if (type == FAT12)
    {
        clusterEntry = *((uint16_t*)&secBuf[thisFatEntry]);
        clusterEntry = (cluster & 0x0001)
            ? clusterEntry >> 4										// odd cluster number
            : clusterEntry & 0x0FFF;								// even cluster number

        if (eoc)
            *eoc = clusterEntry >= 0x0FF8;
    }
    else if (type == FAT16)
    {
        clusterEntry = *((uint16_t*)&secBuf[thisFatEntry]);

        if (eoc)
            *eoc = clusterEntry >= 0xFFF8;
    }
    else if (type == FAT32)
    {
        clusterEntry = *(((uint32_t*)&secBuf[thisFatEntry])) & 0x0FFFFFFF;

        if (eoc)
            *eoc = clusterEntry >= 0x0FFFFFF8;
    }
    else
        assert("Invalid type");

    free(secBuf);
    return clusterEntry;
}

uint8_t fat_firstDirectoryEntry(const fat_BootSector * boot, unsigned partitionOffset, unsigned startCluster, fetchData_t fetch, fat_DirectoryEntry* entry, char* fileName, unsigned nameLen)
{
    _state.startCluster = -1;                                       // resets nextDirectoryEntry
    return fat_nextDirectoryEntry(boot, startCluster, partitionOffset, fetch, entry, fileName, nameLen);
}

uint8_t fat_nextDirectoryEntry(const fat_BootSector * boot, unsigned partitionOffset, unsigned startCluster, fetchData_t fetch, fat_DirectoryEntry* entry, char* fileName, unsigned nameLen)
{
    assert(boot != NULL);
    assert(fetch != NULL);
    assert(entry != NULL);

    if (_state.startCluster != startCluster || _state.startCluster == -1)       // different start cluster -> restart
    {
        memset(&_state, 0, sizeof(EntryState));
        _state.startCluster = startCluster;
        _state.currentCluster = startCluster;
        _state.fatType = fat_getType(boot);
    }
    else                                                            // check if we have valid state
    {
        if (_state.flags & EndOfTable)                              // end has been reached
            return 0;
    }

    // TODO: Can't we use entry??
    char entryBuf[sizeof(fat_DirectoryEntry)];  
    char nameBuf[0xFF] = { 0 };                                     // buffer for the long file name

    fat_LongFileName* lfn = (fat_LongFileName*)&entryBuf;
    fat_DirectoryEntry* dir = (fat_DirectoryEntry*)&entryBuf;
    
    for (;  (_state.fatType != FAT32 && _state.entryIndex < boot->rootEntries) ||   // for FAT12/FAT16 the root entries is a given
            (_state.fatType == FAT32)                                               // FAT32 doesn't restrict this (could be as large as needed)
        ; ++_state.entryIndex)                                                      // just follow the chain like any file :)
    {   
        unsigned clusterEntryIndex = _state.entryIndex % fat_entriesPerCluster(boot);
        if (_state.entryIndex > 0 && clusterEntryIndex == 0)       // next cluster
        {
            if (_state.flags & EndOfChain)
            {
                _state.flags |= EndOfTable;
                return 0;
            }

            uint8_t eoc;
            _state.currentCluster = fat_nextClusterEntry(boot, partitionOffset, _state.currentCluster, fetch, &eoc);
            if (eoc)
                _state.flags |= EndOfChain;
        }

        uint32_t address =                                          // calculates the address of where the entry is located
            partitionOffset +                                                                   // base offset
            fat_firstSectorOfCluster(boot, _state.currentCluster) * boot->bytesPerSector +      // cluster offset
            sizeof(fat_DirectoryEntry) * clusterEntryIndex;                                     // entry offset
        
        fetch(address, sizeof(fat_DirectoryEntry), entryBuf);       // reads the data
        if (_state.flags & LfnDirectoryEntry)                       // after the long file name
        {   
            ++_state.entryIndex;
            memcpy(entry, entryBuf, sizeof(fat_DirectoryEntry));
            if (fileName != NULL)
                strncpy(fileName, nameBuf, nameLen);

            _state.flags &= ~LfnDirectoryEntry;
            return 1;
        }

        if (dir->fileName[0] == 0)                                  // last entry
        {
            _state.flags |= EndOfTable;
            return 0;
        }
        else if (dir->fileName[0] == 0xE5)                          // deleted entry
            continue;                                               // goto the next

        if (dir->fileAttributes != FAT_FILE_ATTR_LONG_NAME)         // just a short file name (8.3 notation)
        {
            ++_state.entryIndex;
            memcpy(entry, entryBuf, sizeof(fat_DirectoryEntry));

            fat_getFileName(nameBuf, dir);
            if (fileName != NULL)
                strncpy(fileName, nameBuf, nameLen);

            return 1;
        }

        uint8_t blockIndex = (lfn->ordinal & 0x0F) - 1;             // calculates which blocks 
                                                                    // (every lfn is 13 bytes of the file name -> the block)
        if (lfn->ordinal & 0x40)                                    // last block
            nameBuf[(blockIndex + 1) * 13] = 0;                     // string termination

        UCS2ToUTF8(nameBuf + blockIndex * 13, lfn);                 // extracts the filename block and convert it to UTF8 (char)
        if (blockIndex == 0)                                        // after the first block is the usual DirectoryEntry which contains location and file date etc.
            _state.flags |= LfnDirectoryEntry;
    }

    _state.flags |= EndOfTable;
    return 0;
}

uint8_t fat_compareFilename(const fat_DirectoryEntry* entry, const char* input)
{
    assert(entry != NULL);
    assert(input != NULL);

    const char* p_input = input;
    const char* p_cmp = (char*)entry->fileName;

    for (uint8_t i = 0; i < 8; ++i, ++p_input, ++p_cmp)
    {	                                                            // for filename length 
        if (toupper(*p_input) == *p_cmp)							// short file name are all in uppercase
            continue;												// continue if matches

        if (*p_input == '.' && *p_cmp == ' ')						// check if we reached extension part
            break;

        return 0;													// if not -> not same filename
    }

    ++p_input;														// skips the period
    p_cmp = (char*)entry->extension;
    for (uint8_t i = 0; i < 3; ++i, ++p_input, ++p_cmp)				// extension is 3 chars
    {
        if (toupper(*p_input) == *p_cmp)							// again compare
            continue;

        return 0;													// if not matches it's not same filename
    }

    return 1;														// completed the checks :)
}
