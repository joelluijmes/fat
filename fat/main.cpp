#include "stdafx.h"

const char* path = "C:\\Users\\joell\\Dropbox\\Documenten\\fat12.img";

using namespace std;

fstream file(path, ios_base::in | ios_base::binary);

uint8_t fetch(unsigned address, unsigned count, char* out)
{
	file.seekg(address);
	file.get(out, count);

	return 0;
}

int main(int argc, char* argv[])
{
	fat_BootSector boot;
    unsigned i = sizeof(fat_BootSector);
    //uint32_t offset = fat_nextPartitionSector(fetch, &boot, nullptr);
	fetch(0, sizeof(boot), (char*)&boot);
	FatType type = fat_getType(&boot);

    uint32_t first = fat_firstDataSector(&boot);
    uint32_t rootDirSectors = fat_numberOfRootDirSectors(&boot);
    uint32_t firstSectorOfCluster = fat_firstSectorOfCluster(&boot, 2);

    uint32_t firstRoot = boot.reservedSectors + (boot.numberOfFATs * boot.sectorsPerFAT16);

    fat_DirectoryEntry entry = { 0 };
    fat_nextDirectoryEntry(&boot, 2, 0, fetch, &entry);

    char* buf = new char[boot.bytesPerSector];
    fat_nextClusterEntry(&boot, 0, 2, fetch, nullptr);
    uint32_t address = firstSectorOfCluster * boot.bytesPerSector + 0;
    fetch(address, boot.bytesPerSector, buf);

    

	cout << "Cluster Chain: ";

	uint8_t eoc = 0;
	uint32_t next = 2;
	while (!eoc)
	{
		cout << next << ", ";

		next = fat_nextClusterEntry(&boot, 0, next, fetch, &eoc);
	}

	cout << endl;
    delete[] buf;
	return 0;
}