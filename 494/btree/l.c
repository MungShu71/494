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
   n->keys = (unsigned char **) malloc( tree->keys_per_block + 1 );
   n->lbas = (unsigned int*) malloc( tree->lbas_per_block + 1 );

   for (int i = 0; i < tree->keys_per_block; i ++){
      n->keys[i] = &n->bytes[(i * tree->key_size) + 2];
   }
   n->nkeys = 0;
   n->flush = 0;
   n->lba = 0
   n->parent_index = 0
   n->parent = NULL;
   n->ptr = NULL;
}

void fill(B_Tree *tree, Tree_Node *n, unsigned int lba){
   jdisk_read(tree->disk, 
   
   for (int i = 0; i < n->nkeys; i ++) n->keys[i] = &n->bytes[(i * tree->key_size) + 2];
   for (int i = 0; i < n->nkeys + 1; i ++) n->lbas[i] = &n->bytes[JDISK_SECTOR_SIZE - ((4 * tree->lbas_per_block) + (4 * i))]; 
}

int sort(B_Tree *tree, Tree_Node *n, void * key, int T){
   int res, found;
   found = 0;
   for (int i = 0; i < n->nkeys; i ++){
      res = memcmp(key, n->keys[i], tree->key_size);
      if (res <= 0){
         if (res == 0) return i;
         break;
      }
   }
   return i;
}


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
   b->tmp_e_index = 2;
   b->flush = -1;

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
   b->flush = -1;



   return (void *) b;
}

unsigned int b_tree_insert(void *b_tree, void *key, void *record){
  (void) b_tree;
  (void) key;
  (void) record;
   return 0;
}

unsigned int b_tree_find(void *b_tree, void *key){

   B_Tree *b = (B_Tree *) b_tree;
   void *j = b->disk;
   int key_size = b->key_size;
   unsigned int cur_lba = b->root_lba;
   int mkeys, mlbas, res, i;
   unsigned char nkeys;
   mkeys = b->keys_per_block;
   mlbas = b->lbas_per_block;
   unsigned char external;
   int F = 0; 
   if (b->root == NULL){
      b->root = (Tree_Node *) malloc(sizeof(Tree_Node));
      b->root->keys = (unsigned char **) malloc((mkeys + 1) * key_size);
      b->root->lbas = (unsigned int *) malloc((mkeys + 2) * 4);
      b->free_list = b->root;

      b->root->bytes[0] = 0;
      b->root->bytes[1] = 0;
      b->root->lba = b->root_lba;
      b->root->nkeys = 0;
      b->root->parent = NULL;
   }

   Tree_Node *parent = b->root;

   while (1){
      Tree_Node *n;
      if (b->free_list == NULL) {

         printf("2"); 
         n = (Tree_Node*)malloc(sizeof(Tree_Node)); 
         n->keys = (unsigned char **) malloc((mkeys + 1) * key_size);
         n->lbas = (unsigned int*) malloc((mkeys + 2) * 4);
      } else {
         n = b->free_list; // ????? something pointers
         b->free_list->parent = n->parent;
         n->parent = NULL;
      }

      jdisk_read(j, cur_lba, (n->bytes));       
      external = n->bytes[0];        // 0 for external 1 for internal
      nkeys = n->bytes[1];

      n->internal = external;
      n->nkeys = nkeys;
      n->lba = cur_lba;

      fill(b, n)
     // memcpy(n->keys, &n->bytes[2], mkeys * key_size);  
     // memcpy(n->lbas, &n->bytes[1024 - (mlbas * 4)], mlbas * 4);

      n->parent = parent;
      parent = n;

      for (i = 0; i < nkeys; i++){
         res = memcmp(key, &n->keys[i * key_size], key_size);
         res = memcmp(key, &n->bytes[i * key_size + 2], key_size);
         if (res <= 0){
            if (res == 0) F = 1;
            break;
         }
      }
      cur_lba = n->lbas[i];
      if (external == 0) {
         b->tmp_e = parent;
         if (F) {
            return cur_lba;
         }
         return 0;
      }

   }
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

