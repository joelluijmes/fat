#include "fat.h"

// All formules without (extensivily) comments is based on Microsofts Specification
// Please read the full document to understand those things
// (Commented functions are made from scratch;)
//
// Microsoft Extensible Firmware Initiative FAT32 File System Specification 
// http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc

static void fat_UCS2ToUTF8(char* filename, const fat_LongFileName* lfn)
{
    assert(filename != NULL);
    assert(lfn != NULL);

    memcpy(filename, lfn->ucs2_1, 0x0A);
    memcpy(filename + 0x05, lfn->ucs2_2, 0x0C);
    memcpy(filename + 0x0B, lfn->ucs2_3, 0x04);
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
    fetchData(0, sizeof(mbr), &mbr);

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

uint8_t fat_nextDirectoryEntry(const fat_BootSector * boot, unsigned cluster, unsigned partitionOffset, fetchData_t fetch, fat_DirectoryEntry* entry)
{
    // TODO: Can't we use entry??
    char entryBuf[sizeof(fat_DirectoryEntry)];  
    char longNameBuffer[0xFF] = { 0 };                                  // buffer for the long file name

    fat_LongFileName* lfn = (fat_LongFileName*)&entryBuf;
    fat_DirectoryEntry* dir = (fat_DirectoryEntry*)&entryBuf;

    static uint8_t endOfCluster = 0, entryIndex = 0;
    if (endOfCluster)
    {

    }

    for (; entryIndex < 32; ++entryIndex)
    {
        uint32_t address = fat_firstSectorOfCluster(boot, cluster) * boot->bytesPerSector + partitionOffset;    // base offset
        address += sizeof(fat_DirectoryEntry) * entryIndex;                                                     // account for the current index

        fetch(address, sizeof(fat_DirectoryEntry), entryBuf);               // reads the data
        if (dir->fileName[0] == 0)                                          // last entry
            return 1;
        else if (dir->fileName[0] == 0xE5)                                  // deleted entry
            return 2;

        if (dir->fileAttributes != FAT_FILE_ATTR_LONG_NAME)                 // just a short file name (8.3 notation)
        {
            ++entryIndex;
            memcpy(entry, entryBuf, sizeof(fat_DirectoryEntry));
            return 0;
        }

        uint8_t blockIndex = (lfn->ordinal & 0x0F) - 1;                     // calculates which blocks 
                                                                            // (every lfn is 13 bytes of the file name -> the block)
        if (lfn->ordinal & 0x40)                                            // last block
            longNameBuffer[(blockIndex + 1) * 13] = 0;                      // string termination

        fat_UCS2ToUTF8(longNameBuffer + blockIndex * 13, lfn);              // extracts the filename block and convert it to UTF8 (char)
        if (blockIndex > 0)                                                 // not the first block
            continue;
            
        // after the first block is the usual DirectoryEntry which contains location and file date etc.
        fetch(address + sizeof(fat_DirectoryEntry), sizeof(fat_DirectoryEntry), entryBuf);
        memcpy(entry, entryBuf, sizeof(fat_DirectoryEntry));
        return 0;
    }
}

uint8_t fat_compareFilename(const fat_DirectoryEntry* entry, const char* input)
{
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
