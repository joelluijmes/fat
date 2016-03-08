#include "stdafx.h"



const char* path = "C:\\Users\\joell\\Dropbox\\Documenten\\fat.bin";

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
	uint32_t offset = fat_nextSector(fetch, &boot);
	FatType type = fat_getType(&boot);

	return 0;
}