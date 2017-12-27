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

/* ns is a nameserver. */
/* Returns 0 on success and 1 on fail. */
extern int resolve_name(struct evdns_base *dnsbase, const char *name, const char *ns, ...);

static void resolvecb(int errcode, struct evutil_addrinfo *addr, void *ctx);

static int dns_pending_requests;

struct dns_context {
  int index;
  char *name;
};

#endif
