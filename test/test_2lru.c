#include "../evs_internal.h"
#include "../evs_lru.h"
#include "tiny_test.h"

int
main()
{
  struct payload_s p;
  lru_node_t *node, *ret;
  time_t now;

  p.key = (const char*)"key";
  p.val = (char*)"value";

  node = init_lru();
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

  block(2);
  lru_remove_oldest(&node, 1);
  purge_all(&node);
  
  return 0;
}
