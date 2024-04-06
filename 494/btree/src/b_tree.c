#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b_tree.h"

typedef struct tnode {
   unsigned char bytes[JDISK_SECTOR_SIZE+256]; /* This holds the sector for reading and writing.
                                                  It has extra room because your internal representation
                                                  will hold an extra key. */
   unsigned char nkeys;                      /* Number of keys in the node */
   unsigned char flush;                      /* Should I flush this to disk at the end of b_tree_insert()? */
   unsigned char internal;                   /* Internal or external node */
   unsigned int lba;                         /* LBA when the node is flushed */
   unsigned char **keys;                     /* Pointers to the keys.  Size = MAXKEY+1 */
   unsigned int *lbas;                       /* Pointer to the array of LBA's.  Size = MAXKEY+2 */
   struct tnode *parent;                     /* Pointer to my parent -- useful for splitting */
   int parent_index;                         /* My index in my parent */
   struct tnode *ptr;                        /* Free list link */
} Tree_Node;

typedef struct {
   int key_size;                 /* These are the first 16/12 bytes in sector 0 */
   unsigned int root_lba;
   Tree_Node *root;
   unsigned long first_free_block;

   void *disk;                   /* The jdisk */
   unsigned long size;           /* The jdisk's size */
   unsigned long num_lbas;       /* size/JDISK_SECTOR_SIZE */
   int keys_per_block;           /* MAXKEY */
   int lbas_per_block;           /* MAXKEY+1 */
   Tree_Node *free_list;         /* Free list of nodes */

   Tree_Node *tmp_e;             /* When find() fails, this is a pointer to the external node */
   int tmp_e_index;              /* and the index where the key should have gone */

   int flush;                    /* Should I flush sector[0] to disk after b_tree_insert() */


} B_Tree;

Tree_Node * new_node(B_Tree * tree){
   Tree_Node * n;
   n = (Tree_Node*)malloc(sizeof(Tree_Node));
   n->keys = (unsigned char **) malloc( sizeof(char *) * (tree->keys_per_block + 1 ));
   n->lbas = (unsigned int*) malloc(sizeof(unsigned int) * (tree->lbas_per_block  + 1) );

   for (int i = 0; i < tree->keys_per_block; i ++){
      n->keys[i] = &n->bytes[(i * tree->key_size) + 2];
   }
   n->nkeys = 0;
   n->flush = 0;
   n->internal = 0;     
   n->lba = 0;
   n->parent_index = 0;
   n->parent = NULL;
   n->ptr = NULL;
   return n;
}

Tree_Node * fill(B_Tree *tree, Tree_Node *n,  unsigned int lba){
   jdisk_read(tree->disk, lba, n->bytes);

   n->lba = lba; 
   n->internal = n->bytes[0];
   n->nkeys = n->bytes[1];

   for (int i = 0; i < n->nkeys + 1; i ++) n->keys[i] = &n->bytes[(i * tree->key_size) + 2];
   for (int i = 0; i < n->nkeys +1; i ++) n->lbas[i] = n->bytes[(JDISK_SECTOR_SIZE - ((4 * tree->lbas_per_block))) + (4 * i)]; 
   return n;
}

void shift(B_Tree *tree, Tree_Node *n, int i){
   if (i >= n->nkeys) return;
   memcpy(n->keys[i + 1], n->keys[i], (tree->keys_per_block - i) * tree->key_size );
   memcpy(&n->lbas[i + 1], &n->lbas[i], (tree->lbas_per_block - i) * 4); 
}


