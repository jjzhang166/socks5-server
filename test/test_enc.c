#include "../evs_internal.h"
#include "../evs_encryptor.h"
#include "tiny_test.h"


int
main()
{
  char buf[] =
    {"This is test data to be encrypted."};
  const char key[] = "this_is_a_good_key";
  size_t buf_len = sizeof(buf);
  size_t key_len = sizeof(key);

  char cpy[buf_len];
  
  memcpy(cpy, buf, buf_len);
  
  encryptor(buf, buf_len, key, key_len);
  
  assert(!(buf == cpy));
  test_ok("encrypt");
  
  encryptor(buf, buf_len, key, key_len);

  assert(!(memcmp(buf, cpy, sizeof(buf))));  
  test_ok("decrypt");

  return 0;
}
