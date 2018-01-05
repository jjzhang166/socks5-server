/* 
 * internal.h
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 *
*/


#ifndef INTERNAL_H
#define INTERNAL_H

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/util.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define u64 ev_uint64_t
#define u32 ev_uint32_t
#define u16 ev_uint16_t
#define u8   ev_uint8_t


/* socks version  */
#define SOCKS_VERSION 5

/* 
 * socks auth password protocol
 *
 * X'00' NO AUTHENTICATION REQUIRED
 * X'01' GSSAPI
 * X'02' USERNAME/PASSWORD
 * X'03' to X'7F' IANA ASSIGNED
 * X'80' to X'FE' RESERVED FOR PRIVATE METHODS
 * X'FF' NO ACCEPTABLE METHODS
*/

#define SOCKSNOAUTH       0
#define GSSAPI            1
#define SOCKSAUTHPASSWORD 2
#define IANASSIGNED       3 /* curl requests this */


/* socks commands */
#define CONNECT  1
#define BIND     2
#define UDPASSOC 3

 /* server replies */
#define SUCCESSED                  0
#define GENERAL_FAILURE            1
#define NOT_ALLOWED                2
#define NETWORK_UNREACHABLE        3
#define HOST_UNREACHABLE           4
#define REFUSED                    5
#define TTL_EXPIRED                6
#define NOT_SUPPORTED              7
#define ADDRESS_TYPE_NOT_SUPPORTED 8
#define UNASSIGNED                 9

 /* address type */
#define IPV4        1
#define IPV6        4
#define _DOMAINNAME 3

 /* internal event flags */
#define SINIT      9
#define SREAD      1 /* reading data that a client send */
#define SWRITE     2 /* writing data to target that a client send */
#define SWAIT      3 /* waiting for a next event*/
#define SHANG      4
#define SDESTROY   5 /* free all pending data */
#define SFINISHED  6 /* client left */
#define SCONNECTED 7
#define SDNS       8

#define SOCKS_INET_ADDRSTRLEN  (sizeof("255.255.255.255") - 1)
#define SOCKS_INET6_ADDRSTRLEN \
  (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)

struct addrspec {
  short   family;
  char   *domain;
  u8      sin6_addr[SOCKS_INET6_ADDRSTRLEN];
  u16     port;
  u64     s_addr;
};

/* verbose for verbose log output */
static int verbose;

struct addrspec * handle_addrspec(u8 * buffer);

extern void debug_addr(struct addrspec *spec);

extern u8 * cpystrn(u8 *dst, u8 *src, size_t s);

extern int resolve_host(char *host, int len, struct addrspec *spec);

#endif
