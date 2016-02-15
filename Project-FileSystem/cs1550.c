#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define BLOCK_SIZE 512

#define DISK_SIZE 5242880

#define NUM_BLOCKS DISK_SIZE / BLOCK_SIZE //10240 blocks

//Bitmap is a char array that takes up the last 20 blocks of .disk
#define BITMAP_SIZE NUM_BLOCKS - (NUM_BLOCKS / BLOCK_SIZE)//Stores entire disk minus bitmap

//we'll use 8.3 filenames
#define MAX_FILENAME 8
#define MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
        int nFiles;     //How many files are in this directory.
                                //Needs to be less than MAX_FILES_IN_DIR
        struct cs1550_file_directory
        {
                char fname[MAX_FILENAME + 1];   //filename (plus space for nul)
                char fext[MAX_EXTENSION + 1];   //extension (plus space for nul)
                size_t fsize;                                   //file size
                long startBlock;                               //where the first block is on disk
       } __attribute__((packed)) files[MAX_FILES_IN_DIR];      //There is an array of these

        //This is some space to get this to be exactly the size of the disk block.
        //Don't use it for anything.
        char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
};

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
        int nDirectories;       //How many subdirectories are in the root
                                                //Needs to be less than MAX_DIRS_IN_ROOT
        struct cs1550_directory
        {
                char dname[MAX_FILENAME + 1];   //directory name (plus space for nul)
                long startBlock;                               //where the directory block is on disk
        } __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];        //There is an array of these

        //This is some space to get this to be exactly the size of the disk block.
        //Don't use it for anything.
        char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
        //The next disk block, if needed. This is the next pointer in the linked
        //allocation list
        long nNextBlock;

        //And all the rest of the space in the block can be used for actual data
        //storage.
        char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */

