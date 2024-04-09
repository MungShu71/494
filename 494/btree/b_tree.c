#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b_tree.h"

//struct declarations

typedef struct tnode {
   unsigned char bytes[JDISK_SECTOR_SIZE +
      256]; /* This holds the sector for reading and writing.
               It has extra room because your internal
               representation will hold an extra key. */
   unsigned char nkeys;      /* Number of keys in the node */
   unsigned char
      flush; /* Should I flush this to disk at the end of b_tree_insert()? */
   unsigned char internal; /* Internal or external node */
   unsigned int lba;       /* LBA when the node is flushed */
   unsigned char **keys;   /* Pointers to the keys.  Size = MAXKEY+1 */
   unsigned int *lbas;     /* Pointer to the array of LBA's.  Size = MAXKEY+2 */
   struct tnode *parent;   /* Pointer to my parent -- useful for splitting */
   int parent_index;       /* My index in my parent */
   struct tnode *ptr;      /* Free list link */
} Tree_Node;

typedef struct {
   int key_size; /* These are the first 16/12 bytes in sector 0 */
   unsigned int root_lba;
   unsigned long first_free_block;

   void *disk;             /* The jdisk */
   unsigned long size;     /* The jdisk's size */
   unsigned long num_lbas; /* size/JDISK_SECTOR_SIZE */
   int keys_per_block;     /* MAXKEY */
   int lbas_per_block;     /* MAXKEY+1 */
   Tree_Node *free_list;   /* Free list of nodes */

   Tree_Node
      *tmp_e; /* When find() fails, this is a pointer to the external node */
   int tmp_e_index; /* and the index where the key should have gone */

   int flush; /* Should I flush sector[0] to disk after b_tree_insert() */
} B_Tree;

Tree_Node * new_node(B_Tree * tree){
   Tree_Node * n;
   n = (Tree_Node*)malloc(sizeof(Tree_Node));
   n->keys = (unsigned char **) malloc((tree->keys_per_block + 1 ) * sizeof(char *));
   n->lbas = (unsigned int*) malloc((tree->lbas_per_block  + 1) * sizeof(unsigned int));

   for (int i = 0; i < tree->keys_per_block + 1; i ++){
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

Tree_Node * fill(B_Tree *tree, Tree_Node *n,  int lba){
   jdisk_read(tree->disk, lba, n->bytes);

   n->lba = lba; 
   n->internal = n->bytes[0];
   n->nkeys = n->bytes[1];

   for (int i = 0; i < tree->keys_per_block + 1; i ++) n->keys[i] = &n->bytes[(i * tree->key_size) + 2];
   memcpy(n->lbas, n->bytes + JDISK_SECTOR_SIZE - (4 * tree->lbas_per_block), tree->lbas_per_block * 4);
   return n;
}

void *b_tree_create(char *filename, long size, int key_size){
   unsigned char BUF[1024];
   void *j = jdisk_create(filename, size);

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
   b->tmp_e = NULL;
   b->tmp_e_index = -1;
   b->flush = 0;

   memcpy(BUF, &key_size, 4);
   memcpy(BUF + 4, &b->root_lba, 4);
   memcpy(BUF + 8, &b->first_free_block, 8);

   jdisk_write(j, 0, BUF);

   Tree_Node *n= new_node(b);
   jdisk_write(j, 1, n->bytes);

   return (void*) b;

}

void *b_tree_attach(char *filename){   
   unsigned char BUF[1024];
   B_Tree *b = (B_Tree*) malloc(sizeof(B_Tree));
   void *d = jdisk_attach(filename);
   jdisk_read(d, 0, BUF);
   memcpy(&b->key_size, BUF, 4);
   memcpy(&b->root_lba, BUF + 4, 4);
   memcpy(&b->first_free_block, BUF + 8,8);

   b->disk = d;
   b->size = jdisk_size(d);
   b->num_lbas = b->size / JDISK_SECTOR_SIZE;
   b->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (b->key_size + 4);
   b->lbas_per_block = b->keys_per_block + 1;
   b->free_list = NULL;
   b->tmp_e = NULL;
   b->tmp_e_index = -1;
   b->flush = 0;

   return (void *) b;
}


void split(B_Tree *tree, Tree_Node *n){
   int i, res;
   Tree_Node  *parent, *child2;

   if (n->lba == tree->root_lba) { 
      parent = new_node(tree);
      parent->lba = tree->first_free_block ++;
      parent->internal = 1;
      parent->parent = NULL;
      n->parent = parent;
      parent->nkeys = 0;
      tree->root_lba = parent->lba; 
   } else {
      parent = n->parent;
   }

   child2 = new_node(tree);
   child2->lba = tree->first_free_block ++;
   child2->parent = parent;
   child2->internal = n->internal;

   int K = n->nkeys / 2;

   for (i = 0; i < parent->nkeys; i++){
      res = memcmp(n->keys[K], parent->keys[i], tree->key_size);
      if (res <= 0){
         break;
      }
   }

   for (int k = parent->nkeys; k > i; k--){
      memcpy(parent->keys[k], parent->keys[k-1], tree->key_size);
      parent->lbas[k + 1] = parent->lbas[k];
   }

   memcpy(parent->keys[i], n->keys[K], tree->key_size); 

   parent->nkeys ++;
   parent->lbas[i] = n->lba;
   parent->lbas[i+1] = child2->lba;


   int j, k = 0;
   for (  j = K+1; j < n->nkeys; j++) {
      child2->nkeys ++;
      memcpy(child2->keys[j-(K+1)], n->keys[j], tree->key_size);
      child2->lbas[k] = n->lbas[j];
      k++;
   }  

   child2->lbas[j-(K+1)] = n->lbas[j];
   n->nkeys = K;
   child2->nkeys = j-(K+1);
   if (parent->nkeys > tree->keys_per_block) split(tree, parent);

   bzero(n->keys[K], ((tree->keys_per_block - (K)) )  * tree->key_size );
   bzero(&n->lbas[K + 1], ((tree->lbas_per_block - (K)) + 1) * 4);

   memcpy(parent->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), parent->lbas, tree->lbas_per_block * 4);
   memcpy(child2->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), child2->lbas, tree->lbas_per_block * 4);

   parent->bytes[0] = parent->internal;
   parent->bytes[1] = parent->nkeys;

   child2->bytes[0] = child2->internal;
   child2->bytes[1] = child2->nkeys;

   jdisk_write(tree->disk, child2->lba, child2->bytes);

   jdisk_write(tree->disk, parent->lba, parent->bytes);
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
            // we have found the key
            if (res == 0) F = 1;
            break;
         }
      }

      n->parent = head;
      head = n;

      cur = n->lbas[i];
      if (n->internal == 0){

         tree->tmp_e = head;
         tree->tmp_e_index = i;
         if (F){
            return cur;
         }
         return 0;
      }
   }
}

