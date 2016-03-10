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

void dumpRandomInfo()
{
    auto fatType = fat_getType(&boot);
    auto rootDirectoryAddress = fat_clusterToAddress(&boot, offset, 2);

    if (offset == 0)
        cout << "MBR Signature not found, assuming bootsector @ offset: 0" << endl;
    
    cout << "Dumping boot sector data:" << endl;
    cout << "  BootSector at: 0x" << setw(8) << offset << endl;
    cout << "  FatType: FAT" << ((fatType == FAT12) ? "12" : ((fatType == FAT16) ? "16" : "32")) << endl;
    cout << "  OEM: " << boot.OEM << endl;
    cout << "  Total Clusters: 0x" << setw(8) << fat_countOfClusters(&boot) << endl;
    cout << "  Cluster Size: 0x" << setw(4) << fat_clusterSize(&boot) << endl;
    cout << "  Sectors Per Cluster: 0x" << setw(2) << static_cast<int>(boot.sectorsPerCluster) << endl;
    cout << "  Bytes Per Sector: 0x" << setw(4) << boot.bytesPerSector << endl;
    cout << "  Root Directory: 0x" << setw(8) << rootDirectoryAddress << endl;
}

void dumpRootDir()
{
    cout << "Dumping root directory at: 0x" << setw(8) << fat_clusterToAddress(&boot, offset, 2) << endl;

    fat_DirectoryEntry entry = { 0 };
    char buf[255];

    while (fat_nextDirectoryEntry(&boot, offset, 2, fetch, &entry, buf, 255))
    {
        uint32_t cluster = entry.clusterHigh << 16 | entry.clusterLow;

        if (entry.fileAttributes & FAT_FILE_ATTR_DIRECTORY)
            printf("  [DIR] [%.8s    ] (%.2d:%.2d) %s\n", entry.fileName, cluster, entry.fileSize, buf);
        else
            printf("  [FIL] [%.8s.%.3s] (%.2d:%.2d) %s\n", entry.fileName, entry.extension, cluster, entry.fileSize, buf);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        cout << "Usage: " << "fatdumper [image] [mbr]" << endl;
        cout << endl;
        cout << "image: the file to be dumped" << endl;
        cout << "mbr: enter true if there is a mbr present otherwise enter false" << endl;
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
    
    cout << hex << setfill('0');
    dumpRandomInfo();
    cout << endl;
    dumpRootDir();
    cout << endl;

    return 0;
}