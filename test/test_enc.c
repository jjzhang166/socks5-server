#include "../evs_internal.h"
#include "../evs_encryptor.h"
#include "tiny_test.h"


int
main()
{
  char *buf, *cpy;
  const char key[] = "this_is_a_good_key";
  size_t key_len = sizeof(key);

  buf = malloc(34 + 1);
  cpy = malloc(34 + 1);
  
  strcpy(buf, "This is test data to be encrypted.");
  strcpy(cpy, buf);
  
  size_t buf_len = sizeof(buf);  
  
  memcpy(cpy, buf, buf_len);
  
  encryptor(buf, buf_len, key, key_len);
  
  assert(!(buf == cpy));
  test_ok("encrypt");
  
  encryptor(buf, buf_len, key, key_len);

  assert(!(memcmp(buf, cpy, sizeof(buf))));  
  test_ok("decrypt");

  return 0;
}
