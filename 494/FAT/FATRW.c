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

typedef struct disk{
   void *dp = NULL;
   int written = -1;
   int read = -1;
   int changed = -1;
} disk;
void EXPORT(int start_block, FILE *fp){
   (void) start_block;
   (void) fp;

}
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
   int first_free = LINKS[0];
   int free_sector_start = FAT_SIZE; 
   int ss;
   unsigned short o;
   if (fsize % 1024 == 0) ss = 0;
   else if (fsize % 1024 == 1023) {
      ss = 1;
      o = 0xff;
   }
   else if (fsize % 1024 < 1023){
      ss = 2;
      o = fsize % 1024;
   }
   if (first_free == 0){
      printf("#F\n");
   }
   while (first_free != 0){
      first_free = LINKS[first_free]; //?
   }
   fsectors = ceil((double) fsize / SECTOR_SIZE);
   if ( argc == 4){
   int i;
   for (i = 0; i < fsectors; i++){

      k += fread(BUF, sizeof(char), SECTOR_SIZE, fp);
      if (i == fsectors-1){
         
         if (ss == 1) memcpy(BUF + 1023, &o, sizeof(char));
         else if (ss == 2) memcpy(BUF + 1022, &o, sizeof(short));
      }
      jdisk_write(p, LINKS[first_free] + FAT_SIZE - 1, BUF); // ???
      first_free = LINKS[first_free];
      free_sector_start ++;
   }
//   printf("%d %d\n", first_free, LINKS[first_free]);
   LINKS[0] = LINKS[first_free]; //?
   switch (ss){
      case 0:
         LINKS[first_free] = 0;
         break;
      case 1:
      case 2:
         LINKS[first_free] = first_free;
         break; 
   }

   jdisk_write(p, 0, LINKS);
   fclose(fp);

}
else if (argc == 5){
  
}
   printf("New file starts at sector %d\n", free_sector_start);
   printf("Reads: %ld\n", jdisk_reads(p));
   printf("Writes: %ld\n", jdisk_writes(p));
   return 0;
}
