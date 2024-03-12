#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "jdisk.h"
#include <math.h>

#define SECTOR_SIZE (1024)
#define SECTOR_MAX_LINKS (512)

char BUF[1024];
//unsigned short LINKS [512];
typedef struct disk{
   void *dp = NULL;
   int written = -1;
   int read = -1;
   int changed = -1;
} disk;
//unsigned short read_short(int link, unsigned short LINKS[]){
   //}
int main(int argc, char** argv){
   struct stat buf;
   char* file = (char*) malloc(sizeof(char) * 50);
   char* diskfile = (char*) malloc(sizeof(char) * 50);
   void *p; // disk pointer
   unsigned long dsize; // bytes in disk
   int dsectors; // sectors in disk
   int FAT_SIZE; // sectors in FAT
   int fsize; // bytes in file
   int fsectors; // sectors in file
   int exist; // stat return val
   int startblock = 0; 
   FILE *fp;

   strcpy(diskfile, argv[1]);

   if (argc == 4){
      strcpy(file, argv[3]);
   }else if (argc == 5){
      strcpy(file, argv[4]);
      startblock = atoi(argv[3]);
   }
   
   fp = fopen(file, "r");
   fseek(fp, 0, SEEK_END);
   fsize = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   //char finput[1];
   //int buf_size = fread(finput, sizeof(char), 
   
   p = jdisk_attach(diskfile);
   if (p == NULL){
      fprintf(stderr, "###\n");
      exit(1);
   }
   dsize = jdisk_size(p);
   dsectors = dsize / SECTOR_SIZE;
  
   FAT_SIZE = ceil((double) dsectors / SECTOR_MAX_LINKS);
   unsigned short LINKS[FAT_SIZE * SECTOR_MAX_LINKS];
   jdisk_read(p, 0, LINKS);
   disk Disks[dsectors];   

   
   if (dsize <= fsize){
      fprintf(stderr, "%s is too big for the disk (%ld vs %d)\n", file, fsize, dsize - SECTOR_SIZE);
      exit(1);
   }
   
   int k = 0;
   int entry;
   int first_free = 0;
   int free_sector_start = FAT_SIZE; 
   fsectors = ceil((double) fsize / SECTOR_SIZE);
   int i;
   for (i = 0; i < fsectors; i++){
      fseek(fp, i * SECTOR_SIZE, SEEK_SET); 
      k += fread(BUF, sizeof(char), SECTOR_SIZE, fp);
      jdisk_write(p, i + FAT_SIZE, BUF);
      first_free = LINKS[first_free];
      free_sector_start ++;
   }

   LINKS[0] = LINKS[first_free];
   LINKS[first_free] = 0;

   jdisk_write(p, 0, LINKS);
   fclose(fp);
   free_sector_start ++;
   
   printf("New file starts at sector %d\n", free_sector_start);
   printf("Reads: %ld\n", jdisk_reads(p));
   printf("Writes: %ld\n", jdisk_writes(p));


   

   return 0;
}
