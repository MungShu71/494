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
   n->keys = (unsigned char **) malloc((tree->keys_per_block + 1 ) * sizeof(char *));
   n->lbas = (unsigned int*) malloc((tree->lbas_per_block  + 1) * sizeof(unsigned int));

   //  bzero(n->keys, (tree->keys_per_block + 1) * sizeof(char *));

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
   //   memcpy(&n->nkeys, &n->bytes[1], 1);

   for (int i = 0; i < n->nkeys + 1; i ++) n->keys[i] = &n->bytes[(i * tree->key_size) + 2];
   for (int i = 0; i < n->nkeys + 1; i ++) n->lbas[i] = n->bytes[(JDISK_SECTOR_SIZE - ((4 * tree->lbas_per_block))) + (4 * i)]; 
   return n;
}

void shift(B_Tree *tree, Tree_Node *n, int i, int T){
   // if (T == 0 && i >= n->nkeys + 1) return;
   if (T != 0 && i >= n->nkeys) {
   //   fprintf(stderr, "T: %d %d\n", T, n->lba);
   
   return;
   }

   memcpy(n->keys[i + 1], n->keys[i], (tree->keys_per_block - i) * tree->key_size );
   if ( T == 1){
      //    memcpy(&n->lbas[i + 1], &n->lbas[i], (tree->lbas_per_block - i) * 4); 
   } else{
      //    memcpy(&n->lbas[i + 1], &n->lbas[i], (tree->lbas_per_block - i) * 4); 
   }

   for (int k = n->nkeys; k >= i; k --){
      //    memcpy(n->keys[k], n->keys[k-1], tree->key_size );
      n->lbas[k+1] = n->lbas[k] ;
   }

}
void split(B_Tree *tree, Tree_Node *n){
   int i, res;
   //Tree_Node *parent, *child2;
   while (1){
      Tree_Node *parent, *child2;
      if (n->nkeys <= tree->keys_per_block){
         return;
      }

      child2 = new_node(tree);
      child2->lba = tree->first_free_block ++;
      child2->parent = parent;
      child2->internal = 0;

      if (n->parent == NULL) {  // root
         parent = new_node(tree);
         parent->lba = tree->first_free_block ++;
         parent->internal = 1;
         parent->bytes[0] = 1;
         parent->parent = NULL;
         n->parent = parent;
         parent->nkeys = 0;
         tree->root_lba = parent->lba;
      } else {
         parent = n->parent;
      }


      int K = tree->keys_per_block % 2 == 0 ? tree->keys_per_block / 2 : (tree->keys_per_block / 2) + 1;

      for (i = 0; i < parent->nkeys; i++){
         res = memcmp(parent->keys[i], n->keys[K], tree->key_size);
         if (res <= 0){
            break;
         }
      }
      shift(tree, parent, i, 0);
      memcpy(parent->keys[i], n->keys[K], tree->key_size); 

      parent->nkeys ++;

      parent->lbas[i] = n->lba;
      parent->lbas[i+1] = child2->lba;

      memcpy(&child2->lbas[0], &n->lbas[(K + 1) ], (tree->lbas_per_block - ((K)  )) * 4);
      memcpy(child2->keys[0], n->keys[(K) + 1], (tree->keys_per_block - (( K))) * tree->key_size);

      //child2->nkeys = tree->keys_per_block - ((K) + 1);
      child2->nkeys = K - 1;

      //n->nkeys = n->nkeys - ((n->nkeys/2) + 1);
      n->nkeys -= (K );
      bzero(n->keys[K], ((tree->keys_per_block - (K)) )  * tree->key_size );
      bzero(&n->lbas[K + 1], ((tree->lbas_per_block - (K)) + 1) * 4);
  /*
  if (n->internal == 0){
      fprintf(stderr, "I: %d\n", child2->nkeys);

   fprintf(stderr, "%c | %c | %c | %c | %c \n", child2->keys[0][0], child2->keys[1][0], child2->keys[2][0], child2->keys[3][0], child2->keys[4][0]);
   fprintf(stderr, "%d | %d | %d | %d \n", child2->lbas[0], child2->lbas[1], child2->lbas[2], child2->lbas[3]);
   fprintf(stderr, "key %c | %c | %c | %c | %c \n", n->keys[0][0], n->keys[1][0], n->keys[2][0], n->keys[3][0], n->keys[4][0]);
   fprintf(stderr, "%d | %d | %d | %d | %d \n", n->lbas[0], n->lbas[1], n->lbas[2], n->lbas[3], n->lbas[4]);
   }
*/
      memcpy(parent->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), parent->lbas, tree->lbas_per_block * 4);

      memcpy(child2->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), child2->lbas, tree->lbas_per_block * 4);


      parent->bytes[0] = parent->internal;
      parent->bytes[1] = parent->nkeys;

      child2->bytes[0] = child2->internal;
      child2->bytes[1] = child2->nkeys;

      jdisk_write(tree->disk, parent->lba, parent->bytes);
      jdisk_write(tree->disk, child2->lba, child2->bytes);
      n = parent;
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

   int i, res;

   for (i = 0; i < n->nkeys; i++){
      res = memcmp(key, n->keys[i], tree->key_size);
      if (res <= 0) break;     
   }

   F = tree->first_free_block;
   tree->first_free_block ++;


   shift(tree, n, i, 1);

   memcpy(n->keys[i], key, tree->key_size);

   n->lbas[i] = F;

   jdisk_write(tree->disk, F, record);
   n->nkeys ++;
   n->internal = 0;

   if (n->nkeys > tree->keys_per_block) {
  //fprintf(stderr, " before %d \n", n->nkeys);
      split(tree, n);
   }
   memcpy(n->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), n->lbas, (tree->lbas_per_block) * 4);

   // n->bytes[1] ++;
   n->bytes[1] = n->nkeys;
   n->bytes[0] = n->internal;
   jdisk_write(tree->disk, n->lba, n->bytes);
  // if (n->internal == 0){
  // fprintf(stderr, "NODE lba %d\n ", n->lba);
      
   //  fprintf(stderr, "key %c | %c | %c | %c | %c \n", n->keys[0][0], n->keys[1][0], n->keys[2][0], n->keys[3][0], n->keys[4][0]);
   //  fprintf(stderr, "lba %d | %d | %d | %d \n", n->lbas[1], n->lbas[2], n->lbas[3], n->lbas[4]);
   //  }
   char BUF[1024];
   memcpy(BUF, &tree->key_size, 4);
   memcpy(BUF + 4, &tree->root_lba, 4);

   memcpy(BUF + 8, &tree->first_free_block, 8);
   jdisk_write(tree->disk, 0, BUF);
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
            // we have found the key
            if (res == 0) F = 1;
            break;
         }
      }
      // i at this point should be the index where the key is

      // building path top down
      n->parent = head;
      head = n; 

      cur = n->lbas[i];
      /* 
       * If we are at external level and we have found the key, return lba of val sector
       * If we are at exernal level and have no found the key, key is not in tree
       */
      if (n->internal == 0){    

         // store complete path from root to external in tmp_e
         tree->tmp_e = head;
         tree->tmp_e_index = i;
         if (F){
            return cur;
         }     
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
   memcpy(&b->key_size, BUF, 4);
   memcpy(&b->root_lba, BUF + 4, 4);
   memcpy(&b->first_free_block, BUF + 8,8);

   b->disk = d;

   jdisk_read(d, 0, b);

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

void *b_tree_disk(void *b_tree){
   B_Tree *b = (B_Tree *) b_tree;
   return b->disk;
}

int b_tree_key_size(void *b_tree){
   B_Tree *b = (B_Tree *) b_tree;
   return b->key_size;
}
void print_node(Tree_Node *node, B_Tree * tree) {
   fprintf(stderr, "[node->nkeys] | %d\n", node->nkeys);
   fprintf(stderr, "[node->flush] | %d\n", node->flush);
   fprintf(stderr, "[node->inter] | %d\n", node->internal);
   fprintf(stderr, "[node->lba  ] | %u\n", node->lba);
   fprintf(stderr, "[node->keys ] | ");
   for (int i = 0; i < tree->keys_per_block + 1; i++) {
      fprintf(stderr, "[%c]", node->keys[i]);
   }
   fprintf(stderr, "\n");
   fprintf(stderr, "[node->lbas ] | ");
   for (int i = 0; i < tree->lbas_per_block + 1; i++) {
      fprintf(stderr, "[%d]", node->lbas[i]);
   }
   fprintf(stderr, "\n");
   fprintf(stderr, "[node->paren] | %p\n", node->parent);
   fprintf(stderr, "[node->par_i] | %d\n", node->parent_index);
   fprintf(stderr, "[node->ptr  ] | %p\n", node->ptr);
   fprintf(stderr, "\n");
}
void p(Tree_Node * n, B_Tree * tree){
   print_node(n, tree);
   for (int i = 0; i < n->nkeys; i ++){
      Tree_Node * e = new_node(tree);

      e = fill(tree,e, e->lbas[i]);
      p(e, tree);
   }
}
void b_tree_print_tree(void *b_tree){
   B_Tree * tree = (B_Tree *)b_tree;
   Tree_Node * root = new_node(tree);
   root = fill(tree,root, tree->root_lba);
   p(root, tree);


}

