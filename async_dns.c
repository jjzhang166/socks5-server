/* 
 * evdns.c
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 *
 */

#include "internal.h"
#include "slog.h"
#include "async_dns.h"

static void A_resolvcb(int res, char type, int c, int ttl, void *addrs, void *ptr);
static void AAAA_resolvcb(int res, char type, int c, int ttl, void *addrs, void *ptr);
static void resolvecb(int errcode, struct evutil_addrinfo *ai, void *ptr);


void
resolve(struct evdns_base *dnsbase, struct dns_context *dnsctx, char *name,
	size_t nslen, const char **nameservers)
{
  int res;
  size_t i;
  
  /* register nameservers */
  for (i = 0; i < nslen; i++) {
    res = evdns_base_nameserver_ip_add(dnsbase, nameservers[i]);
    if (res <0)
      logger_err("nameservers=%s", nameservers[i]);
  }
  
  struct evutil_addrinfo hints;
  struct evdns_getaddrinfo_request *req;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_INET; /* Always prefer AF_INET */
  hints.ai_flags = EVUTIL_AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  dnsctx->name = malloc(sizeof(name)+1);

  if (dnsctx->name == NULL)
    logger_err("malloc");
  
  dnsctx->name = name;
  evdns_getaddrinfo(dnsbase, name, NULL, &hints, resolvecb, dnsctx);
}

static void
resolvecb(int errcode, struct evutil_addrinfo *ai, void *ptr)
{
  struct dns_context *dnsctx = ptr;
  int i;
  
  if (errcode) {
    logger_err("%s:%s", dnsctx->name, evutil_gai_strerror(errcode));
  } else {
    logger_info("%s", dnsctx->name);
    for (i=0; ai; ai = ai->ai_next, ++i) {
      char buf[128];
      const char *s = NULL;
      if (ai->ai_family == PF_INET) {
 	struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;
 	memcpy(&(dnsctx->sin), (struct sockaddr_in*)ai->ai_addr, sizeof(sin));	
 	s = evutil_inet_ntop(AF_INET, &(sin->sin_addr), buf, sizeof(buf));	
      }
      else if (ai->ai_family == PF_INET6) {
 	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)ai->ai_addr;
 	memcpy(&(dnsctx->sin6), (struct sockaddr_in6*)ai->ai_addr, sizeof(sin6));
 	s = evutil_inet_ntop(AF_INET6, &(sin6->sin6_addr), buf, sizeof(buf));	
      }
      if (s != NULL)
 	logger_info("     %s", s);
    }
  }
}
