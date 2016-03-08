#include "fat.h"

// All formules in this source file are extracted from: 
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
		: ((fat32_BootSector*)boot->rest)->sectorsPerFAT32;															// finds sector in the fat32 part
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

uint32_t fat_rootDirSectors(const fat_BootSector * boot)
{
	assert(boot != NULL);

	return (boot->rootEntries * 32 + boot->bytesPerSector - 1) / boot->bytesPerSector;
}

uint32_t fat_firstDataSector(const fat_BootSector * boot)
{
	assert(boot != NULL);

	uint32_t sectorsPerFat = fat_sectorsPerFat(boot);
	uint32_t rootDirSectors = fat_rootDirSectors(boot);

	return boot->reservedSectors + (boot->numberOfFATs * sectorsPerFat) + rootDirSectors;
}

uint32_t fat_firstSectorOfCluster(const fat_BootSector * boot, unsigned cluster)
{
	assert(boot != NULL);
	assert(cluster > 2);

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
	uint32_t rootDirSectors = fat_rootDirSectors(boot);
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

uint32_t fat_nextPartitionSector(fetchData_t fetchData, fat_BootSector* boot)
{
	assert(fetchData != NULL);
	assert(boot != NULL);

	fat_MBR mbr;
	fetchData(0, sizeof(mbr), &mbr);

	static unsigned i = 0;
	uint32_t partitionOffset = 0;
	for (; i < 4; ++i)																								// max 4 boot partitions
	{
		fat_PartitionEntry entry;
		fetchData(offsetof(fat_MBR, partitionTable), sizeof(fat_PartitionEntry), (char*)&entry);					// read partition entry

		if ((entry.type | FAT_SUPPORTED_TYPES) == 0)																// check if it is supported
			continue;

		partitionOffset = entry.startSector * 512;																	// calculate the offset (start of this partition)
		break;
	}

	if (partitionOffset > 0)
		fetchData(partitionOffset, sizeof(fat_BootSector), (char*)boot);											// read the actual bootsector
	else if (i == 3)																								// reset it
		i = 0;

	return partitionOffset;																							// return the start of partition
}

uint32_t fat_nextClusterEntry(const fat_BootSector* boot, unsigned partitionOffset, unsigned cluster, fetchData_t fetch, uint8_t* eoc)
{
	assert(boot != NULL);
	assert(fetch != NULL);

	uint32_t sectorsPerFat = fat_sectorsPerFat(boot);
	FatType type = fat_getType(boot);

	assert(	(type == FAT12 && cluster < 0x0FF8) ||
			(type == FAT16 && cluster < 0xFFF8) ||
			(type == FAT32 && cluster < 0x0FFFFFF8) );
	
	unsigned fatOffset = (type == FAT12)
		? cluster + (cluster / 2)
		: (type == FAT16)
			? cluster * 2
			: cluster * 4;

	uint32_t thisFatSector = boot->reservedSectors + (fatOffset / boot->bytesPerSector);
	uint32_t thisFatEntry = fatOffset % boot->bytesPerSector;

	char* secBuf = malloc(boot->bytesPerSector);
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

uint8_t fat_compareFilename(const fat_DirectoryEntry* entry, const char* input)
{	
	const char* p_input = input;
	const char* p_cmp = (char*)entry->fileName;

	for (uint8_t i = 0; i < 8; ++i, ++p_input, ++p_cmp)
	{	// for filename length 
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

//static uint8_t readChain(uint16_t chain[], uint8_t len, uint16_t start)
//{
//	uint16_t fat[CHAIN_LEN];										// because an avr doesn't have much sram
//	uint16_t fatOffset = start;										// we read the chain buffered (thus reading only a part)
//
//	uint16_t next = start;
//
//	chain[0] = next;												// first is the start cluster
//	for (uint8_t i = 1; i < len; ++i)
//	{
//		if (next > fatOffset + CHAIN_LEN || next == start)			// if requested cluster exceeds the buffered fat
//		{															// read that part
//			fatOffset = next;
//
//			// offsets are byte based but the because the FAT is 16 bit we multiply it by 4
//			sd_read_address(fatOffset * 4 + fatPosition, sizeof(uint16_t) * CHAIN_LEN, (uint8_t*)&fat);
//		}
//
//		next = fat[next - fatOffset];								// read next
//
//		if (next >= 0x0FFFFFFF)										// EOC (End of Chain)
//			return i;												// return chain length
//
//		chain[i] = next;
//	}
//
//	return 1;
//}