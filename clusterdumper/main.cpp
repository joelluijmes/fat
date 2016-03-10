#include "stdafx.h"

using namespace std;

fstream file;
fat_BootSector boot;
fat_MBR mbr;

uint32_t offset = 0;

uint8_t fetch(unsigned address, unsigned count, char* out)
{
    file.seekg(address);
    file.read(out, count);

    return 0;
}

uint8_t findFile(const string& file, fat_DirectoryEntry* entry)
{
    char buf[255];
    while (fat_nextDirectoryEntry(&boot, offset, 2, fetch, entry, buf, 255))
    {
        if (file.compare(buf) == 0)
            return 1;
    }

    entry = NULL;
    return 0;
}

void printChain(unsigned cluster)
{
    FatType type = fat_getType(&boot);
    unsigned width = ((type == FAT32) ? 7 : ((type == FAT16) ? 4 : 3));

    cout << "Dumping cluster chain, starting: 0x" << hex << setfill('0') << setw(width) << cluster << endl;

    uint8_t eoc;

    do
    {
        cout << setw(width) << cluster << " ";
        cluster = fat_nextClusterEntry(&boot, offset, cluster, fetch, &eoc);
    } while (!eoc);

    cout << cluster;
    cout << endl << endl;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cout << "Usage: " << "fatdumper [image] [mbr] [startcluster]" << endl;
        return -1;
    }

    file.open(argv[1], ios_base::in | ios_base::binary);
    if (!file.is_open())
    {
        cout << "Couldn't open file? Check the path." << endl;
        return -1;
    }

    bool mbr;
    istringstream(argv[2]) >> boolalpha >> mbr;

    if (mbr)
    {
        fetch(0, sizeof(fat_MBR), (char*)&mbr);
        offset = fat_nextPartitionSector(fetch, &boot, nullptr);
    }
    else
    {
        fetch(0, sizeof(fat_BootSector), (char*)&boot);
    }

    unsigned cluster;
    istringstream(argv[3]) >> cluster;

    printChain(cluster);
    return 0;
}