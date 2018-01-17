#include "../evs_internal.h"
#include "../evs_helper.h"
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
  {"posteo.de", 11}
};

char *raw_ipv6s[] = {
  "2404:6800:4004:807::200e", "2404:6800:4004:807"
};

void
test_resolve_name()
{
  struct sockaddr *sa;
  socks_name_t t;  
  size_t i;
  char buf[128];

  memset(&t, 0, sizeof(t));

  for (i =0; i < sizeof(hosts)/sizeof(hosts[0]); i++) {
    
    t.host = hosts[i].host;
    t.len = hosts[i].s;
    resolve_host(&t);
    
    if (evutil_inet_ntop(AF_INET,
		    (struct sockaddr_in*)&t.sin.sin_addr, buf, sizeof(buf))
	== NULL)
      test_failed("resolve_host");

    test_ok("resolve_host => %s", buf);
    
#if SOCKS_HAVE_INET6
    if (evutil_inet_ntop(AF_INET6,
		  (struct sockaddr_in*)&t.sin6.sin6_addr, buf, sizeof(buf))
	== NULL)
      test_failed("resolve_host");    
   
    test_ok("resolve_host => %s", buf);
#endif    
  }  
}

int
main()
{
  test_resolve_name();
}
