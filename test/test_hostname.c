
#include "../internal.h"
#include "../async_dns.h"
#include "tiny_test.h"

static struct event_base *base;

static struct regress_host hosts[] = {
  { "google.com", {1, 187}, 10 },
  { "tools.ietf.org" , {1, 187}, 14 },
  { "monkey.org", {1, 187}, 10 }
};

int
test_name()
{
  size_t i;
  announce("test_name");
  for (i =0; i < ARRAY_SIZE(hosts); i++)
    if (resolve_host(hosts[i].domain, hosts[i].len, NULL) < 0)
	test_failed("resolve_host=%s", hosts[i].domain);
    else
      test_ok("resolve_host=%s", hosts[i].domain);
  
  return 1;
}

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

  announce("test_handle_addr");
  
  for (i =0; i < ARRAY_SIZE(test_data); i++) {
    if (handle_addrspec((test_data[i]).buffer) == NULL)
      test_failed("handle_addrspec");
    else
      test_ok("%s", test_data[i].name);
  }
}

int test_resolvecb()
{
  size_t i;
  int res;  
  const char *names[] = {
    "google.com", "github.com", "tools.ietf.org"
  };

  announce("test_resolvecb");
  
  base = event_base_new();
  assert(base);

  struct evdns_base *dnsbase = evdns_base_new(base, EVDNS_BASE_DISABLE_WHEN_INACTIVE);
  assert(dnsbase);
  
  for (i = 0; i < ARRAY_SIZE(names);++i) {
    struct dns_context *ctx;
    ctx = malloc(sizeof(ctx));
    assert(ctx);
    resolve(dnsbase, ctx, names[i], "8.8.8.8", "8.8.4.4");
    test_ok("%s=>%s", ctx->name, ctx->v4);
  }
}

int
main()
{  
  test_name();
  test_handle_addr();
  test_resolvecb();
  event_base_dispatch(base);
}
