#include "LibFS.h"
#include "LibDisk.h"
#include "sys/stat.h"
#include "string.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include <dirent.h>
#define MAX_FILES 1000
#define MAX_FILE_BLOCKS 30
#define MAX_OPEN_FILES 256
#define MAX_PATH_LENGTH 256
#define MAX_NAME_LENGTH 15
#define true 0
#define false 1 

// global errno value here
int osErrno;
// global path name to disk
char *image_path;
// global value of available space
int total_available_space = (NUM_SECTORS * SECTOR_SIZE);
// global value for where the inode values start
int inode_sec_start;
//global value for where the data values start
int data_sec_start;
//global value for the max amount of data blocks
int max_data_blocks;

//defines type values for files (directory vs. normal file)
typedef enum
{
	FS_UNUSED = 0,
	FS_DATA,
	FS_DIRECTORY,
} FS_FILE_T;

//Defines the structure of the inode.
typedef struct
{
	int fsize;
	int type;
	int secnum;
} inode;

//Defines the structure of the directory
typedef struct
{
	char fileName[16];
	int inum;
} directory;

//Defines the structure of a data block.
typedef struct
{
	char data[SECTOR_SIZE];
} data_block;

//Defines the structure of an active file.
typedef struct
{
	char full_path[MAX_PATH_LENGTH];
	char * fbuffer;
	int inumb;
	int current_length;
	int current_pos;
} active_file;

//Function to calculate how many blocks/sectors are required for a given data size of an object.
int block_size(int data_size)
{
	int blocks = data_size/SECTOR_SIZE;
	if((data_size%SECTOR_SIZE) > 0)
	{
		blocks++;
	}
	return blocks;
}

char superblock[SECTOR_SIZE] = {0} ; //Defines the superblock as a data_block structure and initializes every bit value to 0.
unsigned char ibm[SECTOR_SIZE] = {0} ; //Defines the inode bitmap as a  data_block structure and initializes every bit value to 0.
unsigned char dbm[SECTOR_SIZE * 3] = {0} ; //Defines the data block map as three data_block structures and initializes every bit value to 0.
inode inode_table[MAX_FILES]; //Defines the inode table.
active_file open_files[MAX_OPEN_FILES]; //Defines the open file table.

//Checks to see if the disk's file exists.
int disk_exists(char *fileName)
{
	struct stat buffer;
	return (stat (fileName, &buffer) == 0);
}

//Searches for a free inode.
int find_free_inode()
{
	int i;
	int j;
	for (i = 0; i < (MAX_FILES/8); i++)
	{
		if (ibm[i] < 255)
		{
			for (j = 0; j < 8; j++)
			{
				if (((ibm[i] >> j) &1) == 0)
				{
					return ((i*8) + j);
				}
			}
		} 
		return -1;
	}
}

//Searches for a free data block.
int find_free_dblock()
{
	int i;
	int j;
	int sector_number;
	for (i = 0; i < (MAX_FILES/8); i++)
	{
		if (dbm[i] < 255)
		{
			for (j = 0; j < 8; j++)
			{
				if (((dbm[i] >> j) &1) == 0)
				{
					sector_number = ((i*8) + j);
					if (sector_number < max_data_blocks)
					{
						return sector_number;
					}
					
					else
					{
						return -1;
					}
				}
			}
		} 
		return -1;
	}
}

//Sets the value of a sector number in the inode bitmap by the byte offset.
int set_ibm(int sectnum)
{
	int byte_num;
	int byte_off;
	
	byte_num = sectnum / 8;
	byte_off = sectnum % 8;

	ibm[byte_num] |= (1 << byte_off); 
}

//Sets the value of a sector number in the data byte map by the byte offset.
int set_dbm(int sectnum)
{
	int byte_num;
	int byte_off;
	
	byte_num = sectnum / 8;
	byte_off = sectnum % 8;

	dbm[byte_num] |= (1 << byte_off);
}

//Clears the inode bitmap of a sector number by the byte offset.
int clear_ibm(int sectnum)
{
	int byte_num;
	int byte_off;
	
	byte_num = sectnum / 8;
	byte_off = sectnum % 8;

	ibm[byte_num] &= ~(1 << byte_off); 
}

//Clears the data byte map of a sector number by the byte offset.
int clear_dbm(int sectnum)
{
	int byte_num;
	int byte_off;
	
	byte_num = sectnum / 8;
	byte_off = sectnum % 8;

	dbm[byte_num] &= ~(1 << byte_off);
}