unsigned int b_tree_insert(void *b_tree, void *key, void *record){

   B_Tree * tree = (B_Tree *) b_tree;

   if (tree->first_free_block > tree->num_lbas){
      return 0;
   }
   unsigned int F = b_tree_find(b_tree, key);

   if (F != 0) {
      jdisk_write(tree->disk, F, record);
      return F;
   }

   Tree_Node *n = tree->tmp_e;
   int i, res, k;

//   memmove(n->keys[tree->tmp_e_index + 1], n->keys[tree->tmp_e_index], (tree->keys_per_block - tree->tmp_e_index ) * tree->key_size);
   for(i = n->nkeys; i> tree->tmp_e_index; i--){
      memcpy(n->keys[i], n->keys[i-1], tree->key_size);
      n->lbas[i + 1] = n->lbas[i];
   }

   n->lbas[i+1] = n->lbas[i];
   memcpy(n->keys[i], key, tree->key_size);


   n->lbas[i] = tree->first_free_block++;
   F = n->lbas[i];
   jdisk_write(tree->disk, F, record);

   n->nkeys ++; 

   if (n->nkeys > tree->keys_per_block) {
      split(tree, n);
   }

   memcpy(n->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), n->lbas, (tree->lbas_per_block) * 4);

   n->bytes[1] = n->nkeys;
   n->bytes[0] = n->internal;
   jdisk_write(tree->disk, n->lba, n->bytes);

   char BUF[1024];
   memcpy(BUF, &tree->key_size, 4);
   memcpy(BUF + 4, &tree->root_lba, 4);
   memcpy(BUF + 8, &tree->first_free_block, 8);

   jdisk_write(tree->disk, 0, BUF);

   return F;
}

void *b_tree_disk(void *b_tree){
   B_Tree * tree = (B_Tree *)b_tree;
   return (void *)tree->disk;
}

int b_tree_key_size(void *b_tree){
   B_Tree * tree = (B_Tree *)b_tree;
   return tree->key_size;
}

void print_node(B_Tree * tree, Tree_Node *n){
   printf("LBA 0x%08x.  Internal: %d\n", n->lba, n->internal);

    for (int i = 0; i < n->nkeys; i++) {
        printf("  Entry %d: Key: %-32s ", i, n->keys[i]);
        printf("LBA: 0x%08x\n", b_tree_find((void *)tree, n->keys[i]));
    }

    printf("  Entry %d:                                       LBA: 0x%08x\n", n->nkeys, n->lbas[n->nkeys]); 
    printf("\n");

    if (n->internal == 0) return;
    
    for(int i = 0; i < n->nkeys; i++){
      Tree_Node *n2 = new_node(tree);
      n2 = fill(tree, n2, n->lbas[i]);
      print_node(tree, n2);
    }
}

void b_tree_print_tree(void *b_tree){
   B_Tree * tree = (B_Tree *)b_tree;
   Tree_Node *root = new_node(tree);
   root = fill(tree, root, tree->root_lba);
   print_node(tree, root);
}


