#include "../evs_internal.h"
#include "../evs_lru.h"
#include "tiny_test.h"

int
main()
{
  struct payload_s p1, p2, p3, p4;
  lru_node_t *node, *ret;

  p1.key = (const char*)"key";
  p1.val = (char*)"value";
  node = init_lru(&p1, sizeof(p1));

  assert(node != NULL);

  assert(
         strcmp("key", lru_get_head(&node)->payload_ptr->key)
         == 0);

  p2.key = (const char*)"what";
  p2.val = (char*)"baz";
  assert(lru_insert_left(&node, &p2, sizeof(p2)) != false);
  test_ok("insert \"%s\" to left", (const char*)node->payload_ptr->key);
  test_ok("prev key is \"%s\"", (const char*)node->prev->payload_ptr->key);

  assert(strcmp("what", lru_get_head(&node)->payload_ptr->key) == 0);

  assert(strcmp(lru_get_tail(&node)->payload_ptr->key, "key") == 0);

  p3.key = (const char*)"doo";
  p3.val = (char*)"bar";
  assert(lru_insert_left(&node, &p3, sizeof(p3)) != false);
  test_ok("insert \"%s\" to left", (const char*)node->payload_ptr->key);

  assert(strcmp("doo", lru_get_head(&node)->payload_ptr->key)== 0);

  assert(strcmp(lru_get_node(&node, "doo",
                             (lru_cmp_func*)strcmp)->payload_ptr->key, "doo") == 0);
  assert(strcmp(lru_get_node(&node, "what",
                             (lru_cmp_func*)strcmp)->payload_ptr->key, "what") == 0);
  assert(strcmp(lru_get_node(&node, "key",
                             (lru_cmp_func*)strcmp)->payload_ptr->key, "key") == 0);

  assert(strcmp(lru_get_head(&node)->payload_ptr->key, "key") == 0);
  
  assert(strcmp(lru_get_tail(&node)->payload_ptr->key, "doo") == 0);

  purge_all(&node);
  
  return 0;
}
