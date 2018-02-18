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
  struct sockaddr_in *sin;
  cache = init_lru();

  memset(&t, 0, sizeof(socks_name_t*));
  t.host = "tools.ietf.org";
  t.hlen = strlen("tools.ietf.org");

  if (resolve_host(&t, &cache) < 0)
    test_failed("resolved_host");

  if (t.naddr == 0)
    test_failed("resolved_host");
  
  assert(lru_get_head(&cache) != NULL);
  test_ok("key=\"%s\"", lru_get_head(&cache)->key);
  
  if (strcmp(lru_get_tail(&cache)->key, "tools.ietf.org") != 0)
    test_failed("lru_get_tail");

  sin = (struct sockaddr_in*)t.addrs[0].sockaddr;

  assert(evutil_inet_ntop(AF_INET, &sin->sin_addr,
			  buf, sizeof(buf)) != NULL);

  test_ok("resolve_host =>%s", buf);

  t.host = "tools.ietf.org";
  t.hlen = strlen("tools.ietf.org");
  if (resolve_host(&t, &cache) < 0)
    test_failed("resolved_host");


  if (strcmp(lru_get_head(&cache)->key, "tools.ietf.org") != 0)
    test_failed("lru_get_head");
  
  if (strcmp(lru_get_tail(&cache)->key, "tools.ietf.org") != 0)
    test_failed("lru_get_tail");
  
  sin = (struct sockaddr_in*)t.addrs[0].sockaddr;
  
  if (evutil_inet_ntop(AF_INET, &sin->sin_addr,
			  buf, sizeof(buf)) == NULL)
    test_failed("cache failed");

  test_ok("cached=%s", buf);
}

int
main()
{
  (void)test_resolve_name();
  (void)purge_all(&cache); // clean up
  return 0;
}
