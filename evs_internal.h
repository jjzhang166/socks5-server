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


/* simple debug mode */
#ifndef  DEBUG
#define  DEBUG 0
#endif


/* socks version  */
#define SOCKS_VERSION 5


#define SOCKSNOAUTH       0
#define GSSAPI            1
#define SOCKSAUTHPASSWORD 2
#define IANASSIGNED       3 /* curl requests this */


typedef enum {
  SUCCESSED = 0,
  GENERAL_FAILURE,
  METHOD_NOT_ALLOWED,
  NETWORK_UNREACHABLE,
  HOST_UNREACHABLE,
  CONNECTION_REFUSED,
  TTL_EXPIRED,
  METHOD_NOT_SUPPORTED,
  ADDRESS_TYPE_NOT_SUPPORTED,
  UNASSIGNED  
} socks_reply_e;  /* server replies */


 /* address type */
#define IPV4        1
#define DOMAINN     3
#define IPV6        4


/* socks status flags */
#define SREAD      1 /* reading data that a client send */
#define SWRITE     2 /* writing data to target that a client send */
#define SWAIT      3 /* waiting for a next event*/
#define SHANG      4
#define SDESTROY   5 /* free all pending data */
#define SFINISHED  6 /* client left */
#define SCONNECTED 7
#define SDNS       8
#define SINIT      9
#define DNS_OK    10

#define SOCKS_INET_ADDRSTRLEN  (sizeof("255.255.255.255") - 1)
#define SOCKS_INET6_ADDRSTRLEN \
  (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)

#endif
