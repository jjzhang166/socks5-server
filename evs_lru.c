#include "evs_internal.h"
#include "evs_lru.h"
#include "evs_log.h"

static lru_node_t * lru_find_tail(lru_node_t *node, void *key);
static lru_node_t * lru_find_head(lru_node_t *node, void *key);

const void * get_key(payload_t *p) { return p->key; };

lru_node_t *
init_lru(void *data_p, size_t size)
{
  lru_node_t *node = NULL;
  
  node = malloc(sizeof(node) + size);
  if (node != NULL)
    {
      node->next = NULL;
      node->prev = NULL;
      node->payload_ptr = data_p;
    }
  return node;
}

_Bool
lru_insert_left(lru_node_t **node_pptr, void *data_p, size_t s)
{
  lru_node_t *ptr = *node_pptr,  /* current */
    *prev = ptr;
  
  for (;;)
    {
      ptr = ptr->next;
      if (ptr == NULL)
	{
	  ptr = malloc(sizeof(ptr) + s);
	  if (ptr != NULL)
	    {
	      ptr->next = NULL;
	      ptr->prev = prev;
	      prev->next = ptr;
	      ptr->payload_ptr = data_p;
	      *node_pptr = ptr;
	      return true;
	    }
	  break;
	}
    }
  return false;
}

/* Append */
_Bool
lru_insert_right(lru_node_t **node_pptr, void *data_p, size_t s)  
{
  lru_node_t *node_ptr = *node_pptr; /* current */
  
  if (node_ptr == NULL)
    {
      node_ptr = malloc(sizeof(node_ptr) + s);
      if (node_ptr != NULL)
	{
	  node_ptr->next = *node_pptr;
	  node_ptr->prev = NULL;
	  node_ptr->payload_ptr = data_p;
	  *node_pptr = node_ptr; /* swap */
	  return true;
	}
      return false;
    }
  return lru_insert_left(&node_ptr->prev, data_p, s);  
}

lru_node_t *
lru_get_head(lru_node_t **node_pptr)
{
  lru_node_t *ptr = *node_pptr;
  if (ptr->next != NULL)
    return lru_get_head(&ptr->next);
  return ptr;
}

lru_node_t *
lru_get_tail(lru_node_t **node_pptr)
{
  lru_node_t *ptr = *node_pptr;
  if (ptr->prev != NULL)
    return lru_get_tail(&ptr->prev);
  return ptr;
}

lru_node_t *
lru_get_node(lru_node_t **node_pptr, void *key, lru_cmp_func *func)
{
  lru_node_t *ptr = *node_pptr,
    *head = lru_get_head(node_pptr),
    *tail = lru_get_tail(node_pptr);
  
  if (ptr != NULL)
    {
      while (ptr != NULL)
	{

	  if (func(key, get_key(ptr->payload_ptr)) == 0)
	    {
	      lru_node_t *cpy = ptr;
	      
	      if (func(key, get_key(head->payload_ptr)) == 0) /* the key hits head */
		{
		  return ptr;
		}
	      
	      if (func(key, get_key(tail->payload_ptr)) == 0) /* the key hits tail */
		{
		  log_info("tail");		  
		  assert(lru_insert_left(node_pptr, ptr->payload_ptr,
					 sizeof(ptr->payload_ptr)) != false);
		  memcpy(cpy->payload_ptr, ptr->payload_ptr, sizeof(ptr->payload_ptr));		  
		  cpy->prev = head;
		  cpy->next = NULL;		  
		  // ptr->prev = NULL;
		  tail->prev = NULL;
		  tail = ptr->next;
		}
	      else
		{
		  assert(ptr->prev != NULL);		  
		  assert(ptr->next != NULL);
		  assert(lru_insert_left(node_pptr, ptr->payload_ptr,
					 sizeof(ptr->payload_ptr)) == true);
		  memcpy(cpy->payload_ptr, ptr->payload_ptr, sizeof(ptr->payload_ptr));
		  ptr->next->prev = ptr->prev;
		  ptr->prev->next = ptr->next;
		  cpy->next = NULL;
		  cpy->prev = head;
		  head->next = cpy;
		}
	      free(ptr);
	      return cpy;
	    }
	  ptr = ptr->prev;
	}
      return NULL;
    }
  else /* ptr is NULL */
    return NULL;
}
