#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "jdisk.h"
#include <math.h>

#define SECTOR_MAX_LINKS (512)

char BUF[1024];

void IMPORT(FILE *fp, char *file, void *p){
   unsigned short o, n;
   int ss, FAT_SIZE = ceil((double) ((jdisk_size(p) / JDISK_SECTOR_SIZE ) +1) / ( 1+SECTOR_MAX_LINKS));
   
   //if (((jdisk_size(p)/1024)-FAT_SIZE+1)%512  == 0) FAT_SIZE ++;
   unsigned short *links[FAT_SIZE];
   for (int i = 0; i < FAT_SIZE; i ++) links[i] = NULL;
   unsigned short x = 0;
   fp = fopen(file, "r");
   fseek(fp, 0, SEEK_END);
   int fsize = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   int changed = 0;

   if (jdisk_size(p) <= fsize){
      fprintf(stderr, "%s is too big for the disk (%ld vs %d)\n", file, jdisk_size(p), fsize, jdisk_size(p) - JDISK_SECTOR_SIZE );
      exit(1);
   }
   if (fsize % 1024 == 0) ss = 0;
   else if (fsize % 1024 == 1023) ss = 1;
   else if (fsize % 1024 < 1023) {ss = 2;}
   o = (ss == 1) ? 0xffffffff : fsize % 1024;
   int fsectors = ceil((double) fsize / JDISK_SECTOR_SIZE );

   int free_count = 0;
   for (int i = 0; i < fsectors; i++){

      fread(BUF, sizeof(char), JDISK_SECTOR_SIZE , fp);
      if (i == fsectors-1){
         if (ss == 1) memcpy(BUF + 1023, &o, sizeof(char));
         else if (ss == 2) memcpy(BUF + 1022, &o, sizeof(short));
      }
      if (links[x/512] == NULL){
         links[x/512] = (unsigned short *) malloc(sizeof(unsigned short) * 512);
         jdisk_read(p, x/512, links[x/512]);
      }

      x = (links[x/512][x%512] );
      if (x == 0 && i < fsectors){
         fprintf(stderr, "Not enough free sectors (%d) for %s, which needs %d\n", i, file, fsectors);
         exit(1);
      }
      jdisk_write(p, x + FAT_SIZE - 1, BUF); 
   }
   if (links[x/512] == NULL) {
      links[x/512] = (unsigned short *) malloc(sizeof(unsigned short) * 512);
      jdisk_read(p, x/512, links[x/512]);  
   }
   int set = 0;
   if (links[x/512][x%512] != links[0][0])  {
      links[0][0] = links[x/512][x%512] ;
      set |= (1 << (x/1024));
   }
   if (ss == 0) {
      if (links[x/512][x%512] != 0){
         links[x/512][x%512] = 0;
         set |= (1 << (x/1024) );
      }
   }
   else {
      if( links[x/512][x%512] != x){
         links[x/512][x%512] = x;
         set |= (1 << (x/1024) );
      }
   }
   for (int i = 0; i < FAT_SIZE; i++){
      if (links[i] != NULL && (set & (1 << i) > 0)) {
         jdisk_write(p, i, links[i]);
      }
   }
   printf("New file starts at sector %d\n", links[0][0] + FAT_SIZE - 1);
   fclose(fp);
}
void EXPORT(int startblock, FILE *fp, char *file, void *p){
   fp = fopen(file, "w");
   int ss, FAT_SIZE = ceil((double) (jdisk_size(p) / JDISK_SECTOR_SIZE ) / (1+ SECTOR_MAX_LINKS));
   unsigned short *links[FAT_SIZE ];
   int ind; 
   int nbr = 0;
   for (int i = 0; i < FAT_SIZE; i ++) links[i] = NULL;
   int start = startblock;
   while (1){
      ind = start - FAT_SIZE + 1;
      if (links[ind / 512] == NULL){
         links[ind / 512] = (unsigned short*)malloc(sizeof(unsigned short) * 512);
         jdisk_read(p, ind / 512, links[ind /512]);
      }
      unsigned short k = links[ind / 512][ind % 512];
      if (k == 0 || k == ind ) break;
      jdisk_read(p,start, BUF);
      fwrite(BUF, 1, 1024, fp);
      start = k + FAT_SIZE -1;
   }
   int sector = start ;
   jdisk_read(p, sector, BUF);
   if (links[ind/512][ind%512] == 0){
      nbr = 1024;                                       /// taks up all 1024 bytes
   }else {
      if (BUF[1023] == 0xffffffff){
         nbr = 1023;
      }
      else{
         for (int j = 1023; j >= 1022; j --){
            for (int i = 0; i < 8; i ++){
               nbr |= ((BUF[j] & (1 << i)) << ((j - 1022) * 8));
            }
         }
      }
   }
   fwrite(BUF, sizeof(char), nbr, fp);
   fclose(fp);
}

int main(int argc, char** argv){
   void *p; // disk pointer
   int FAT_SIZE; // sectors in FAT
   int startblock = 0;
   FILE *fp;
   p = jdisk_attach(argv[1]);
   if (p == NULL){
      fprintf(stderr, "###\n");
      exit(1);
   }
   FAT_SIZE = ceil((double) (jdisk_size(p) / JDISK_SECTOR_SIZE  ) / SECTOR_MAX_LINKS);
   if (argc == 4){
      IMPORT(fp, argv[3], p);

   }else if (argc == 5){
      startblock = atoi(argv[3]);
      EXPORT(startblock, fp, argv[4], p);
   }

   printf("Reads: %ld\n", jdisk_reads(p));
   printf("Writes: %ld\n", jdisk_writes(p));
   return 0;
}

