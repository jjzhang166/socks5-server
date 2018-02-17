#include "../evs_internal.h"
#include "../evs_helper.h"
#include "../evs_lru.h"
#include "tiny_test.h"

char *raw_ipv4s[] = {
  "172.217.27.78", "89.1460.133234"
};

struct host_s {
  char *host;
  size_t s;
  short port;
};

/* names to be resolved */
struct host_s hosts[] = {
  {"github.com", 11, 80},
  {"tools.ietf.org", 15, 80},
  {"posteo.de", 11, 80},
  {"google.com", 10, 80}
};

char *raw_ipv6s[] = {
  "2404:6800:4004:807::200e", "2404:6800:4004:807"
};

static lru_node_t *cache;

void
test_resolve_name()
{
  socks_name_t t, f;
  size_t i;
  char buf[128];
  cache = init_lru();

  memset(&t, 0, sizeof(socks_name_t*));
  t.host = "tools.ietf.org";
  t.len = strlen("tools.ietf.org");
  
  if (resolve_host(&t, &cache) < 0)
    test_failed("resolved_host");
  
  assert(lru_get_head(&cache) != NULL);
  test_ok("key=\"%s\"", lru_get_head(&cache)->key);
  
  assert(strcmp(lru_get_tail(&cache)->key, "tools.ietf.org") == 0);

  assert(evutil_inet_ntop(AF_INET, (struct sockaddr*)&t.sin.sin_addr,
			  buf, sizeof(buf)) != NULL);
  
  test_ok("resolve_host =>%s", buf);

  f.host = "tools.ietf.org";
  f.len = strlen("tools.ietf.org");
  if (resolve_host(&f, &cache) < 0)
    test_failed("resolved_host");

  assert(evutil_inet_ntop(AF_INET, (struct sockaddr*)&f.sin.sin_addr,
			  buf, sizeof(buf)) != NULL);  
  test_ok("resolve_host =>%s", buf);
    
  test_ok("head=\"%s\"", lru_get_head(&cache)->key);
  test_ok("tail=\"%s\"", lru_get_tail(&cache)->key);  
}

int
main()
{
  (void)test_resolve_name();
  (void)purge_all(&cache);
  return 0;
}
