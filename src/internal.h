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

#include <stdio.h>
#include <stdlib.h> /* EXIT_SUCCESS and EXIT_FAILURE */
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>

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
#define CONNECT 1
#define BIND 2
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
#define  UNASSIGNED                9

 /* address type */
#define IPV4        1
#define IPV6        4
#define _DOMAINNAME 3

 /* internal event flags */
#define SINIT      0
#define SREAD      1 /* reading data that a client send */
#define SWRITE     2 /* writing data to target that a client send */
#define SWAIT      3 /* waiting for a next event*/
#define SHANG      4
#define SDESTROY   5 /* free all pending data */
#define SFINISHED  6 /* client left */
#define SCONNECTED 7

struct addrspec {
  short family;
  char *domain;
  ev_uint8_t *ipv4_addr;
  ev_uint8_t _s6_addr[16];
  ev_uint16_t port;  
  ev_uint64_t s_addr;
};

struct socks_conn {  
  struct bufferevent *bev;
  const char *auth;
};

struct addrspec * handle_addrspec(ev_uint8_t * buffer);
struct addrspec * handle_connect(struct bufferevent *bev, ev_uint8_t *buffer, ev_ssize_t evsize);

extern void debug_addr(struct addrspec *spec);

/* verbose for verbose log output */
static int verbose;

#endif
