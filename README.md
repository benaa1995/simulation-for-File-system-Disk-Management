# simulation for File system Disk Management
Program Description:
This Program is a simulation for File system Disk Management using "Indexed-allocation" system.
The disk is simulated by a file of size is 256 Bytes.

The Files (FsFile) on the file system have NO "direct blocks" and 1 "single Indirect" block 
[which can contain up to block_size "references"=blocks sequential numbers that represents data blocks


Commands number and their description (+input):
0 - Delete fsDisk with all its data, and exit the program.
1 - Print disk content and files list.
2 - Format the disk, requires an argument of block size.
3 - Create new file(File Name) if there is enough space. Files names are unique.
4 - Open file(File Name). File must be created ahead and should be closed before calling Open.
5 - Close file(file descriptor number). - Closing an opened file referenced by a file descriptor
6 - Write new data to file (file descriptor number,text)
7 - Read n chars from file (file descriptor number,n=number of chars to read).
8 - Delete file from disk(File Name)

Output to the user:
0: -
1: All opened File Descriptors and all the data on the disk.
2: Number of blocks on the foramatted disk.
3: The name of the file you created and its file descriptor number.
4: The name of the file you opened and its file descriptor number.
5: The name of the file you closed and its file descriptor number.
6: -
7: File_Name.data[0:n]. First n bytes of given file name
8: The name of the file you deleted and its file descriptor.

Compile & Run on VScode
Compilation: Ctrl+Shift+B .
Run: Ctrl+F5     

or use g++ / Makefile provided

Attached filed:
FinalExam.cpp
DISK_SIM_FILE.txt - Simulation Disk file.
MakeFile, 
test.txt - given test case input commands

in the program, there is a use in 3 classes:
1. fsFile - represents a file. contains "location" (block number) of its index block on the disk.
2. FileDescriptor - represents a file that was opened during the run.
3. fsDisk - represents the disk manager in the file system.
	         All disk operations are implemented in this class.


Basic example flow of using the program:
First, format the disk using the command "2", then choose block size.
Open a new file, by use command "3", and give it a name. 
Write data into the open file, using command "6" and by FD (the file Descriptor). 
Close (5), open (4) or delete (8) file by the required command. 



The real Linux file system implementation is much more complex ,robust and efficient
https://elixir.bootlin.com/linux/latest/source/include/linux/file.h
https://elixir.bootlin.com/linux/latest/source/fs/file.c
https://elixir.bootlin.com/linux/latest/source/fs/file_table.c
