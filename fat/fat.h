#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#ifdef _MSC_VER
#define PACK( __declaration__ ) __pragma( pack(push, 1) ) __declaration__ __pragma( pack(pop) )
#else
#define PACK( __declaration__ ) __declaration__ __attribute__((__packed__))
#endif

#define FAT_TYPE_NOTHING 0x00
#define FAT_TYPE_12BIT 0x01
#define FAT_TYPE_16BIT 0x04
#define FAT_TYPE_MSDOS 0x05
#define FAT_TYPE_16BIT_EXTENDED 0x06
#define FAT_TYPE_32BIT 0x0B
#define FAT_TYPE_32BIT_LBA 0x0C
#define FAT_TYPE_16BIT_LBA 0x0E
#define FAT_TYPE_MSDOS_LBA 0x0F
#define FAT_SUPPORTED_TYPES (FAT_TYPE_12BIT|FAT_TYPE_16BIT|FAT_TYPE_32BIT|FAT_TYPE_32BIT_LBA|FAT_TYPE_16BIT_LBA)

#define FAT_FILE_ATTR_READONLY 0x01
#define FAT_FILE_ATTR_HIDDEN 0x02
#define FAT_FILE_ATTR_SYSTEM 0x04
#define FAT_FILE_ATTR_VOLUME 0x08
#define FAT_FILE_ATTR_DIRECTORY 0x10
#define FAT_FILE_ATTR_ARCHIVE 0x20
#define FAT_FILE_ATTR_LONG_NAME (FAT_FILE_ATTR_READONLY | FAT_FILE_ATTR_HIDDEN | FAT_FILE_ATTR_SYSTEM | FAT_FILE_ATTR_VOLUME)
#define FAT_FILE_ATTR_LONG_NAME_MASK (FAT_FILE_ATTR_LONG_NAME | FAT_FILE_ATTR_DIRECTORY | FAT_FILE_ATTR_ARCHIVE)

typedef struct fat_PartitionEntry fat_PartitionEntry;
PACK(
struct fat_PartitionEntry
{
	uint8_t state;
	uint8_t chsStart[0x03];
	uint8_t type;
	uint8_t chsEnd[0x03];
	uint32_t startSector;
	uint32_t numberOfSectors;
});


typedef struct fat_MBR fat_MBR;
PACK(
struct fat_MBR
{
	uint8_t bootstrapCode[0x1BE];
	fat_PartitionEntry partitionTable[0x04];
	uint16_t signature;
});

typedef struct fat_BootSector fat_BootSector;
PACK(
struct fat_BootSector
{
	uint8_t jumpBoot[0x03];
	uint8_t OEM[0x08];
	uint16_t bytesPerSector;
	uint8_t sectorsPerCluster;
	uint16_t reservedSectors;
	uint8_t numberOfFATs;
	uint16_t rootEntries;
	uint16_t totalSectors16;
	uint8_t media;
	uint16_t sectorsPerFAT16;
	uint16_t sectorsPerTrack;
	uint16_t numberOfHeads;
	uint32_t hiddenSectors;
	uint32_t totalSectors32;
	uint8_t rest[476];
});

typedef struct fat16_BootSector fat16_BootSector;
PACK(
struct fat16_BootSector
{
	uint8_t driveNumber;
	uint8_t reserved;
	uint8_t bootSignature;
	uint32_t volumeId;
	uint8_t volumeLabel[11];
	uint8_t fileSystemType[8];
	uint8_t bootCode[448];
	uint16_t signature;
});

typedef struct fat32_BootSector fat32_BootSector;
PACK(
struct fat32_BootSector
{
	uint32_t sectorsPerFAT32;
	uint16_t flags;
	uint16_t version;
	uint32_t rootCluster;
	uint16_t fileSystemInformationSector;
	uint16_t backupBootSector;
	uint8_t reserved[0x0C];
	uint8_t logicalDriveNumber;
	uint8_t unused;
	uint8_t bootSignature;
	uint32_t volumeId;
	uint8_t volumeLabel[0x0B];
	uint8_t fatName[0x08];
	uint8_t bootCode[422];
});

typedef struct fat_FileSystemInformationSector fat_FileSystemInformationSector;
PACK(
struct fat_FileSystemInformationSector
{
	uint32_t firstSignature;
	uint8_t unknown1[0x1E0];
	uint32_t fsinfoSignature;
	int32_t freeClusters;
	uint32_t lastAllocatedCluster;
	uint8_t reserved[12];
	uint16_t unknown2;
	uint16_t signature;
});

