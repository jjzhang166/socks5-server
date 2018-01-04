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
  char                *name;  
  struct socksaddr_in *sin;
  struct sockaddr_in6 *sin6;
};

/* ns is a nameserver. */
/* Returns 0 on success and 1 on fail. */
void resolve(struct evdns_base *dnsbase, struct dns_context *ctx,
					  char *name, size_t nslen, const char **nameservers);
#endif
