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

int
resolve_name(struct evdns_base *dnsbase, const char *name, const char *ns, ...)
{
  va_list ap;
  int res;

  /* Register nameservers */
  va_start(ap, ns);
  res = evdns_base_nameserver_ip_add(dnsbase, ns);
  if (res<0) {
    logger_err("nameservers=%s", ns);
  }
  va_end(ap);
  
  struct evutil_addrinfo hints;
  struct evdns_getaddrinfo_request *req;
  struct dns_context *ctx;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = EVUTIL_AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  
  if (!(ctx = malloc(sizeof(ctx))))
    logger_err("malloc");
  
  ctx->index = dns_pending_requests;
  
  if (!(ctx->name = strdup(name)))
    logger_err("malloc");    
  
  ++dns_pending_requests;

  req = evdns_getaddrinfo(dnsbase, name, NULL, &hints, resolvecb, ctx);
  if (req == NULL)
    return 1;
  
  return 0;
}

static void
resolvecb(int errcode, struct evutil_addrinfo *addr, void *ctx)
{
  struct dns_context *dnsctx = ctx;

  if (errcode) {
    logger_err("%s:%s", dnsctx->name, evutil_gai_strerror(errcode));
  } else {
    struct evutil_addrinfo *ai;
    logger_info("%d. %s", dnsctx->index, dnsctx->name);
    for (ai = addr; ai; ai->ai_next) {
      char buf[128];
      const char *s = NULL;
      if (ai->ai_family == AF_INET) {
	struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;
	s = evutil_inet_ntop(AF_INET, &sin->sin_addr, buf, 128);
	fprintf(stdout, " ----> %s\n", s);
      }
      else if (ai->ai_family == AF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)ai->ai_addr;
	s = evutil_inet_ntop(AF_INET6, &sin6->sin6_addr, buf, 128);	
	fprintf(stdout, " ----> %s\n", s);
      }
    }
    evutil_freeaddrinfo(addr);
  }
  free(dnsctx->name);
  free(dnsctx);
}
