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

bool compareCaseInsensitive(const string& a, const string& b)
{
    unsigned int sz = a.size();
    if (b.size() != sz)
        return false;
    for (unsigned int i = 0; i < sz; ++i)
        if (tolower(a[i]) != tolower(b[i]))
            return false;
    return true;
}

uint8_t findFile(const string& file, fat_DirectoryEntry* entry)
{
    char buf[255];
    while (fat_nextDirectoryEntry(&boot, offset, 2, fetch, entry, buf, 255))
    {
        if (compareCaseInsensitive(buf, file))
            return 1;
    }

    entry = NULL;
    return 0;
}

ostream& asHex(std::ostream& os, uint8_t i)
{
    char table[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    char low = table[i & 0x0F];
    char high = table[i >> 4];

    os << low << high;
    return os;
}

void dumpFile(const fat_DirectoryEntry* entry)
{
    uint32_t cluster = entry->clusterHigh << 16 | entry->clusterLow;
    uint32_t fileSize = entry->fileSize;

    cout << "Dumping contents of cluster: 0x" << hex << setfill('0') << setw(4) << cluster << endl;
    cout << "FileSize: 0x" << setw(8) << fileSize << endl;

    if (fileSize == 0)
        return;

    uint32_t clusterSize = fat_clusterSize(&boot), currentCluster = cluster, x = 0;
    char* buf = new char[clusterSize];
    uint8_t eoc, addressPrinted = 0;

    do
    {
        uint32_t sector = fat_firstSectorOfCluster(&boot, currentCluster) + fat_firstDataSector(&boot);
        uint32_t address = fat_sectorToAddress(&boot, offset, sector);
        if (!addressPrinted)
        {
            cout << "Address: 0x" << setw(8) << address << endl << endl;
            addressPrinted = 1;
        }
        if (!fetch(address, clusterSize, buf))
        {
            cout << "Error reading data from fetch." << endl;
            break;
        }

        for (unsigned i = 0; i < clusterSize; ++i, ++x)
        {
            if (x >= fileSize)
                break;

            asHex(cout, buf[i]);
            cout << " ";
        }

        currentCluster = fat_nextClusterEntry(&boot, offset, currentCluster, fetch, &eoc);
    } while (!eoc);

    delete[] buf;

    cout << endl << endl;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cout << "Usage: " << "filedumper [image] [mbr] [filename]" << endl;
        cout << endl;
        cout << "image: the file to be dumped" << endl;
        cout << "mbr: enter true if there is a mbr present otherwise enter false" << endl;
        cout << "filename: filename of a file in the root directory to be dumped" << endl;
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

    string filename(argv[3]);
    fat_DirectoryEntry entry;
    if (!findFile(filename, &entry))
    {
        cout << "File not found in root directory (don't forget the extension.) Check with fatdumper what is in it." << endl;
        return -1;
    }


    dumpFile(&entry);
    return 0;
}