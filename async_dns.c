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
resolve(struct evdns_base *dnsbase, struct dns_context *ctx, const char *name, const char *ns, ...)
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
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
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
resolvecb(int errcode, struct evutil_addrinfo *ai, void *ptr)
{
  struct dns_context *ctx = ptr;
  int i;

  if (errcode) {
    logger_err("%s:%s", ctx->name, evutil_gai_strerror(errcode));
  } else {
    logger_info("==> %s", ctx->name);
    for (i=0; ai; ai = ai->ai_next, i++) {
      char buf[128];
      if (ai->ai_family == PF_INET) {
	struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;
	evutil_inet_ntop(AF_INET, &(sin->sin_addr), buf, sizeof(buf));
      }
      else if (ai->ai_family == PF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)ai->ai_addr;
	evutil_inet_ntop(AF_INET6, &(sin6->sin6_addr), buf, sizeof(buf));
      }
    }
  }
}