typedef struct fat_DirectoryEntry fat_DirectoryEntry;
PACK(
struct fat_DirectoryEntry
{
	uint8_t fileName[0x08];
	uint8_t extension[0x03];
	uint8_t fileAttributes;
	uint8_t reserved;
	uint8_t timeCreatedMillis;
	uint16_t timeCreatedHourMinute;
	uint16_t dateCreated;
	uint16_t dateAccessed;
	uint16_t clusterHigh;
	uint16_t time;
	uint16_t word;
	uint16_t clusterLow;
	uint32_t fileSize;
});

typedef struct fat_LongFileName fat_LongFileName;
PACK(
struct fat_LongFileName
{
	uint8_t ordinal;
	uint16_t ucs2_1[0x05];
	uint8_t attribute;
	uint8_t type;
	uint8_t checksum;
	uint16_t ucs2_2[0x06];
	uint16_t cluster;
	uint16_t ucs2_3[0x02];
});

typedef enum FatType FatType;
enum FatType
{
	FAT12,
	FAT16,
	FAT32
};

// Fetches data from the device (i.e. file or hardware driver)
typedef uint8_t(*fetchData_t)(unsigned address, unsigned count, char* out);

// Gets date from fat date format
void fat_getDate(uint16_t date, uint8_t* day, uint8_t* month, uint16_t* year);

// Gets time from fat time format
void fat_getTime(uint16_t time, uint8_t* seconds, uint8_t* minute, uint8_t* hour);

// Gets filename out of a short directory entry, fileName length must be >= 12 
void fat_getFileName(char* fileName, const fat_DirectoryEntry* entry);

// Calculate sectors per fat
uint32_t fat_sectorsPerFat(const fat_BootSector* boot);

// Calculate total size usage of the FATs
uint32_t fat_totalFatSize(const fat_BootSector* boot);

// Calculates the byte address from sector
uint32_t fat_sectorToAddress(const fat_BootSector* boot, unsigned partitionOffset, uint32_t sector);

// Calculates the first sector address of this cluster
uint32_t fat_clusterToAddress(const fat_BootSector* boot, unsigned partitionOffset, uint32_t cluster);

// Gets the amount of sectors used by root directory
uint32_t fat_numberOfRootDirSectors(const fat_BootSector* boot);

// Finds the first data sector
uint32_t fat_firstDataSector(const fat_BootSector* boot);

// Finds the first sector of cluster N
uint32_t fat_firstSectorOfCluster(const fat_BootSector* boot, unsigned cluster);

// Calculates the amount of sectors
uint32_t fat_countOfSectors(const fat_BootSector* boot);

// Calculates the amount of clusters
uint32_t fat_countOfClusters(const fat_BootSector* boot);

uint32_t fat_clusterSize(const fat_BootSector* boot);

uint8_t fat_entriesPerCluster(const fat_BootSector* boot);

// Checks what FAT type bootSector is (FAT12, FAT16, FAT32)
FatType fat_getType(const fat_BootSector* boot);

// Follows the cluster chain, check eoc if End Of Cluster has been reached
uint32_t fat_nextClusterEntry(const fat_BootSector* boot, unsigned partitionOffset, unsigned cluster, fetchData_t fetch, uint8_t* eoc);

// Returns the first directory entry in a cluster
uint8_t fat_firstDirectoryEntry(const fat_BootSector * boot, unsigned partitionOffset, unsigned startCluster, fetchData_t fetch, fat_DirectoryEntry* entry, char* fileName, unsigned nameLen);

// Returns the next directory entry (cluster chaining is built in)
uint8_t fat_nextDirectoryEntry(const fat_BootSector * boot, unsigned partitionOffset, unsigned cluster, fetchData_t fetch, fat_DirectoryEntry* entry, char* fileName, unsigned nameLen);

// Fetches the next partition, returns the partition offset, use eop to check if end of partitions is reached
uint32_t fat_nextPartitionSector(fetchData_t fetchData, fat_BootSector* boot, uint8_t* eop);

// Compares input with the directory entry (short file name)
uint8_t fat_compareFilename(const fat_DirectoryEntry* entry, const char* input);

// Calculates checksum of long file name
uint8_t fat_checksum(uint8_t* name);