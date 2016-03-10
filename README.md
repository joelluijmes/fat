# FAT

This repository contains an *ANSI C* fat driver which can be found in the fat directory. There are some other demo projects (in C++, it is just dumping random data to the cout) as well:

 - **clusterdumper**: Follows a cluster chain and prints it on the screen
 - **fatdumper**: Prints some bootsector info and the root directory
 - **filedumper**: Dumps the content of a file, from the root directory, on the screen

Please note that all numbers printed are hexadecimal numbers (base 16.) Sometimes the 0x prefix is presented but it can be omitted as well. The usage of the demo projects are very similiar:

```
clusterdumper.exe [image] [mbr] [cluster]

image: the file to be dumped
mbr: enter true if there is a mbr present otherwise enter false
cluster: the starting cluster number
```

```
fatdumper.exe [image] [mbr]

image: the file to be dumped
mbr: enter true if there is a mbr present otherwise enter false
```

```
filedumper.exe [image] [mbr] [cluster] [filename]

image: the file to be dumped
mbr: enter true if there is a mbr present otherwise enter false
filename: filename of a file in the root directory to be dumped
```
