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

uint32_t fat_nextSector(fetchData_t fetchData, fat_BootSector* boot)
{
	assert(fetchData != NULL);
	assert(boot != NULL);

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

//uint32_t fat_clusterInFatEntry(const fat_BootSector* boot, unsigned cluster, fetchData_t fetch)
//{
//	assert(boot != NULL);
//	assert(fetch != NULL);
//
//	uint32_t sectorsPerFat = fat_sectorsPerFat(boot);
//	FatType type = fat_getType(boot);
//	
//	unsigned fatOffset = (type == FAT12)
//		? cluster + (cluster / 2)
//		: (type == FAT16)
//			? cluster * 2
//			: cluster * 4;
//
//	uint32_t thisFatSector = boot->reservedSectors + (fatOffset / boot->bytesPerSector);
//	uint32_t thisFatEntry = fatOffset % boot->bytesPerSector;
//
//	char* secBuf = malloc(boot->bytesPerSector);
//	assert(secBuf != NULL);
//
//	fetch(thisFatSector, boot->bytesPerSector, &secBuf);
//
//	
//
//	free(secBuf);
//}
