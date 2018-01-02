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

struct evdns_getaddrinfo_request* 
resolve(struct evdns_base *dnsbase, struct dns_context *ctx, char *name,
	size_t nslen, const char **nameservers)
{
  int res;
  size_t i;
  
  /* register nameservers */
  for (i = 0; i < nslen; i++) {
    res = evdns_base_nameserver_ip_add(dnsbase, nameservers[i]);
    if (res<0) {
      logger_err("nameservers=%s", nameservers[i]);
    }
  }
  
  struct evutil_addrinfo hints;
  struct evdns_getaddrinfo_request *req;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_INET; /* Always prefer AF_INET */
  hints.ai_flags = EVUTIL_AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  ctx->name = malloc(sizeof(name)+1);

  if (ctx->name == NULL)
    logger_err("malloc");
  
  ctx->name = name;

  return evdns_getaddrinfo(dnsbase, name, NULL, &hints, resolvecb, ctx);
}

static void
resolvecb(int errcode, struct evutil_addrinfo *addr, void *ptr)
{
  struct dns_context *ctx = ptr;
  int i;

  if (errcode) {
    logger_err("%s:%s", ctx->name, evutil_gai_strerror(errcode));
  } else {
    struct evutil_addrinfo *ai;
    logger_info("%s.", ctx->name);
    for (ai =addr; ai = ai->ai_next;) {
      char buf[128];
      const char *s = NULL;
      if (ai->ai_family == PF_INET) {
	struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;
	memcpy(&(ctx->sin), (struct sockaddr_in*)ai->ai_addr, sizeof(sin));	
	s = evutil_inet_ntop(AF_INET, &(sin->sin_addr), buf, sizeof(buf));	
      }
      else if (ai->ai_family == PF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)ai->ai_addr;
	memcpy(&(ctx->sin6), (struct sockaddr_in6*)ai->ai_addr, sizeof(sin6));
	s = evutil_inet_ntop(AF_INET6, &(sin6->sin6_addr), buf, sizeof(buf));	
      }
      if (s != NULL)
	logger_info("     %s", s);
    }
    evutil_freeaddrinfo(addr);
    free(ctx);
  }
}