//Boots the file system.
int 
FS_Boot(char *path)
{
    printf("FS_Boot %s\n", path);

    image_path = path; //Puts the given path name into a global variable for reference.
    int used_space = 0; //Defines and initializes used_space variable to 0.
    int i; //Defines i.

   //If the file at the end of the path already exists, load said disk.
    if(disk_exists(path) == true)
    {
	//Loads the disk image for a given path. Needs to already exist before loading.
	Disk_Load(path);
	
	//Checks if the superblock exists. If not, return an error. If so, read it from the disk.
	if (Disk_Read(0, superblock) != 0)
	{
		osErrno = E_READING_FILE;
		return -1;
	}
	
	//Checks if the superblock's value is the magic number, 42. If it's not, return an error.
	if (superblock[0] != 42)
	{
		osErrno = E_GENERAL;
		return -1;
	}

	//Checks if the inode bitmap exists. If not, return an error. If so, read it from the disk.
	if (Disk_Read(1, (char *)ibm) != 0)
	{	
		osErrno = E_READING_FILE;
		return -1;
	}

	//Checks if the data byte map exists. If not, return an error. If so, read it from the disk.
	for (i = 0; i < 3; i++)
	{
		if (Disk_Read(i+2, (char *)&dbm[i*512]) != 0)
		{	
			osErrno = E_READING_FILE;
			return -1;
		}
	}

	//Checks if the inode table exists. If not, return an error. If so, read it from the disk.
	for (i = 0; i < MAX_FILES; i++)
	{
		if (Disk_Read(i+5, (char *)&inode_table[i].fsize) != 0)
		{
			osErrno = E_READING_FILE;
			return -1;
		}
		
		used_space = used_space + inode_table[i].fsize; //Calculates the amount of used space in the disk image.
	} 

	//Calculates the amount of total available space on the disk, sets it in the global variable.
	total_available_space = total_available_space - (used_space + sizeof(superblock) + sizeof(ibm) + sizeof(dbm) + sizeof(inode_table));
	
	//Initializes the start sector of the inodes.
	inode_sec_start = 5;
	//Calculates and initializes the start sector of the data.
	data_sec_start = (MAX_FILES/4) + 5;
	//Calculates and initializes the max amount of data blocks possible.
	max_data_blocks = total_available_space/SECTOR_SIZE;

	return 0;
    }

    //If the file at the end of the path does not exist, call the disk initialization.
    else
    {
        // oops, check for errors
        if (Disk_Init() != 0) 
    	{
	    printf("Disk_Init() failed\n");
	    osErrno = E_GENERAL;
	    return -1;
    	}

	//If no error in the disk initialization:
	else
	{
		//Initialize the inode bitmap and data byte map's first value to 1 to indicate that they are taken by the superblock.
		ibm[0] = 1;
		dbm[0] = 1;

		//Initializes the start sector of the inodes.
		inode_sec_start = 5;
		//Calculates and initializes the start sector of the data.
		data_sec_start = (MAX_FILES/4) + 5;
		//Initializes the root directory.
		inode_table[0].fsize = 512;
		inode_table[0].type = FS_DIRECTORY;
		inode_table[0].secnum = 0;

		//Calculates the total available space and sets it in the global variable.
		total_available_space = total_available_space - (sizeof(superblock) + sizeof(ibm) + sizeof(dbm) + sizeof(inode_table) + SECTOR_SIZE); //includes root directory
		//Calculates and initializes the max amount of data blocks possible.
		max_data_blocks = total_available_space/SECTOR_SIZE;

		superblock[0] = 42; //Writes 42 into the superblock.
		Disk_Write(0, superblock); //Writes the superblock to memory.
		Disk_Write(1, (char *)ibm); //Writes the sector of the inode bitmap to memory.
		Disk_Write(2, (char *)&dbm[0]); //Writes the first sector of the data block map to memory.
		Disk_Write(3, (char *)&dbm[512]); //Writes the second sector of the data block map to memory.
		Disk_Write(4, (char *)&dbm[1024]); //Writes the third sector of the data block map to memory.

		//Check that the initialized memory version of the disk has been written to the real disk. If it fails, print an error and return -1.
		if (Disk_Save(path) != 0)
		{
			printf("Disk_Save(path) failed\n");
			return -1;
		}

		//If it succeeds, return 0.
		else
		{	
    			return 0;
		}
	}
    }
}

int
FS_Sync()
{
    printf("FS_Sync\n");
    Disk_Save(image_path); //Calls Disk_Save for the global path name.
    return 0;
}

int File_Create(char *file){
  FILE* fp;
  
  fp = fopen (file, "w+");
  if fp =! NULL{
    osErrno = "E_CREATE";
    return -1;
  }
  else if(fclose(fp));
  return 0;
}
int File_Open(char *file){
  FILE* fp;
  int fp;
  if( access( file, F_OK ) != -1 ) {
    open(file);
    printf file;
    return 0;
    else {
      return -1;
      osErrno = "E_NO_SUCH_FILE";
    }
  }
  if files_open > file_limit{ //pseudocode, couldn't figure this one out
    osErrno = "E_FILE_TOO_BIG";
    return -1;
  }
  fclose(fp);   
}


