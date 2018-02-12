/* Linked list
 */

#include "evs_internal.h"
#include "evs_lru.h"
#include "evs_log.h"

static lru_node_t * lru_find_tail(lru_node_t *node, void *key);
static lru_node_t * lru_find_head(lru_node_t *node, void *key);

const void * lru_get_key(payload_t *p) { return p->key; };

lru_node_t *
init_lru(void *data_p, size_t size)
{
  lru_node_t *node = NULL;
  time_t now = time(&now);
  
  node = malloc(sizeof(node) + size + sizeof(time_t) + sizeof(int));
  if (node != NULL)
    {
      node->next = NULL;
      node->prev = NULL;
      node->payload_ptr = data_p;
      node->start = now;
      node->struct_ref = 1;
    }
  
  return node;
}

/* Timer also starts */
_Bool
lru_insert_left(lru_node_t **node_pptr, void *data_p, size_t s)
{
  lru_node_t *ptr = *node_pptr,  /* current */
    *prev = ptr, *head = lru_get_head(node_pptr);
  time_t now = time(&now);
  
  for (;;)
    {
      ptr = ptr->next;
      if (ptr == NULL)
	{
	  ptr = malloc(sizeof(ptr) + s + sizeof(time_t) + sizeof(int));
	  if (ptr != NULL)
	    {
	      ptr->next = NULL;
	      ptr->prev = *node_pptr;// head
	      prev->next = ptr;
	      ptr->payload_ptr = data_p;
	      ptr->start = now;
	      ptr->struct_ref++;
	      *node_pptr = ptr;
	      return true;
	    }
	  break;
	}
    }
  return false;
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

/* Pop node and insert it to head */
lru_node_t *
lru_get_node(lru_node_t **node_pptr, void *key, lru_cmp_func *func)
{
  lru_node_t *ptr = *node_pptr,
    *head = lru_get_head(node_pptr),
    *tail = lru_get_tail(node_pptr);
  time_t now = time(&now);
  
  if (ptr != NULL)
    {
      while (ptr != NULL)
	{

	  if (func(key, lru_get_key(ptr->payload_ptr)) == 0)
	    {
	      lru_node_t *cpy = ptr;
	      
	      if (func(key, lru_get_key(head->payload_ptr)) == 0) /* the key hits head */
		{
		  ptr->start = now; // reinitialize timer
		  return ptr;
		}
	      
	      if (func(key, lru_get_key(tail->payload_ptr)) == 0) /* the key hits tail */
		{
		  tail->next->prev = NULL;
		  tail = tail->next;
		  memcpy(cpy->payload_ptr, ptr->payload_ptr, sizeof(ptr->payload_ptr)); 
		  cpy->prev = head;
		  cpy->next = NULL;
		  assert(lru_insert_left(node_pptr, ptr->payload_ptr,
					 sizeof(ptr->payload_ptr)) != false);		  
		}
	      else
		{
		  assert(ptr->prev != NULL);		  
		  assert(ptr->next != NULL);
		  memcpy(cpy->payload_ptr, ptr->payload_ptr, sizeof(ptr->payload_ptr));
		  ptr->next->prev = ptr->prev;
		  ptr->prev->next = ptr->next;
		  cpy->next = NULL;
		  cpy->prev = head;
		  head->next = cpy;
		  assert(lru_insert_left(node_pptr, ptr->payload_ptr,
					 sizeof(ptr->payload_ptr)) == true);		  
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

void
purge_all(lru_node_t **node_pptr)
{
  lru_node_t *ptr = *node_pptr;
  
  while(ptr != NULL)
    {
      log_debug(DEBUG, "removeing ... %s", (const char*)ptr->payload_ptr->key);      
      ptr = ptr->prev; //backward til nil
      free(ptr);
    }
}

void
lru_remove_oldest(lru_node_t **node_pptr, int timeout)
{
  lru_node_t *ptr = *node_pptr,
    *tail = lru_get_tail(node_pptr),
    *next = tail->next;
  time_t now;
  
  time(&now);
  
  while(1)
    {
      if (now - tail->start <= timeout)
	{
	  if (tail != NULL)
	    {
	      next->prev = NULL;
	      log_debug(DEBUG, "removed \"%s\"", (char*)tail->payload_ptr->key);
	      free(tail);
	      break;
	    }
	}
      break;
    }
}
