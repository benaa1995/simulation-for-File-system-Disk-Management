#include <assert.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <vector>
#include <map>

//Linux specific headers:
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std; // Not recommended but ok for small project

// Disk will be actually a file (a directory) of size 256 Bytes
// with no sub-directories.
// Indexed allocation file system
#define DISK_SIZE 256

//The file must be created ahead of time (before running the code)
#define DISK_SIM_FILE "DISK_SIM_FILE.txt"


//Supplied function
//convert from decimal byte -> to binary 8bits(byte)
//changed return type to void...
void decToBinary(int n, char &c)
{
    // array to store binary number (as 0/1 bits, int - a little wasteful)
    int binaryNum[8];

    // counter for binary array
    // (form the bits array)
    int i = 0;
    while (n > 0)
    {
        // storing remainder in binary array
        binaryNum[i] = n % 2;
        n = n / 2;
        i++;
    }

    // printing binary array in reverse order
    for (int j = i - 1; j >= 0; j--)
    {
        if (binaryNum[j] == 1)
            c = c | 1u << j;
    }
}

// ============================================================================
class FsFile
{
    int file_size;    // How many bytes actually have user data in it
    int block_in_use; // how many blocks does this file uses
    int index_block;  // (block) index of the block With the file's Blocks Numbers

    //////////////////
    // These should actually be defined const
    // We will need to set them in the ctor initiallization list.
    const int block_size; // this is common for all files on the disk,
                          // but can change among files on different disks
    const int max_file_size;
    const int max_blocks;
    /////////////////

public:
    // Note that _block_size must be <= DISK_SIZE
    //Use initialization list to set the const fields
    FsFile(int _block_size) : block_size(_block_size),
                              max_file_size(_block_size * _block_size),
                              max_blocks(_block_size)
    {
        file_size = 0;    // index block is not part of the file (and file size)
        block_in_use = 0; // MAX_BLOCKS_PER_FILE = block_size
        index_block = -1; // block number on disk which contains the "pointers"
                          // = blocks numbers that contain the file data blocks
    }

    int getfile_size() { return file_size; }
    /////////////////////////////
    void IncreaseFileSize(int bytes) { file_size += bytes; }
    void IncreaseBlocksInUse(int blocks) { block_in_use += blocks; }

    // TODO: currently we set the index block only after the first write
    //     to the file. which is done from another class (public method),
    //          (no encapsulation)
    // This should actually be done in one of two other ways:
    // 1.create a pre-defined number of index blocks after disk format
    //   at the beginning of the disk
    // 2. allocate the index block when the FsFile is created (on file creation)
    void setIndexBlock(int block_num) { index_block = block_num; }
    int getIndexBlock() { return index_block; }

    int getNumOfBlocksInUse() { return block_in_use; }
    const int GetMaxFileSize() { return max_file_size; }
    const int getMaxFileBlocks() { return max_blocks; }
    /////////////////////////
};

// ============================================================================
/* ----------------------------------- */
/* ---- DO NOT CHANGE THIS CLASS! (except for setters and getters?)---- */
/* ----------------------------------- */
class FileDescriptor
{
    string file_name; // Unique for each file (assumption given to us)
    FsFile *fs_file;  // Each FD is associated with one FsFile at any given time.
                      // TODO: In this implementation it actually holds true(no reuse of FD's)
    bool inUse;       // is FD open? (bad name for variable...)

public:
    FileDescriptor(string FileName, FsFile *fsFile)
    {
        file_name = FileName;
        fs_file = fsFile;
        inUse = true; // A FD creation is caused only by creating a new file,
                      // In which the FileDescriptor is immidiately opened
                      // TODO: is this a correct assumption? (not after opening the filename?)
    }

    string getFileName() { return file_name; }

    ///////////////////////////////////
    bool isInUse() { return inUse; }
    void setInUse(bool _inUse) { inUse = _inUse; }
    void setFileName(string name) { file_name = name; }
    FsFile *getFsFile() { return fs_file; }
    ////////////////////////////
};

// ============================================================================

class fsDisk
{

    FILE *sim_disk_fd; // the simulated disk is actually a unix FILE
    bool is_formated;  // was fsFormat() called yet ?

    int BitVectorSize; //num of elements in array BitVector

    // BitVector -
    //   "bit" (int) vector, indicate which blocks in the disk is free
    //    or not.  (i.e. if BitVector[0] == 1 , means that the
    //    first block is occupied.
    //    Array size depends on BitVectorSize (known at runtime -> dynamic memory)
    int *BitVector;

