
#include "assert.h"
#include "../internal.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

struct regress_host {
  u8 *domain;
  u8 port[2];
  int len;
};

static struct regress_host hosts[] = {
  { "google.com", {1, 187}, 10 },
  { "tools.ietf.org" , {1, 187}, 14 },
  { "monkey.org", {1, 187}, 10 }
};
int
test_name()
{
  size_t i;
  
  for (i =0; i < ARRAY_SIZE(hosts); i++)
    assert(resolve_host(hosts[i].domain, hosts[i].len, NULL));
  fprintf(stdout, "OK: test_name\n");
  return 1;
}

struct regress_bytes_data {
  const char *name;
  u8 buffer[256];
};

int test_handle_addr()
{
  size_t i;
  struct regress_bytes_data test_data[] = {
    { "google.com",
      { 5, 0, 0, 3, 10, 103, 111, 111, 103, 108, 101, 46, 99, 111, 109 }}, /* google.com */
    { "tools.ietf.org",
      { 5, 0, 0, 3, 14, 116, 111, 111, 108, 115, 46, 105, 101, 116, 102, 46, 111, 114, 103 }}, /* tools.ietf.org */    
    { "monkey.org",
      { 5, 0, 0, 3, 10, 109, 111, 110, 107, 101, 121, 46, 111, 114, 103 }}, /* monkey.org */
    { "www.wangafu.net",
      { 5, 0, 0, 3, 15, 119, 119, 119, 46, 119, 97, 110, 103, 97, 102, 117, 46, 110, 101, 116 }} /* www.wangafu.net */
  };

  for (i =0; i < ARRAY_SIZE(test_data); i++) {
    assert(handle_addrspec((test_data[i]).buffer));
  }
  fprintf(stdout, "OK: test_handle_addr\n");
}

int
main()
{
  test_name();
  test_handle_addr();
}
