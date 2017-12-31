/* 
 * evdns.h
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 *
*/

#ifndef EVDNS_H
#define EVDNS_H

#include <event2/dns.h>
#include <event2/dns_compat.h>

#include "internal.h"

struct dns_context {
  const char *name;  
  char         *v4;
  char         *v6;
};

/* ns is a nameserver. */
/* Returns 0 on success and 1 on fail. */
struct evdns_getaddrinfo_request* resolve(struct evdns_base *dnsbase, struct dns_context *ctx,
					  const char *name, const char *ns, ...);

static void resolvecb(int errcode, struct evutil_addrinfo *addr, void *ctx);

#endif
