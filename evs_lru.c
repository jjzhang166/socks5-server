/* Linked list
 */

#include "evs_internal.h"
#include "evs_lru.h"
#include "evs_log.h"

static lru_node_t *get_tail(lru_node_t **node_pptr);
static lru_node_t *get_head(lru_node_t **node_pptr);
static lru_node_t *get_node(lru_node_t **node_pptr, void *key, lru_cmp_func *func);

const char *
lru_get_key(lru_node_t *p)
{
  if (p != NULL)
    return p->key;
  return NULL;
};

lru_node_t *
init_lru(void)
{
  time_t now = time(&now);
  lru_node_t *node_ptr;

  node_ptr = (lru_node_t*)malloc(2 * sizeof(node_ptr)
				 + sizeof(payload_t*) + sizeof(const char*)
			     + sizeof(time_t) + sizeof(int));  
  if (node_ptr != NULL)
    {
      node_ptr->next = NULL;
      node_ptr->prev = NULL;
      node_ptr->struct_ref = 1;
    }
  
  return node_ptr;
}

/* Timer also starts */
_Bool
lru_insert_left(lru_node_t **node_pptr, const char *key, void *data_p, size_t s)
{
  lru_node_t *ptr = *node_pptr, *head = *node_pptr;
  time_t now = time(&now);

  if (ptr != NULL)
    {
      for (;;)
	{
	  ptr = ptr->next;
	  if (ptr == NULL)
	    {
	      ptr = (lru_node_t*)malloc(2 * sizeof(ptr) + sizeof(const char*) +
					s + sizeof(now) + sizeof(int));
	      if (ptr != NULL)
		{
		  ptr->next = NULL;
		  ptr->prev = *node_pptr; // head
		  ptr->key = key;
		  ptr->payload_ptr = data_p;		  
		  ptr->start = now;
		  ptr->struct_ref++;
		  head->next = ptr;
		  *node_pptr = ptr; // swap
		  return true;
		}
	      break;
	    }
	}
    }
  return false;
}

lru_node_t *
lru_get_head(lru_node_t **node_pptr)
{
  lru_node_t *ptr = *node_pptr;
  if (ptr != NULL)
    return get_head(&ptr);
}

static lru_node_t *
get_head(lru_node_t **node_pptr) // head == *node_pptr
{
  lru_node_t *ptr = *node_pptr;
  if (ptr->next != NULL)
    return lru_get_head(&ptr->next);
  return ptr;  
}

static lru_node_t *
get_tail(lru_node_t **node_pptr)
{
  lru_node_t *ptr = *node_pptr;
  
  if (ptr->prev != NULL)
    return get_tail(&ptr->prev);
  return ptr;
}

lru_node_t *
lru_get_tail(lru_node_t **node_pptr)
{
  lru_node_t *node = NULL;
  
  if (*node_pptr != NULL)
    node = get_tail(node_pptr);
  
  return node->next;
}

lru_node_t *
lru_get_node(lru_node_t **node_pptr, void *key, lru_cmp_func *func)
{
  lru_node_t *ptr = *node_pptr;
  if (*node_pptr != NULL && lru_get_key(ptr) != NULL)
    return get_node(node_pptr, key, func);
  return NULL;
}

/* Pop node and insert it to head */
static lru_node_t *
get_node(lru_node_t **node_pptr, void *key, lru_cmp_func *func)
{
  lru_node_t *ptr = *node_pptr, *head = ptr,
    *tail = lru_get_tail(node_pptr);  
  time_t now = time(&now);

  if (ptr != NULL)
    {
      while (ptr != NULL)
	{
	  if (func(key, lru_get_key(ptr)) == 0)
	    {
	      lru_node_t *cpy = ptr;
	      
	      if (func(key, lru_get_key(head)) == 0) /* the key hits head */
		{
		  ptr->start = now; // reinitialize timer
		  return ptr;
		}	    
	      if (func(key, lru_get_key(tail)) == 0) /* the key hits tail */
		{
		  tail->next->prev = NULL;
		  tail = tail->next;
		  memcpy(cpy->payload_ptr, ptr->payload_ptr, sizeof(ptr->payload_ptr)); 
		  cpy->prev = head;
		  cpy->next = NULL;
		  assert(lru_insert_left(node_pptr, key, ptr->payload_ptr,
					 sizeof(ptr->payload_ptr)) != false);		  
		}
	      else
		{
		  assert(ptr->prev != NULL);		  
		  assert(ptr->next != NULL);
		  ptr->next->prev = ptr->prev;
		  ptr->prev->next = ptr->next;
		  cpy->next = NULL;
		  cpy->prev = head;
		  memcpy(cpy->payload_ptr, ptr->payload_ptr, sizeof(ptr->payload_ptr));
		  head->next = cpy;
		  assert(lru_insert_left(node_pptr, key, ptr->payload_ptr,
					 sizeof(ptr->payload_ptr)) == true);		  
		}
	      free(ptr);
	      return cpy;
	    }
	  else // doesn't exist
	    return NULL;
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

  while (ptr != NULL) {
    free(ptr);
    log_debug(DEBUG, "removeing ... %s", lru_get_key(ptr));
    ptr = ptr->prev;
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
      if (now - tail->start <= timeout) {
	if (tail != NULL)
	  {
	    next->prev = NULL;
	    log_debug(DEBUG, "%ld sec elapsed", (long)now - tail->start);
	    log_debug(DEBUG, "removed \"%s\"", (char*)tail->key);
	    free(tail);
	    break;
	  }
      }
      log_debug(DEBUG, "%ld sec elapsed", (long)now - tail->start);
      break;
    }
}
