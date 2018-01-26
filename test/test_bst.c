#include "../evs_internal.h"
#include "../evs_bst.h"
#include "../evs_helper.h"
#include "tiny_test.h"


int
main()
{
  /* initialize a tree */  
  bst_t *tree = new_bst((bst_cmp_t*)strcmp, NULL);

  if (!(bst_insert(tree, "root", strlen("root")))) /* init */
    test_failed("insert");
  
  assert(bst_insert(tree, "buz", strlen("buz")));
  assert(bst_insert(tree, "doo", strlen("doo")));
  
  test_ok("bst_insert");

  assert(bst_delete(tree, "doo"));
  if (bst_search(tree, "doo") != NULL)
    test_failed("delete");
  
  if ("search %s", bst_search(tree, "foo")) /* doesn't exist */
    test_failed("search");

  if (!(bst_search(tree, "buz")))
    test_failed("search");
  test_ok("search");
}