    // Maps a filename to a FsFile (hash map).
    // [all files on disk - open + not open]
    map<string, FsFile *> MainDir;

    //   When you open a file, the operating system creates an entry to represent that file
    //   This entry number is the file descriptor.
    // The following array will hold the opened files only
    // This datastructure will help us know which fd's are opened/closed
    // and in case of opening a new file it would help us pick a new FD number
    //TODO: Are FileDescriptors not deleted until format/disk destruction?
    vector<FileDescriptor> OpenFileDescriptors;

    int block_size; // can change during lifetime of the Disk object
    int maxSize;    // max size of disk?
    int freeBlocks; // number of available blocks

    ////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////
    // Private helper functions //
    //////////////////////////////
    //Calculate free Space within allocated Blocks.
    int freeSpaceWithinAllocated(FsFile *fsf)
    {
        return (fsf->getNumOfBlocksInUse() * block_size) - fsf->getfile_size();
    }

    //Find an unused block and allocate it.
    // Return it's index
    int allocateBlock()
    {
        for (int blockIndex = 0; blockIndex < BitVectorSize; blockIndex++)
        {
            //if block is free
            if (BitVector[blockIndex] == 0)
            {
                BitVector[blockIndex] = 1; // allocate it
                freeBlocks--;              // update num of free blocks
                return blockIndex;         //return selected block index
            }
        }
        return -1; //Failed
    }

    //Read From Disk
    size_t readDisk(long int offset, size_t elem_size, size_t nmemb, char *buf)
    {
        fseek(sim_disk_fd, offset, SEEK_SET);
        return fread(buf, elem_size, nmemb, sim_disk_fd);
    }

    //Write To Disk.
    size_t writeDisk(long int offset, char *buf, size_t elem_size, size_t nmemb)
    {
        fseek(sim_disk_fd, offset, SEEK_SET);
        return fwrite(buf, elem_size, nmemb, sim_disk_fd);
    }
    /////////////////////////////////////////////////////////////////////////////////////////

public:
    // ------------------------------------------------------------------------
    fsDisk()
    {
        // open the (disk_fs) file for read and write
        // TODO: check if file must exist preior to simulation!!
        sim_disk_fd = fopen(DISK_SIM_FILE, "r+"); //open for read and write

        assert(sim_disk_fd); // make sure file was opened succesfully
                             //usualy we remove assert in production code. (use exit() instead)

        // fill the disk_fs FILE content with null bytes (naive method)
        //  could have use fseek once because each time we write- the fseek pointer is advanced
        for (int i = 0; i < DISK_SIZE; i++)
        {

            // sets the file position of the stream to an offset of i bytes from the start.
            //returns zero on success (retval is ignored here)
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);

            // write one null byte at current offset
            ret_val = fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
        }

        //TODO: For some reason - this didn't work well
        //getting a fsFormat: Can not initialize blocks of size > bytes).Format ERROR:
        //                       block size must be positive
        // int ret_val = fseek(sim_disk_fd, 0, SEEK_SET);
        // ret_val = fwrite("\0", 1, DISK_SIZE, sim_disk_fd);
        // assert(ret_val == DISK_SIZE); // make sure one byte was written successfully

        fflush(sim_disk_fd); // don't delay/postpone the writes to the FILE.
                             // flush the write buffer so we are sure it was written at this point

        block_size = 0; // invalid value

