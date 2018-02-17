#include "../evs_internal.h"
#include "../evs_lru.h"
#include "tiny_test.h"

void
test1()
{
  struct payload_s p;
  lru_node_t *node;
  time_t now;

  node = init_lru();

  p.key = (const char*)"key";
  p.val = (char*)"value";  
  assert(node != NULL);
  
  if (lru_insert_left(&node, "first", &p, sizeof(p)))
    test_ok("insert_left");

  const char *key = lru_get_node(&node, "first", (lru_cmp_func*)strcmp)->key;
  assert(strcmp(key, "first") == 0);
  test_ok("lru_get_node");
  
  p.key = (const char*)"foo";
  p.val = (char*)"bar";

  if (lru_insert_left(&node, "second", &p, sizeof(p)))
    test_ok("insert_left");  

  lru_node_t *head = lru_get_head(&node);
  lru_node_t *tail = lru_get_tail(&node);
  assert(strcmp(head->key, "second") == 0);
  assert(strcmp(tail->key, "first") == 0);
  
  // Key doesn't exit, so returns NULL.
  assert(lru_get_node(&node, "bar", (lru_cmp_func*)strcmp) == NULL);
  
  purge_all(&node);
}

lru_node_t *node_ptr;

static void *
create_node()
{
  node_ptr = init_lru();
  return (void*)node_ptr;
}

static void
insert(lru_node_t **node, const char *key, payload_t *p)
{
  assert(lru_insert_left(node, key, p, sizeof(p)));  
}

void
test2()
{
  struct payload_s p;
  memset(&p, 0, sizeof(p));
  
  p.key = "www";
  p.val = "wwvalue";
  insert(&node_ptr, "a", &p);  
  insert(&node_ptr, "f", &p);
  insert(&node_ptr, "z", &p);
  insert(&node_ptr, "g", &p);
  insert(&node_ptr, "e", &p);
}

int main() {
  create_node();
  test2();
  purge_all(&node_ptr);  
  return 0;
}
