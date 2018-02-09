#ifndef EVS_LRU_H
#define EVS_LRU_H

typedef int lru_cmp_func(const void *, const void *);

typedef int lru_get_key_func(void *);

typedef struct payload_s payload_t;

struct payload_s {
  const void  *key;
  void        *val;
};

typedef struct lru_node_s lru_node_t;

struct lru_node_s {
  struct lru_node_s *next;  
  struct lru_node_s *prev;
  struct payload_s *payload_ptr;
};

const void * lru_get_key(payload_t *p);
_Bool lru_insert_left(lru_node_t **node_pptr, void *data_p, size_t s);
_Bool delete(lru_node_t *n);
void swap(int timeout);
lru_node_t * init_lru(void *data_p, size_t size);
lru_node_t * lru_get_node(lru_node_t **node, void *key, lru_cmp_func *);
lru_node_t *lru_get_head(lru_node_t **node_pptr);
lru_node_t *lru_get_tail(lru_node_t **node_pptr);
void purge_all(lru_node_t **node_pptr);
#endif