        is_formated = false; //blockSize is unknown at this stage....
    }

    ////////////////////////////////////////////////
    //Dtor
    ~fsDisk()
    {

        for (auto it = MainDir.begin(); it != MainDir.end(); it++)
            delete it->second; // call dtor of associated FsFile

        MainDir.clear();             //clear map content (MainDir records)
        OpenFileDescriptors.clear(); // no files exist anymore.
        delete[] BitVector;          // de-allocate BitVector of occupied blocks
        fclose(sim_disk_fd);         // Don't leave open unix files on the system when simulation is done
    }
    ///////////////////////////////////////////////

    // ------------------------------------------------------------------------
    /* ---- This implementation is given to us except for line 2 ---- */
    void listAll()
    {
        int i = 0;

        //for (.... ALL FILE DESCRIPTORS .... ) {
        for (auto it = begin(OpenFileDescriptors); it != end(OpenFileDescriptors); ++it)
        {
            cout << "index: " << i << ": FileName: " << it->getFileName() << " , isInUse: " << it->isInUse() << endl;
            i++;
        }

        char bufy;

        cout << "Disk content: '";
        // Now disk content will be printed in a sequential manner (file contents will be mixed/ unordered)

        //Each byte in the simulated disk (file) holds a file descriptor of an opened file
        for (i = 0; i < DISK_SIZE; i++)
        {

            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);

            //read one byte (char) each time from the disk and print it
            ret_val = fread(&bufy, 1, 1, sim_disk_fd);
            cout << bufy;
        }

        cout << "'" << endl;
    }

    // ------------------------------------------------------------------------
    //Signature was fixed/updated (was previously for I-node)
    void fsFormat(int blockSize)
    {
        //TODO: need to check if disk is already formatted?

        if (blockSize > DISK_SIZE)
        {
            cerr << __FUNCTION__ << ": "
                 << "Can not initialize blocks of size >" << DISK_SIZE << " bytes).";
            return;
        }

        if (blockSize <= 0)
        {
            cerr << "Format ERROR: block size must be positive" << endl;
            return;
        }

        is_formated = true;
        block_size = blockSize;
        BitVectorSize = DISK_SIZE / block_size; // how many blocks can fit inisde the disk? (integer division)
        freeBlocks = BitVectorSize;

        //Init BitVector.
        BitVector = new int[BitVectorSize];
        assert(BitVector);
        for (int i = 0; i < BitVectorSize; i++)
            BitVector[i] = 0; // Mark all blocks as available.

        cout << "Format Disk: number of blocks: " << BitVectorSize << endl;
    }

    // ------------------------------------------------------------------------
    /* Responsible for creating a FsFile object (and opening it the first time)
         + Updating MainDir & OpenFileDescriptors
       It would also check if the Disk has been formatted yet
       Returns: 
            If the disk was formatted: the new file File descriptor (its index in the OpenFileDescriptor vector)
            else: -1
    */
    int CreateFile(string fileName)
    {

        if (!is_formated)
        {
            cerr << "Disk Hasn't Been Formatted Yet." << endl;
            return -1;
        }

        //Check if File name already exist in the MainDir.
        if (MainDir.find(fileName) != MainDir.end())
        {
            cout << "File name already exist." << endl;
            return -1;
        }

        // Not Enough Blocks To Allocate from.
        if (freeBlocks <= 0)
        {
            cout << "No availble blocks." << endl;
            return -1;
        }

        //Create FsFile
        FsFile *fsf = new FsFile(block_size);
        assert(fsf != nullptr);

        //Create a temporary local file Descriptor ()
        // This is the equivalent of opening the file the first time
        // (only one fd for the each fileName can be allocated. unlike in Linux)
        FileDescriptor fd(fileName, fsf);

        //Update MainDir - Create and add a new Pair<string, FsFile*> to the MainDir Map
        MainDir.insert(make_pair(fileName, fsf));

        //Update the Open FD "Table" - push it at the end of the Vector.
        OpenFileDescriptors.push_back(fd);

        // The new file descriptor was added at the end of the vector so we can calculate the index
        int new_fd_idx = OpenFileDescriptors.size() - 1; 
        
        //TODO : check if this is a good enough assumption/ correct logic
        // Because otherwise- we need to look for the 
        // first "empty" (available) file-descriptor in a CONSTANT size array of file-descriptor numbers
        // Why constant size? (i.e 1024 like in Linux) because otherwise we won't be able to 
        // do lookups(searches) in the array in O(1) by an index + being able
        // to easily manage the allocation of file descriptor numbers.

        // While in the Vector case ("openFileDescriptors") each time we close a file
        // (actually only the file descriptor we god from open() should have been closed) or delete a file
        // we would have need to DELETE an entry from the vector (so the indexes of other elements is now corrupted)
        // Therefore- Without a constant size array - There will be no meaning to the "file descriptor number" we return
        // on open (because it might become incorrect and irrelevant)

        //To Conclude - If we want the openFileDescriptor vectors to hold only "open File descriptors"
        //Then:     1.There will be no meaning to the field inUse/isOpen 
        //          (because FD object either exist and in "open" state or doesn't exist)
        //          2. We will still need a constant size array of size MAX_FILE_DESCRIPTORS
        //              to mark the used file descriptor numbers

        return new_fd_idx;
    }

    // ------------------------------------------------------------------------
    // Needs to verify that file exist and wasn't already opened!
    // Returns
    //      On success: File's FD
    //      Failure: -1
    int OpenFile(string fileName)
    {
        //TODO: code duplication
        if (!is_formated)
        {
            cout << "Disk Hasn't Been Formatted Yet." << endl;
            return -1;
        }

        //Find The File Descriptor And Check If Not Open.
        for (auto it = begin(OpenFileDescriptors); it != end(OpenFileDescriptors); ++it)
        {
            if (fileName == it->getFileName())
            {
                if (it->isInUse() == false)
                {
                    it->setInUse(true);
                    //The index of the FileDescriptor inside the vector is the FD number.
                    return distance(OpenFileDescriptors.begin(), it);
                }
                cout << fileName << " file is already open." << endl;
                break; 
            }
        }
        return -1; // failure (name not found or already opened)
    }

    // ------------------------------------------------------------------------
    // Needs to verify that file exist and is opened first
    // returns -1 on failure
    // returns the file name on success
    string CloseFile(int fd)
    {
        if (!is_formated)
        {
            cout << "Disk hasn't been formatted Yet." << endl;
            return "-1";
        }

        //TODO: depends on correctness of implementation (assumptions)
        // Check if no such FD in our open FD's vector.
        if (fd >= OpenFileDescriptors.size())
        {
            cout << "File doesn't exist" << endl;
            return "-1";
        }

        //File Is Open- Close It.
        if (OpenFileDescriptors[fd].isInUse() == true)
            OpenFileDescriptors[fd].setInUse(false);
        else
        {
            cout << "File is already closed." << endl;
            return "-1";
        }

        return OpenFileDescriptors[fd].getFileName();
    }

    // ------------------------------------------------------------------------
    // Should check that there is enough space in disk and in file
    // that the file is open and the disk is formatted
    // either reuse allocated unused blocks of this file
    // or allocate new blocks to it
    // -1 on failure
    int WriteToFile(int fd, char *buf, int len)
    {

        //If Disk Has Not Been Formatted OR FD Not Exist OR File is Close.
        if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return -1;
        }

        //FD doens't exist?
        if (fd >= OpenFileDescriptors.size())
        {
            cout << "No such file descriptor" << endl;
            return -1;
        }

        //If file isn't open
        if (!OpenFileDescriptors[fd].isInUse())
        {
            cout << OpenFileDescriptors[fd].getFileName() << ": Write failed - Matching file is closed!" << endl;
            return -1;
        }

        FsFile *fsf = OpenFileDescriptors[fd].getFsFile();

        // Check If There More Place To Insert The Data Into the File.
        // This Test Cover The Case Of FILE Itself Have Not Enough More Blocks To Allocate
        if (len > (fsf->GetMaxFileSize() - fsf->getfile_size()))
        {
            cout << "Not Enough free space in this file." << endl;
            return -1;
        }

        //Check how much space can be reused from the Allocated Blocks.
        int last_block_free_space = freeSpaceWithinAllocated(fsf);
        int bytes_to_allocate = len - last_block_free_space; //num of bytes that needs to be allocated
        int blocks_to_allocate;

        if (bytes_to_allocate > 0)
        {
            //Calculate How Many Blocks Requierds. (could use Math.ceil(A/B))
            blocks_to_allocate = bytes_to_allocate / block_size;
            if ((bytes_to_allocate % block_size) != 0)
                blocks_to_allocate++;

            //Check If There Enough Blocks On The Disk To Be Allocate
            // and against the limit of max num of blocks per FsFile
            if ((blocks_to_allocate > this->freeBlocks) ||
                (blocks_to_allocate > fsf->getMaxFileBlocks() - fsf->getNumOfBlocksInUse()))
            {
                cout << "No enough free space on disk." << endl;
                return -1;
            }

            //check if index block hasn't been allocated yet (is this the first write?)
            if (fsf->getIndexBlock() == -1)
                fsf->setIndexBlock(allocateBlock());
        }

        int write_to_last_blk_len = last_block_free_space;

        if (last_block_free_space != 0)
        {
            if (len < last_block_free_space)
                write_to_last_blk_len = len;

            // Single Indirect Block.
            //Calculate The sequence num of the Block At The Block Of Single Indirect.
            int last_block_seqNum = fsf->getNumOfBlocksInUse();

            //Get the char that Represent the Block Number.
            char c;
            readDisk((fsf->getIndexBlock() * block_size) + last_block_seqNum - 1, 1, 1, &c);
            int file_last_block_number = (int)c;

            //Write Into The Disk
            writeDisk(file_last_block_number * block_size + (block_size - last_block_free_space),
                      buf, sizeof(char), write_to_last_blk_len);

            fsf->IncreaseFileSize(write_to_last_blk_len); //increase #1
        }

        // Now handle the blocks we need to allocate

        // Check If All The User Chars Has Been Inserted Into The Free Space Those Already Allocated.
        if (bytes_to_allocate <= 0)
        {
            return len; // Done. //TODO: is this the right API?
        }

        //Fill the new blocks.
        for (int i = 0; i < blocks_to_allocate; i++)
        {
            int new_block_idx = allocateBlock();
            //Convert Allocated Block Number To Char
            char c = '\0';
            decToBinary(new_block_idx, c);

            int nextBlockPtrDiskOffset = (fsf->getIndexBlock() * block_size) + fsf->getNumOfBlocksInUse();
            writeDisk(nextBlockPtrDiskOffset, &c, 1, 1); // update the index block content

            for (int j = 0; j < block_size && bytes_to_allocate; j++)
            {
                writeDisk((new_block_idx * block_size) + j,
                          buf + last_block_free_space + (i * block_size) + j,
                          1, 1);

                bytes_to_allocate--;
            }

            fsf->IncreaseBlocksInUse(1);
        }

        fsf->IncreaseFileSize(len - write_to_last_blk_len); //increase #2 (add bytes within allocated blocks only)

        return len;
    }

    // ------------------------------------------------------------------------
    //Delete all data related to a given file name
    // and delete it's FILE from the disk meta-data
    // Returns: it's file descriptor # ?
    //          -1 on Failure
    //TODO : can we delete an open file? (should the function close it?)
    int DelFile(string FileName)
    {
        if (!is_formated)
        {
            cout << "Disk isn't formatted." << endl;
            return -1;
        }

        //Find The File By Name.
        auto fd = OpenFileDescriptors.begin();
        while (fd != OpenFileDescriptors.end())
        {
            if (FileName == fd->getFileName())
                break;
            fd++;
        }

        if (fd == OpenFileDescriptors.end())
        {
            cout << "File not Found!." << endl;
            return -1;
        }

        // TODO: If The File Is Open - Delete is not allowed?
        if (fd->isInUse())
        {
            cout << "File is open. Close it first." << endl;
            return -1;
        }

        int fdNum = distance(OpenFileDescriptors.begin(), fd);

        int file_size = fd->getFsFile()->getfile_size();

        char c = '\0', zero = '\0'; //or use nullptr ?
        bool flag = false;

        int index_block_idx = fd->getFsFile()->getIndexBlock();
        int nextBlockPtrDiskOffset = 0;

        int nextBlockIdx = 0;

        //Delete Data from blocks
        for (long i = 0, offsetWithinBlock = 0, currBlockSeqNum = 0; i < file_size; i++)
        {
            // One Single InDirect block only.
            nextBlockPtrDiskOffset = (index_block_idx * block_size) + currBlockSeqNum;
            readDisk(nextBlockPtrDiskOffset, 1, 1, &c);
            nextBlockIdx = (int)c;
            writeDisk(nextBlockIdx * block_size + offsetWithinBlock, &zero, 1, 1);
            offsetWithinBlock++; //we just wrote one byte...

            if (offsetWithinBlock == block_size)
            {
                //Delete InDirect Block.
                writeDisk(nextBlockPtrDiskOffset, &zero, 1, 1); //set pointer offset value to null char
                BitVector[nextBlockIdx] = 0;                    //mark block as unused
                offsetWithinBlock = 0;
                currBlockSeqNum++;
            }
        }

        //Only now we can delete the index_block
        BitVector[index_block_idx] = 0;

        //Delete Name From Vector.
        fd->setFileName("\0"); //TODO: this is under the assumption we
                               //    only delete files,not file descriptors.

        // Update num of free blocks in disk
        freeBlocks += fd->getFsFile()->getNumOfBlocksInUse();

        fd->setInUse(false); //TODO: It was set to false anyway when closing the file before calling delete...

        //Delete From MainDir (Map).
        auto dirEntry = MainDir.find(FileName);
        delete dirEntry->second; //destroy the FsFile object
        MainDir.erase(dirEntry); //delete the entry from the map
                                 //TODO: what about deleting/removing the
                                 // relevant FileDescriptor from the OpenFileDescriptors?
                                 // (Assumed for simplicity that we don't delete it from there)

        return fdNum;
    }

    // ------------------------------------------------------------------------
    // returns amount of data requested / read?
    int ReadFromFile(int fd, char *buf, int len)
    {

        if (len <= 0)
        {
            cout << "ReadFromFile: len must be positive!" << endl;
            return -1;
        }
        if (!buf)
        {
            cout << "buffer isn't writeable." << endl;
            return -1;
        }

        if (!is_formated)
        {
            cout << "Disk isn't formatted" << endl;
            return -1;
        }

        if (fd > OpenFileDescriptors.size())
        {
            cout << "No such file on disk" << endl;
            return -1;
        }

        if (OpenFileDescriptors[fd].isInUse() == false)
        {
            cout << "File is not open" << endl;
            return -1;
        }

        FsFile *fsf = OpenFileDescriptors[fd].getFsFile();

        int index_block = fsf->getIndexBlock();

        if (index_block == -1)
        {
            cout << "File is empty, nothing was written yet (no index block)" << endl;
            *buf = '\0'; 
            return -1;   // TODO: what is the API in such case?
        }

        int bytesToRead = len;
        int bytesRead = 0;

        //if user requested to read more bytes than possible
        if (fsf->getfile_size() < len)
            bytesToRead = fsf->getfile_size();

        for (long int offsetWithinBlock = 0, currBlockSeqNum = 0; bytesRead < bytesToRead; bytesRead++)
        {
            // One Single Indirect block only.
            int nextBlockPtr = (index_block * block_size) + currBlockSeqNum;
            char c;
            readDisk(nextBlockPtr, 1, 1, &c);
            int nextBlockIdx = (int)c;
            // int nextBlockIdx = static_cast<int>(c);

            //read one byte from file to the buffer
            readDisk(nextBlockIdx * block_size + offsetWithinBlock, 1, 1, buf + bytesRead);

            offsetWithinBlock++;

            if (offsetWithinBlock == block_size)
            {
                offsetWithinBlock = 0;
                currBlockSeqNum++; //move to the next block
            }
        }
        buf[bytesRead] = '\0';
        return len; // TODO: return bytesRead instead?
    }
};

