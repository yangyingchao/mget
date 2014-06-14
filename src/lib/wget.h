/* Miscellaneous declarations.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
   Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

/* This file contains declarations that are universally useful and
   those that don't fit elsewhere.  It also includes sysdep.h which
   includes some often-needed system includes, like the obnoxious
   <time.h> inclusion.  */

#ifndef WGET_H
#define WGET_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

/* Document type ("dt") flags */
enum
{
  TEXTHTML             = 0x0001,	/* document is of type text/html
                                           or application/xhtml+xml */
  RETROKF              = 0x0002,	/* retrieval was OK */
  HEAD_ONLY            = 0x0004,	/* only send the HEAD request */
  SEND_NOCACHE         = 0x0008,	/* send Pragma: no-cache directive */
  ACCEPTRANGES         = 0x0010,	/* Accept-ranges header was found */
  ADDED_HTML_EXTENSION = 0x0020,        /* added ".html" extension due to -E */
  TEXTCSS              = 0x0040	        /* document is of type text/css */
};

/* Universal error type -- used almost everywhere.  Error reporting of
   this detail is not generally used or needed and should be
   simplified.  */
typedef enum
{
  /*  0  */
  NOCONERROR, HOSTERR, CONSOCKERR, CONERROR, CONSSLERR,
  CONIMPOSSIBLE, NEWLOCATION, NOTENOUGHMEM /* ! */,
  CONPORTERR /* ! */, CONCLOSED /* ! */,
  /* 10  */
  FTPOK, FTPLOGINC, FTPLOGREFUSED, FTPPORTERR, FTPSYSERR,
  FTPNSFOD, FTPRETROK /* ! */, FTPUNKNOWNTYPE, FTPRERR, FTPREXC /* ! */,
  /* 20  */
  FTPSRVERR, FTPRETRINT, FTPRESTFAIL, URLERROR, FOPENERR,
  FOPEN_EXCL_ERR, FWRITEERR, HOK /* ! */, HLEXC /* ! */, HEOF,
  /* 30  */
  HERR, RETROK, RECLEVELEXC, FTPACCDENIED /* ! */, WRONGCODE,
  FTPINVPASV, FTPNOPASV, CONTNOTSUPPORTED, RETRUNNEEDED, RETRFINISHED,
  /* 40  */
  READERR, TRYLIMEXC, URLBADPATTERN /* ! */, FILEBADFILE /* ! */, RANGEERR,
  RETRBADPATTERN, RETNOTSUP /* ! */, ROBOTSOK /* ! */, NOROBOTS /* ! */,
  PROXERR,
  /* 50  */
  AUTHFAILED, QUOTEXC, WRITEFAILED, SSLINITFAILED, VERIFCERTERR,
  UNLINKERR, NEWLOCATION_KEEP_POST, CLOSEFAILED,

  WARC_ERR, WARC_TMP_FOPENERR, WARC_TMP_FWRITEERR
} uerr_t;


struct url;
struct address_list;

/* This struct defines an IP address, tagged with family type.  */

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

/* IP_INADDR_DATA macro returns a void pointer that can be interpreted
   as a pointer to struct in_addr in IPv4 context or a pointer to
   struct in6_addr in IPv4 context.  This pointer can be passed to
   functions that work on either, such as inet_ntop.  */
#define IP_INADDR_DATA(x) ((void *) &(x)->data)

enum {
    LH_SILENT  = 1,
    LH_BIND    = 2,
    LH_REFRESH = 4
};

typedef long long wgint;
# define str_to_wgint strtoll
#endif /* WGET_H */
