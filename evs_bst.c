#include "evs_internal.h"
#include "evs_log.h"
#include "evs_helper.h"
#include "evs_bst.h"

static _Bool insert(bst_t *t, node_t **node_pptr, const void *data_p, size_t s);
static _Bool delete(bst_t *t, node_t **node_pptr, const void *key);
static void * search(bst_t *t, node_t *node_ptr, const void *key);
static node_t * detach_min(node_t **node_pptr);

const void * get_key(const void * p) { return p; };

static node_t *
detach_min(node_t **node_pptr)
{
  node_t *node_ptr = *node_pptr; /* current node */

  if (node_ptr == NULL)
    return NULL;

  else if (node_ptr->left != NULL)
    return detach_min(&node_ptr->left);

  else
    {
      *node_pptr = node_ptr->right;
      return node_ptr;
    }
}

static _Bool
delete(bst_t *t, node_t **node_pptr, const void *key)
{
  node_t *node_ptr = *node_pptr;  /* current node */
  
  if (node_ptr == NULL) return false;

  int res = t->cmp(key, t->get_key(node_ptr->payload));

  if (res < 0)
    return delete(t, &node_ptr->left, key);
  else if (res > 0)
    return delete(t, &node_ptr->right, key);

  else /* found node to be deleted */
    {
      if (node_ptr->left == NULL)
	*node_pptr = node_ptr->right;
      else if (node_ptr->right == NULL)
	*node_pptr = node_ptr->left;
      else /* two children */
	{
	  node_t *min = detach_min(&node_ptr->right);
	  *node_pptr = min;
	  min->left = node_ptr->left;
	  min->right = node_ptr->right;
	}
      free(node_ptr);
      return true;
    }
}

static void *
search(bst_t *t, node_t *node_ptr, const void *key)
{
  if (node_ptr == NULL) return NULL;

  else
    {
      int res = t->cmp(key, t->get_key(node_ptr->payload));

      if (res == 0)	
	return node_ptr->payload;
      
      else if (res <0)	
	return search(t, node_ptr->left, key); /* let this keep searching */
      
      else	
	return search(t, node_ptr->right, key);
    }
}

static _Bool
insert(bst_t *t, node_t **node_pptr, const void *data_p, size_t s)
{
  node_t *node_ptr = *node_pptr; /* current node */

  if (node_ptr == NULL)
    {
      node_ptr = malloc(sizeof(node_t) + s); /* allocate mem for the insertion */
      
      if (node_ptr != NULL)
	{
	  node_ptr->left = NULL;
	  node_ptr->right = NULL;
	  memcpy(node_ptr->payload, data_p, s);
	  *node_pptr = node_ptr;
	  return true;
	}
      else
	return false;
    }
  else
    {
      const void *key = t->get_key(data_p),
	*root_key = t->get_key(node_ptr->payload);
      
      if (t->cmp(key, root_key)<0) /* root is always greater or equal than left node */
	{
	  log_debug(DEBUG, "left: %s<%s", (char*)key, (char*)root_key);
	  return insert(t, &node_ptr->left, data_p, s);
	}

      else
	{
	  log_debug(DEBUG, "right: %s>%s", (char*)key, (char*)root_key);	
	  return insert(t, &node_ptr->right, data_p, s);
	}
    }
}


bst_t *
new_bst(bst_cmp_t *cmp, bst_get_key_t *get)
{
  bst_t *tree = NULL;
  if (cmp != NULL)
    tree = malloc(sizeof(bst_t));
  if (tree != NULL)
    {
      tree->root = NULL;
      tree->cmp =  cmp;
      tree->get_key = (get != NULL) ? get : get_key;
    }
  return tree;
}

const void
* bst_search(bst_t *t, const void *key)
{
  if (t == NULL || key == NULL) return NULL;

  return search(t, t->root, key);
}

_Bool
bst_insert(bst_t *t, const void *payload, size_t s)
{
  if (t == NULL || payload == NULL || s == 0)
    return false;
  return insert(t, &t->root, payload, s);
}

_Bool
bst_delete(bst_t *t, const void *key)
{
  if (t == NULL || key == NULL) return false;

  return delete(t, &t->root, key);
}

void bst_swap(void)
{
}