/////////////////////////////////////////////////////////////
int main()
{
    int blockSize;
    string fileName;
    char str_to_write[DISK_SIZE];
    char str_to_read[DISK_SIZE];
    int size_to_read;
    int _fd;
    int writed; //written...

    fsDisk *fs = new fsDisk();
    int cmd_;

    //infinite(endless) while(True) loop
    while (1)
    {
        cin >> cmd_;
        if (!cin)
            exit(0); //EOF

        switch (cmd_)
        {
        //Delete all Disk content and exit
        case 0:        // exit
            delete fs; //will call the destructor ~fsDisk
            exit(0);
            break;

        //Print all the files that are on the disk
        // [Files list and the disk's content]
        case 1: // list-file
            fs->listAll();
            break;

        // format the disk with a new blockSize value
        case 2:               // format
            cin >> blockSize; // no message is printed to the user asking for input
                              // instead - we consider this as an argument to the command ("2 2046")
            fs->fsFormat(blockSize);
            break;

        case 3:                             // create-file
            cin >> fileName;                //first arg is the new file name
            _fd = fs->CreateFile(fileName); // keep the new fd
            cout << "CreateFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 4: // open-file
            cin >> fileName;
            _fd = fs->OpenFile(fileName);
            cout << "OpenFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 5: // close-file
            cin >> _fd;
            fileName = fs->CloseFile(_fd);
            cout << "CloseFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 6: // write-file
            cin >> _fd;
            cin >> str_to_write;
            writed = fs->WriteToFile(_fd, str_to_write, strlen(str_to_write));
            // I needed to add this line according to the test image
            cout << "Writed: " << writed << " Char's into File Descriptor #: " << _fd << endl;
            break;

        case 7: // read-file
            cin >> _fd;
            cin >> size_to_read;
            fs->ReadFromFile(_fd, str_to_read, size_to_read);
            cout << "ReadFromFile: " << str_to_read << endl;
            break;

        case 8: // delete file
            cin >> fileName;
            _fd = fs->DelFile(fileName);
            cout << "DeletedFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;
        default:
            break;
        }
    }
}
