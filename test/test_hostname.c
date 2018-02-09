#include "../evs_internal.h"
#include "../evs_helper.h"
#include "../evs_bst.h"
#include "tiny_test.h"

char *raw_ipv4s[] = {
  "172.217.27.78", "89.1460.133234"
};

struct host_s {
  char   *host;
  size_t    s;
};

/* names to be resolved */
struct host_s hosts[] = {
  {"github.com", 11},
  {"tools.ietf.org", 15},
  {"posteo.de", 11},
  {"google.com", 10}
};

char *raw_ipv6s[] = {
  "2404:6800:4004:807::200e", "2404:6800:4004:807"
};

void
test_resolve_name()
{
  socks_name_t t;
  size_t i;
  char buf[128];
  
  t.host = "google.com";
  t.len = strlen("google.com");
  (void)resolve_host(&t);
  assert(evutil_inet_ntop(AF_INET, (struct sockaddr*)&t.sin.sin_addr,
			  buf, sizeof(buf)) != NULL);
  test_ok("resolve_host");
}

int
main()
{
  test_resolve_name();
}