int File_Read(int fd, void *buffer, int size){
  FILE* fp;
  stream = fopen(fd, "r");

 if ( ftell(stream) >= 0 ){ //file is open
  while(fgets(line, 80, fr) != NULL){ //files reads but doesn't do anything else
    sscanf (line);
    printf ("done reading");
    return 0;
  }
    
 }
 else //file is closed
 {
    return -1;
    osErrno = E_BAD_FD;
 }
  fclose(fp);  
}


int
File_Write(int fd, void *buffer, int size)
{
    int new_blocks; //Defines variable for the amount of new blocks needed for the write.
    int cur_blocks; //Defines the variable for the current amount of blocks.
    int snum; //Defines the variable for the sector number.
    int i; //Defines the variable i.
    int bytes_written = 0; //Defines the amount of bytes being written.
    char secbuf[((size/SECTOR_SIZE) + 1) * SECTOR_SIZE]; //Defines and initializes a buffer large enough to contain the user's data.

    printf("FS_Write\n");

    //Checks if the file descriptor's full path exists in the open files table (aka if the file is already open). If not, returns an error.
    if (open_files[fd].full_path != "\0")
    {
	osErrno = E_BAD_FD;
	return -1;
    }

    //Checks if the size of the file's length exceeds the size possible for a file. If it does, returns an error.
    if((size + open_files[fd].current_length) > (SECTOR_SIZE * MAX_FILE_BLOCKS))
    {
	osErrno = E_FILE_TOO_BIG;
	return -1;
    }

    //Checks if there is sufficient space to write a file of size. If not, returns an error.
    if((total_available_space - size) < 0)
    {
	osErrno = E_NO_SPACE;
	return -1;
    }

    //Calculates and initializes the new_blocks variable.
    new_blocks = block_size(size);
    //Calculates and initializes the cur_blocks variable.
    cur_blocks = block_size(inode_table[open_files[fd].inumb].fsize);

    //Enters a memory copy of the user's data into the secbuff.
    memcpy(secbuf, buffer, size);

    //Loop goes through the amount of blocks necessary for the write.
    for (i = 0; i < new_blocks; i++)
    {
	snum = find_free_dblock(); //Finds the first free block.
	if (snum < 0) //If the sector does not exist, return an error (double-checking available space).
	{
		osErrno = E_NO_SPACE;
		return -1;
	} 

	if (Disk_Write(snum, &secbuf[i * SECTOR_SIZE]) != 0) //If the write fails, return an error. Otherwise, the write succeeds.
	{
		osErrno = E_WRITING_FILE;
		return -1;
	}
	set_dbm(snum); //Sets the sector number in the data byte map to occupied.
    } 
    return size; //Returns the size.
}

int
File_Seek(int fd, int offset)
{
    printf("FS_Seek\n");
    return 0;
}

int
File_Close(int fd)
{
    printf("FS_Close\n");
    return 0;
}

int
File_Unlink(char *file)
{
    printf("FS_Unlink\n");
    return 0;
}


// directory ops
int Dir_Create(char *path){
    int r = 0;
    struct stat st = {0};
    if (-1 != stat(path, &st)){//checks to see if the directory already exists
      char* osErrno = "E_CREATE"; // already exists
         return -1;
}
    char* parent = strdup(path);
    if (strcmp(dirname(parent), path)) //creates directory
        r = mkdirhier(parent); // recurse
    if (parent)
        free(parent);//free memory

    if (!r && (r = mkdir(path, 0700)))
        perror(path);

        return r;
    }
}

int
Dir_Size(char *path)
{
    static unsigned int total = 0;

    int sum(const char *fpath, const struct stat *sb, int typeflag) {
    total += sb->st_size;
    return 0;
    }

    if (ftw(path, &sum, 1)) {
        perror("ftw");
        return 2;
    }
    printf("%s: %u\n", path, total);
    printf("Dir_Size\n");
    return total;
}

int
Dir_Read(char *path, void *buffer, int size)
{
char *s;
char *appendchar(char *szString, size_t strsize, char c)
{
    size_t len = strlen(szString);
    if((len+1) < strsize)
    {
        szString[len++] = c;
        szString[len] = '\0';
        return szString;
    }
    return NULL;
}
 DIR *dir;
struct dirent *ent;
if ((dir = opendir (path)) != NULL) {
  /* print all the files and directories within directory */
  while ((ent = readdir (dir)) != NULL) {
    appendchar(s,size,ent->d_name);
  }
  closedir (dir);
} else {
  /* could not open directory */
  perror ("");
  return EXIT_FAILURE;

}
buffer=s;
return buffer;
}


int
Dir_Unlink(char *path)
{
  DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d)
   {
      struct dirent *p;

      r = 0;

      while (!r && (p=readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;

          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
          {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf)
          {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);

             if (!stat(buf, &statbuf))
             {
                if (S_ISDIR(statbuf.st_mode))
                {
                   r2 = remove_directory(buf);
                }
                else
                {
                   r2 = unlink(buf);
                }
             }

             free(buf);
          }

          r = r2;
      }

      closedir(d);
   }

   if (!r)
   {
      r = rmdir(path);
   }

   return r;
}