unsigned int b_tree_insert(void *b_tree, void *key, void *record){

   B_Tree * tree = (B_Tree *) b_tree;

   if (tree->first_free_block > tree->num_lbas){
      fprintf(stderr, "Tree Full\n");
      exit(0);
   }

   unsigned int F = b_tree_find(b_tree, key);

   Tree_Node *n = tree->tmp_e;
   int i, res;
   for (i = 0; i < n->nkeys; i++){
      res = memcmp(key, n->keys[i], tree->key_size);
      if (res <= 0){
         if (res == 0){
            jdisk_write(tree->disk, F, record);
            return F;
         }
         break;
      }
   }
   shift(tree, n, i);
   memcpy(n->keys[i], key, tree->key_size); 
   memcpy(&n->lbas[i], &tree->first_free_block, 4); 

   fprintf(stderr, " %c | %c | %c | %c ", n->keys[0][0], n->keys[1][0], n->keys[2][0], n->keys[3][0]);
   printf(" %d | %d | %d | %d \n", n->lbas[0], n->lbas[1], n->lbas[2], n->lbas[3]);

   n->nkeys ++;
   n->bytes[1] ++;

   memcpy(&n->bytes + 2, n->keys, tree->keys_per_block * tree->key_size);
   memcpy(&n->bytes[JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4)], n->lbas, tree->lbas_per_block * 4);

   jdisk_write(tree->disk, tree->first_free_block, record);
   jdisk_write(tree->disk, n->lba, n->bytes);
   jdisk_write(tree->disk, 0, tree);
   F = tree->first_free_block;
   tree->first_free_block ++;
   return F;
}

unsigned int b_tree_find(void *b_tree, void *key){
   unsigned int cur;
   int F = 0;
   int i, res;
   B_Tree * tree = (B_Tree *) b_tree;
   cur = tree->root_lba;
   Tree_Node *head = NULL;
   int s = 1;
   while (1){
      Tree_Node *n = new_node(tree);
      n = fill(tree, n, cur);
      for ( i = 0; i < n->nkeys; i ++){
         res = memcmp(key, n->keys[i], tree->key_size);
         if (res <= 0){
            if (res == 0) F = 1;
            break;
         }
      }
      n->parent = head;
      head = n; 

      cur = n->lbas[i];
      if (n->internal == 0){
         tree->tmp_e = head;
         if (F){
            return cur;
         }     
         tree->tmp_e_index = cur;
         return 0;
      }


   }
}

/*
   while ( tree->tmp_e != NULL){
   printf("%d | ", tree->tmp_e->lba);
   tree->tmp_e = tree->tmp_e->parent;

   }


*/


void *b_tree_create(char *filename, long size, int key_size){
   unsigned char BUF[1024];
   if (size % 1024 != 0 || key_size > 254) {
      fprintf(stderr, "bad file size\n");
      exit(1);
   }
   void *j = jdisk_create(filename, size);

   jdisk_read(j, 0, BUF);


   B_Tree *b = (B_Tree*) malloc(sizeof(B_Tree));
   b->key_size = key_size;
   b->root_lba = 1;
   b->first_free_block = 2;

   b->disk = j;
   b->size = size;
   b->num_lbas = size / 1024;
   b->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (key_size + 4);
   b->lbas_per_block = (JDISK_SECTOR_SIZE - 6) / (key_size + 4) + 1;
   b->free_list = NULL;
   b->tmp_e = NULL;     // holds path that is set in find, always starts from head
   b->tmp_e_index = -1;
   b->flush = 1;

   memcpy(BUF, &key_size, 4);
   memcpy(BUF + 4, &b->root_lba, 4);
   memcpy(BUF + 8, &b->first_free_block, 8);

   jdisk_write(j, 0, BUF);

   return (void*) b;

}











void *b_tree_attach(char *filename){   
   unsigned char BUF[1024];

   B_Tree *b = (B_Tree*) malloc(sizeof(B_Tree));


   void *d = jdisk_attach(filename);


   b->disk = d;

   jdisk_read(d, 0, BUF);

   memcpy(&b->key_size, BUF, 4);
   memcpy(&b->root_lba, BUF + 4, 4);
   memcpy(&b->first_free_block, BUF + 8,8);

   b->size = jdisk_size(d);
   b->num_lbas = b->size / JDISK_SECTOR_SIZE;
   b->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (b->key_size + 4);
   b->lbas_per_block = b->keys_per_block + 1;
   b->free_list = NULL;
   b->tmp_e = NULL;
   b->tmp_e_index = -1;
   b->flush = 1;



   return (void *) b;
}













void *b_tree_disk(void *b_tree){
   B_Tree *b = (B_Tree *) b_tree;
   return b->disk;
}

int b_tree_key_size(void *b_tree){
   B_Tree *b = (B_Tree *) b_tree;
   return b->key_size;
}

void b_tree_print_tree(void *b_tree){
   B_Tree *b = (B_Tree *) b_tree;

   for (int i = 0; i < b->root->nkeys; i ++) fprintf(stderr,"%c ", b->root->keys[i * 200]); 
}

