
/** netutils.h --- utility and structure related to net
 *
 * Copyright (C) 2013 Yang,Ying-chao
 *
 * Author: Yang,Ying-chao <yangyingchao@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _NETUTILS_H_
#define _NETUTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mget_types.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

typedef enum _url_protocol {
	UP_HTTP = 0,
	UP_HTTPS,
	UP_FTP,
	UP_INVALID
} url_protocol;

typedef struct {
    /* Address family, one of AF_INET or AF_INET6. */
    int family;

    /* The actual data, in the form of struct in_addr or in6_addr: */
    union {
        struct in_addr d4;		/* IPv4 address */
#ifdef ENABLE_IPV6
        struct in6_addr d6;		/* IPv6 address */
#endif
    } data;

    /* Under IPv6 getaddrinfo also returns scope_id.  Since it's
       IPv6-specific it strictly belongs in the above union, but we put
       it here for simplicity.  */
#if defined ENABLE_IPV6 && defined HAVE_SOCKADDR_IN6_SCOPE_ID
    int ipv6_scope;
#endif
} ip_address;



typedef struct _url_info {
	url_protocol eprotocol;
	uint32 port;
	char protocol[8];
	char host[64];
	char sport[8];
    ip_address* addr;
	char *bname;
	char *uri;
	char *furl;		/* full url. */
} url_info;

bool parse_url(const char *url, url_info ** ui);
void url_info_copy(url_info *, url_info *);
void url_info_display(url_info *);
void url_info_destroy(url_info ** ui);

#ifdef __cplusplus
}
#endif
#endif				/* _NETUTILS_H_ */

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * fill-column: 78
 * End:
 *
 * vim: set noet ts=4 sw=4:
 */
