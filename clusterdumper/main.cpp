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

    return file.good();
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

    uint32_t sectorsPerFat = fat_sectorsPerFat(&boot);
    unsigned fatOffset = (type == FAT12)
        ? cluster + (cluster / 2)
        : (type == FAT16)
            ? cluster * 2
            : cluster * 4;

    uint32_t thisFatSector = boot.reservedSectors + (fatOffset / boot.bytesPerSector);
    uint32_t thisFatEntry = fatOffset % boot.bytesPerSector;
    uint32_t address = fat_sectorToAddress(&boot, offset, thisFatSector);

    cout << hex << setfill('0');
    cout << "Dumping cluster chain at: 0x" << setw(width) << address << endl;
    cout << "Base of chain at: 0x" << setw(width) << fat_clusterToAddress(&boot, offset, 2) << endl << endl;

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
        cout << endl;
        cout << "image: the file to be dumped" << endl;
        cout << "mbr: enter true if there is a mbr present otherwise enter false" << endl;
        cout << "cluster: the starting cluster number" << endl;
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