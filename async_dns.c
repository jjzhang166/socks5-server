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

static void resolvecb(int errcode, struct evutil_addrinfo *ai, void *ptr);

void
resolve(struct evdns_base *dnsbase, struct dns_context *dnsctx,size_t nslen,
	const char **nameservers)
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
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC; /* Usually hang if you take PF_INET or PF_INET6. */
  hints.ai_flags = EVUTIL_AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_UDP;
  
  if (evdns_getaddrinfo(dnsbase, dnsctx->name, NULL, &hints, resolvecb, dnsctx) == NULL)
    logger_err("%s returned immediately", dnsctx->name);
}

static void
resolvecb(int errcode, struct evutil_addrinfo *ai, void *ptr)
{
  struct dns_context *dnsctx = ptr;
   int i;
  
  if (errcode) {
    logger_err("%s:%s", dnsctx->name, evutil_gai_strerror(errcode));
  } else {
    for (i=0; ai; ai = ai->ai_next, ++i) {
      char buf[128];
      const char *s = NULL;
      if (ai->ai_family == PF_INET) {
 	struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;
 	s = evutil_inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
	dnsctx->sin = malloc(sizeof(s));
      }
      else if (ai->ai_family == PF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)ai->ai_addr;
	s = evutil_inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
	if (s != NULL)
	  dnsctx->sin6 = strdup(s);	
      }

      if (s != NULL) {  /* Make sure `s` is not NULL */
	dnsctx->sin = (char*)s;
	logger_info( "%s==>%s", dnsctx->name, dnsctx->sin);
	dnsctx->name = NULL;
	evutil_freeaddrinfo(ai);
      }
    }
  }
}
