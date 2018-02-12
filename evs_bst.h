#ifndef EVS_BST_H
#define EVS_BST_H


typedef struct node_s node_t;

struct node_s {
  struct node_s *left, *right;
  struct socks_name_s *name;
  char payload[];
};

typedef const void *bst_get_key_t(const void *);
typedef int bst_cmp_t(const void *key1, const void *key2);

typedef struct bst_s bst_t;

struct bst_s {
  struct node_s *root;
  bst_get_key_t *get_key;
  bst_cmp_t *cmp;
};

bst_t * new_bst(bst_cmp_t *cmp, bst_get_key_t *get);
_Bool bst_insert(bst_t *t, const void *, size_t size);
_Bool bst_delete(bst_t *t, const void *key);
const void * bst_search(bst_t *t, const void *key);
const void * get_key(const void * p);

#endif /* EVS_BST_H */
