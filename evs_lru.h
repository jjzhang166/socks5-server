#ifndef EVS_LRU_H
#define EVS_LRU_H

#include <sys/time.h>
#include <sys/socket.h>

typedef int lru_cmp_func(const void *, const void *);
typedef int lru_get_key_func(void *);

typedef struct payload_s payload_t;

struct payload_s {
  const void  *key;
  void        *val;
};

typedef struct lru_node_s lru_node_t;

struct lru_node_s {
  /* reference count */
  int struct_ref;
  /* insertion time */
  time_t start;
  const char *key;
  void *payload_ptr;
  //payload_t *payload_ptr;
  /* Used to maintain linked list */  
  struct lru_node_s *next;  
  struct lru_node_s *prev;
};

const char * lru_get_key(lru_node_t *p);
void purge_all(lru_node_t **node_pptr);
_Bool lru_insert_left(lru_node_t **node_pptr, const char *key, void *data_p, size_t s);
/* wait for x nanosecond */
void lru_remove_oldest(lru_node_t **node_pptr, int timeout);
lru_node_t * init_lru(void);
lru_node_t * lru_get_node(lru_node_t **node, void *key, lru_cmp_func *);
lru_node_t *lru_get_head(lru_node_t **node_pptr);
lru_node_t *lru_get_tail(lru_node_t **node_pptr);
#endif
