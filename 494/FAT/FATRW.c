#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "jdisk.h"
#define SECTOR_SIZE (1024)
typedef struct disk{
   void *dp = NULL;
   int written = -1;
   int read = -1;
   int changed = -1;
} disk;
int main(int argc, char** argv){
   struct stat buf;
   char* file = (char*) malloc(sizeof(char) * 50);
   char* diskfile = (char*) malloc(sizeof(char) * 50);
   strcpy(diskfile, argv[1]);
   unsigned long disk_size;
   
   void *p = jdisk_attach(diskfile);
   if (p == NULL){
      fprintf(stderr, "###\n");
      exit(1);
   }
   disk_size = jdisk_size(p);
   int disk_sectors = disk_size / SECTOR_SIZE;
   disk Disks[disk_sectors];
   int startblock = 0;
   if (argc == 4){
      strcpy(file, argv[3]);
   }else if (argc == 5){
      strcpy(file, argv[4]);
      startblock = atoi(argv[3]);
   }
   printf("%s %s\n", diskfile, file);
   int exist = stat(file, &buf);
   if (exist < 0){
      fprintf(stderr, "sum ting wong\n");
      exit(1);      
   }
   int file_size = buf.st_size;
   int file_sectors = buf.st_size / SECTOR_SIZE;


   if (disk_size <= file_size){
      fprintf(stderr, "%s is too big for the disk (%ld vs %d)\n", file, file_size, disk_size - SECTOR_SIZE);
      exit(1);
   }

   printf("%ld\n", buf.st_size);

   

   printf("HLLO\n");
   return 0;
}
