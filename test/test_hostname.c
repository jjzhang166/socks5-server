#include "../evs_internal.h"
#include "../evs_dns.h"
#include "tiny_test.h"
  
static struct event_base *base;
static struct evdns_base *dnsbase;

struct regress_host {
  u8 *domain;
  u8 port[2];
  int len;
};


char *raw_ipv4s[] = {
  "172.217.27.78", "89.1460.133234"
};

char *raw_ipv6s[] = {
  "2404:6800:4004:807::200e", "2404:6800:4004:807"
};

void
test_parse_addr(void)
{
  int res;

  announce("test_parse_addr");  
  res = parse_addr(raw_ipv4s[0]);
  assert(res == 1);

  /* Should be failed */
  res = parse_addr(raw_ipv4s[1]);
  assert(res == -1);
  
  res = parse_addr(raw_ipv6s[0]);
  assert(res == 1);

    /* Should be failed */
  res = parse_addr(raw_ipv6s[1]);
  assert(res == -1);
  
  test_ok("test_parse_addr");
}

static struct regress_host hosts[] = {
  { "google.com", {1, 187}, 10 },
  { "tools.ietf.org" , {1, 187}, 14 },
  { "monkey.org", {1, 187}, 10 }
};

void
test_name(void)
{
  size_t i, hostlen = sizeof(hosts) / sizeof(hosts[0]);
  announce("test_name");

  for (i =0; i < hostlen; i++) {
    struct addrspec spec;
    if (resolve_host(hosts[i].domain, hosts[i].len, &spec) < 0)
	test_failed("resolve_host=%s", hosts[i].domain);
    else
      test_ok("resolve_host=%s", hosts[i].domain);
  }
}

struct regress_bytes_data {
  const char *name;
  u8 buffer[256];
};

struct regress_bytes_data test_data[] = {
  { "google.com",
    { 5, 0, 0, 3, 10, 103, 111, 111, 103, 108, 101, 46, 99, 111, 109 }},
  { "tools.ietf.org",
    { 5, 0, 0, 3, 14, 116, 111, 111, 108, 115, 46, 105, 101, 116, 102, 46, 111, 114, 103 }},
  { "monkey.org",
    { 5, 0, 0, 3, 10, 109, 111, 110, 107, 101, 121, 46, 111, 114, 103 }},
  { "www.wangafu.net",
    { 5, 0, 0, 3, 15, 119, 119, 119, 46, 119, 97, 110, 103, 97, 102, 117, 46, 110, 101, 116 }}
};

void
test_handle_addr(void)
{  

  size_t i, datalen = sizeof(test_data) / sizeof(test_data[0]);
  
  announce("test_handle_addr");
  
  for (i =0; i < datalen; i++) {
    if (handle_addrspec((test_data[i]).buffer) == NULL)
      test_failed("handle_addrspec");
    else
      test_ok("%s", test_data[i].name);
  }
}


const char *nameservers[] = {
  "8.8.4.4", "127.0.0.1",   "10.39.8.4"
};

/* names to be resolved */
char *names[] = {
  "github.com", "tools.ietf.org", "posteo.de",
};

void
test_resolvecb(void)
{
  
  size_t i, nslen = sizeof(nameservers) / sizeof(nameservers[0]);
  size_t namelen = sizeof(names) / sizeof(names[0]);
  int res;  
  
  announce("test_resolvecb");
  
  base = event_base_new();
  dnsbase = evdns_base_new(base, EVDNS_BASE_DISABLE_WHEN_INACTIVE); 

  for (i = 0; i < namelen; ++i) {
    struct dns_context *ctx;
    ctx = malloc(sizeof(ctx));
    ctx->name = strdup(names[i]);
    ctx->name = names[i];
    resolve(dnsbase, ctx, nslen, nameservers);
  }
}

int
main()
{
  test_parse_addr();
  test_name();
  test_handle_addr();
  test_resolvecb();
  event_base_dispatch(base);  
}
