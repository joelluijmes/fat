#include "stdafx.h"

const char* path = "C:\\Users\\joell\\Dropbox\\Documenten\\fat.bin";

using namespace std;

int main(int argc, char* argv[])
{
	unsigned f32 = sizeof(fat32_BootSector);
	unsigned f16 = sizeof(fat16_BootSector);

	fstream file(path, ios_base::in | ios_base::binary);

	fat_MBR mbr;
	fat32_BootSector bootSector;

	file.get((char*)&mbr, sizeof(fat_MBR));

	for (unsigned i = 0; i < 4; ++i)
	{
		uint32_t offset = 512 * mbr.partitionTable[i].startSector;
		file.seekg(offset);
		file.get((char*)&bootSector, sizeof(fat32_BootSector));

		//uint32_t rootDirSectors = FAT_ROOTDIR_SECTORS(&bootSector);

		i = i;
	}


	return 0;
}