static long find_directory(char dir[MAX_FILENAME+1]) {
        int nDirectories[1];
        char checkDir[MAX_FILENAME+1];
        long startBlock[1];
        int found = 0;
        int i = 0;
        FILE *fd;

        fd = fopen(".disk","rb");
        fseek(fd,0,SEEK_SET);//Go to the start of .disk

        fread(nDirectories, sizeof(int), 1, fd);//Get the number of directories inside root
        fseek(fd,sizeof(int),SEEK_SET);//seek to start of directories array


        for(i=0;i<nDirectories[0];i++){//Search through the directories to see if any match directory
       for(i=0;i<nDirectories[0];i++){//Search through the directories to see if any match directory
                fread(checkDir,MAX_FILENAME+1,1,fd);
                if(strcmp(dir,checkDir) == 0){//If we found the directory
                        found = 1;
                        fseek(fd,MAX_FILENAME+1,SEEK_CUR);
                        fread(startBlock,sizeof(long),1,fd);//get where the directory is on the disk
                        break;
                }
                fseek(fd,MAX_FILENAME+1+sizeof(long),SEEK_CUR);
        }

        if(!found){
            startBlock[0] = -1;
        }

        return startBlock[0];
}

static long find_open_block(){

    int i = 1;
    //seek to the start of bitmap
    char bitmap[BITMAP_SIZE];
    FILE *fd;

    fd = fopen(".disk","rb+");
    fseek(fd,-NUM_BLOCKS,SEEK_END);//Go to the start of the bitmap from the end of the file
    fread(bitmap,BITMAP_SIZE,1,fd);//read bitmap from disk into char array bitmap

    //go through bitmap and find the first open space
    for(i=1; i<BITMAP_SIZE; i++){//start at 1 because 0 is root
        if(bitmap[i]==0){//i is the first free block
            return (long)BLOCK_SIZE * i;//return the address to it
        }
    }

    //if we got here there were no more free blocks, return error
    return -1;
}

static int cs1550_getattr(const char *path, struct stat *stbuf)
{
        int res = 0;//return value
        int i = 0;

        int exists = 0;
        long startBlock;
        int nFiles[1];
        int fSize[sizeof(size_t)];
        char checkFile[MAX_FILENAME+1];

        FILE *fd;
        char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];

        memset(directory, 0, MAX_FILENAME + 1);
        memset(filename, 0, MAX_FILENAME + 1);
        memset(extension, 0, MAX_EXTENSION + 1);

        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

        memset(stbuf, 0, sizeof(struct stat));

        //looking for root directory
        if (strcmp(path, "/") == 0) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                res = 0;
        }else{//Not the root diectory
                startBlock = find_directory(directory);//Check for directory in root and returns it's location

                if(startBlock != -1){//if directory exists
                        if(strlen(filename)==0){//Were looking for the directory and found it
                                stbuf->st_mode = S_IFDIR | 0755;
                                stbuf->st_nlink = 2;
                                res = 0; //no error directory found
                        }else{//We are looking for a file inside that directory
                                fd = fopen(".disk","rb+");
                                fseek(fd,startBlock,SEEK_SET);//Go to the directory
                                fread(nFiles, sizeof(int), 1, fd);//Get the number of files inside subdirectory

                                exists = 0;

                                for(i=0; i<nFiles[0]; i++){
                                        fread(checkFile,MAX_FILENAME+1,1,fd);
                                                if(strcmp(filename,checkFile)==0){
                                                        exists = 1;
                                                        fseek(fd,(MAX_FILENAME+1)+(MAX_EXTENSION+1),SEEK_CUR);
                                                        fread(fSize,sizeof(size_t),1,fd);
                                                        break;
                                                }
                                        fseek(fd,((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long)),SEEK_CUR);
                                }
                                if(exists==1){
                                        stbuf->st_mode = S_IFREG | 0666;
                                        stbuf->st_nlink = 1; //file links
                                        stbuf->st_size = fSize[0]; //file size - make sure you replace with real size!
                                        res = 0; // no error file found
                                }else{
                                        res = -ENOENT;
                                }
                                fclose(fd);
                        }
                }else{
                        res = -ENOENT;//Directory doesn't exist
                }
        }
        return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
        (void) offset;
        (void) fi;

        int i = 0;
        int exists = 0;

        FILE* fd;

        int nDirectories[1];
        int startBlock[1];
        int nFiles[1];
        char checkDir[MAX_FILENAME+1];
        char checkFile[MAX_FILENAME+1];


        char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];

        memset(directory, 0, MAX_FILENAME + 1);
        memset(filename, 0, MAX_FILENAME + 1);
        memset(extension, 0, MAX_EXTENSION + 1);

        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

        if (strcmp(path, "/") != 0){//if this is a subdirectory
                fd = fopen(".disk","rb");
                fseek(fd,0,SEEK_SET);//Go to the start of .disk
                fread(nDirectories, sizeof(int), 1, fd);//Get the number of directories
                fseek(fd,sizeof(int),SEEK_SET);//seek to start of directories array

                for(i=0;i<nDirectories[0];i++){//Search through the directories in root
                        fread(checkDir,MAX_FILENAME+1,1,fd);
                        if(strcmp(directory,checkDir)==0){
                                exists = 1;
                                fseek(fd,MAX_FILENAME+1,SEEK_CUR);
                                fread(startBlock,sizeof(long),1,fd);//get where our directory is located on disk
                                break;
                        }
                        fseek(fd,MAX_FILENAME+1+sizeof(long),SEEK_CUR);
                }

                if(exists){
                        filler(buf, ".", NULL,0);
                        filler(buf, "..", NULL, 0);
                        fseek(fd,startBlock[0],SEEK_SET);
                        fread(nFiles, sizeof(int), 1, fd);//Get the number of files inside subdirectory
                        for(i=0; i<nFiles[0]; i++){
                                fread(checkFile,MAX_FILENAME+1,1,fd);
                                filler(buf,checkFile,NULL,0);
                                fseek(fd,((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long)),SEEK_CUR);
                        }
                        fclose(fd);
                        return 0;
                }else{
                        fclose(fd);
                        return -ENOENT;
                }
        }else{//if this directory is the root, read from root_directory
                filler(buf, ".", NULL,0);
                filler(buf, "..", NULL, 0);
                fd = fopen(".disk","rb");
                fseek(fd,0,SEEK_SET);
                fread(nDirectories,sizeof(int),1,fd);
                fseek(fd,sizeof(int),SEEK_SET);
                for(i=0;i<nDirectories[0];i++){
                        fread(checkDir,MAX_FILENAME+1,1,fd);
                        filler(buf,checkDir,NULL,0);
                        fseek(fd,MAX_FILENAME+1+sizeof(long),SEEK_CUR);
                }
                fclose(fd);
                return 0;

        }
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
        (void) path;
        (void) mode;

        int i = 0;
        int found = 0;
        cs1550_root_directory root_dir;
        FILE* fd;

        char directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];

        memset(directory, 0, MAX_FILENAME + 1);
        memset(filename, 0, MAX_FILENAME + 1);
        memset(extension, 0, MAX_EXTENSION + 1);

        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

        if(strlen(directory)>MAX_FILENAME){
                return -ENAMETOOLONG;
        }

        if(strlen(filename)!=0){
                return -EPERM;
        }

        //Check if directory exists
        fd = fopen(".disk","rb+");
        fseek(fd,0,SEEK_SET);//Go to the start of .disk
        fread(root_dir, sizeof(root_dir), 1, fd);//Get root

        for(i=0;i<MAX_DIRS_IN_ROOT;i++){//Search through the directories to see if any match directory
                if(strcmp(root_dir.directories[i],directory)==0{
                    found = 1;
                }
        }
        fclose(fd);

        if(found){
                return -EEXIST;
        }

        //Root is already full
        if(root_dir.nDirectories==MAX_DIRS_IN_ROOT){
            return -ENOENT;
        }

        //Add directory to root
        root_dir.nDirectories += 1;//Increase number of directories in root

        newStartBlock = find_open_block();//Find a good location for the directory

        i = 0;
        //Update root with new directory
        while(true){
            if(strlen(root_dir.directories[i].dname)==0){
                strcpy(root_dir.directories[i].dname,directory);
                root_dir.directories[i].startBlock = newStartBlock;
                break;
            }
            if(i>MAX_DIRS_IN_ROOT){
                return -ENOENT;
            }
            i++;
        }

        fwrite(root_dir,sizeof(root_dir,1,fd);//Add new root file to disk
        fclose(fd);

        return 0;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
        (void) path;
    return 0;


/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
        (void) mode;
        (void) dev;
        (void) path;
        return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
                          struct fuse_file_info *fi)
{
        (void) buf;
        (void) offset;
        (void) fi;
        (void) path;

        //check to make sure path exists
        //check that size is > 0
        //check that offset is <= to the file size
        //read in data
        //set size and return, or error

        size = 0;

        return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi)
{
        (void) buf;
        (void) offset;
        (void) fi;
        (void) path;

        //check to make sure path exists
        //check that size is > 0
        //check that offset is <= to the file size

        //write data
        //set size (should be same as input) and return, or error

        return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
        (void) path;
        (void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
        (void) path;
        (void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
           if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
        (void) path;
        (void) fi;

        return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr    = cs1550_getattr,
    .readdir    = cs1550_readdir,
    .mkdir      = cs1550_mkdir,
        .rmdir = cs1550_rmdir,
    .read       = cs1550_read,
    .write      = cs1550_write,
        .mknod  = cs1550_mknod,
        .unlink = cs1550_unlink,
        .truncate = cs1550_truncate,
        .flush = cs1550_flush,
        .open   = cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
        return fuse_main(argc, argv, &hello_oper, NULL);
}


