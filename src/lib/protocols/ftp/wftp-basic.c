/* Basic FTP routines.
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

#include "c-ctype.h"
#include "log.h"
#include "wftp.h"
#include "wget.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char ftp_last_respline[128];

char *fd_read_line(ftp_connection * conn)
{
    char *line = NULL;
    if (!conn->bq) {
        conn->bq = bq_init(256);
    }

    byte_queue *bq = conn->bq;
    char *ptr = bq->r;
    char *p = NULL;
    p = memchr(bq->r, '\n', bq->w - bq->r);
    if (!p) {
        conn->bq = bq_enlarge(conn->bq, 256);
        uint32 rd = conn->conn->co.read(conn->conn, bq->w,
                                        bq->x - bq->w, NULL);
        if (rd != -1) {
            bq->w += rd;
        }

        if (bq->w > bq->r) {
            p = memchr(bq->r, '\n', bq->w - bq->r);
        }
    }

    mlog(VERBOSE, "MSG:\n%s\n", bq->r);

    if (p) {
        size_t size = p - bq->r;
        line = XALLOC(size);
        memcpy(line, bq->r, size);
        bq->r = p + 1;
    }

    return line;
}


#define fd_write(C, R, L, U)  ((C)->conn)->co.write((C)->conn, (R), (L), NULL)


/* Get the response of FTP server and allocate enough room to handle
   it.  <CR> and <LF> characters are stripped from the line, and the
   line is 0-terminated.  All the response lines but the last one are
   skipped.  The last line is determined as described in RFC959.

   If the line is successfully read, FTPOK is returned, and *ret_line
   is assigned a freshly allocated line.  Otherwise, FTPRERR is
   returned, and the value of *ret_line should be ignored.  */

uerr_t ftp_response(ftp_connection * conn, char **ret_line)
{
    while (1) {
        char *p;
        char *line = fd_read_line(conn);
        if (!line) {
            fprintf(stderr, "Failed to read line from socket."
                    " Data received so far:\n%s\n", conn->bq->p);
            return FTPRERR;
        }

        /* Strip trailing CRLF before printing the line, so that
           quotting doesn't include bogus \012 and \015. */
        p = strchr(line, '\0');
        if (p > line && p[-1] == '\n')
            *--p = '\0';
        if (p > line && p[-1] == '\r')
            *--p = '\0';
        // TODO: Remove this ifdef!
#if 0

        if (opt.server_response)
            logprintf(LOG_NOTQUIET, "%s\n",
                      quotearg_style(escape_quoting_style, line));
        else
            DEBUGP(("%s\n", quotearg_style(escape_quoting_style, line)));
#endif                          // End of #if 0

        /* The last line of output is the one that begins with "ddd ". */
        if (c_isdigit(line[0]) && c_isdigit(line[1]) && c_isdigit(line[2])
            && line[3] == ' ') {
            strncpy(ftp_last_respline, line, sizeof(ftp_last_respline));
            ftp_last_respline[sizeof(ftp_last_respline) - 1] = '\0';
            *ret_line = line;
            return FTPOK;
        }
        xfree(line);
    }
    return FTPRERR;
}

/* Returns the malloc-ed FTP request, ending with <CR><LF>, printing
   it if printing is required.  If VALUE is NULL, just use
   command<CR><LF>.  */
static char *ftp_request(const char *command, const char *value)
{
    char *res;
    if (value) {
        /* Check for newlines in VALUE (possibly injected by the %0A URL
           escape) making the callers inadvertently send multiple FTP
           commands at once.  Without this check an attacker could
           intentionally redirect to ftp://server/fakedir%0Acommand.../
           and execute arbitrary FTP command on a remote FTP server.  */
        if (strpbrk(value, "\r\n")) {
            /* Copy VALUE to the stack and modify CR/LF to space. */
            char *defanged, *p;
            STRDUP_ALLOCA(defanged, value);
            for (p = defanged; *p; p++)
                if (*p == '\r' || *p == '\n')
                    *p = ' ';
            /* DEBUGP (("\nDetected newlines in %s \"%s\"; changing to %s \"%s\"\n", */
            /*          command, quotearg_style (escape_quoting_style, value), */
            /*          command, quotearg_style (escape_quoting_style, defanged))) */
            ;
            /* Make VALUE point to the defanged copy of the string. */
            value = defanged;
        }
        res = concat_strings(command, " ", value, "\r\n", (char *) 0);
    } else
        res = concat_strings(command, "\r\n", (char *) 0);
    // TODO: Remove this ifdef!

    PDEBUG("--> %s\n", res);
    return res;
}

/* Sends the USER and PASS commands to the server, to control
   ftp_connection socket csock.  */
uerr_t ftp_login(ftp_connection * conn, const char *acc, const char *pass)
{
    uerr_t err;
    char *request, *respline;
    int nwritten;

    /* Get greeting.  */
    err = ftp_response(conn, &respline);

    if (err != FTPOK) {
        return err;
    }

    if (*respline != '2') {
        xfree(respline);
        return FTPSRVERR;
    }
    xfree(respline);
    /* Send USER username.  */
    request = ftp_request("USER", acc);
    nwritten =
        conn->conn->co.write(conn->conn, request, strlen(request), NULL);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    /* An unprobable possibility of logging without a password.  */
    if (*respline == '2') {
        xfree(respline);
        return FTPOK;
    }
    /* Else, only response 3 is appropriate.  */
    if (*respline != '3') {
        xfree(respline);
        return FTPLOGREFUSED;
    }
#ifdef ENABLE_OPIE
    {
        static const char *skey_head[] = {
            "331 s/key ",
            "331 opiekey "
        };
        size_t i;
        const char *seed = NULL;

        for (i = 0; i < countof(skey_head); i++) {
            int l = strlen(skey_head[i]);
            if (0 == strncasecmp(skey_head[i], respline, l)) {
                seed = respline + l;
                break;
            }
        }
        if (seed) {
            int skey_sequence = 0;

            /* Extract the sequence from SEED.  */
            for (; c_isdigit(*seed); seed++)
                skey_sequence = 10 * skey_sequence + *seed - '0';
            if (*seed == ' ')
                ++seed;
            else {
                xfree(respline);
                return FTPLOGREFUSED;
            }
            /* Replace the password with the SKEY response to the
               challenge.  */
            pass = skey_response(skey_sequence, seed, pass);
        }
    }
#endif                          /* ENABLE_OPIE */
    xfree(respline);
    /* Send PASS password.  */
    request = ftp_request("PASS", pass);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline != '2') {
        xfree(respline);
        return FTPLOGINC;
    }
    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

static void
ip_address_to_port_repr(const ip_address * addr, int port, char *buf,
                        size_t buflen)
{
    unsigned char *ptr;

    assert(addr->family == AF_INET);
    /* buf must contain the argument of PORT (of the form a,b,c,d,e,f). */
    assert(buflen >= 6 * 4);

    ptr = IP_INADDR_DATA(addr);
    snprintf(buf, buflen, "%d,%d,%d,%d,%d,%d", ptr[0], ptr[1],
             ptr[2], ptr[3], (port & 0xff00) >> 8, port & 0xff);
    buf[buflen - 1] = '\0';
}

/* Bind a port and send the appropriate PORT command to the FTP
   server.  Use acceptport after RETR, to get the socket of data
   ftp_connection.  */
uerr_t ftp_port(ftp_connection * conn, int *local_sock)
{
// TODO: Remove this ifdef!
#if 0

    uerr_t err;
    char *request, *respline;
    ip_address addr;
    int nwritten;
    int port;
    /* Must contain the argument of PORT (of the form a,b,c,d,e,f). */
    char bytes[6 * 4 + 1];

    /* Get the address of this side of the ftp_connection. */
    if (!socket_ip_address(conn, &addr, ENDPOINT_LOCAL))
        return FTPSYSERR;

    assert(addr.family == AF_INET);

    /* Setting port to 0 lets the system choose a free port.  */
    port = 0;

    /* Bind the port.  */
    *local_sock = bind_local(&addr, &port);
    if (*local_sock < 0)
        return FTPSYSERR;

    /* Construct the argument of PORT (of the form a,b,c,d,e,f). */
    ip_address_to_port_repr(&addr, port, bytes, sizeof(bytes));

    /* Send PORT request.  */
    request = ftp_request("PORT", bytes);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        fd_close(*local_sock);
        return WRITEFAILED;
    }
    xfree(request);

    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK) {
        fd_close(*local_sock);
        return err;
    }
    if (*respline != '2') {
        xfree(respline);
        fd_close(*local_sock);
        return FTPPORTERR;
    }
    xfree(respline);
#endif                          // End of #if 0

    return FTPOK;
}

#ifdef ENABLE_IPV6
static void
ip_address_to_lprt_repr(const ip_address * addr, int port, char *buf,
                        size_t buflen)
{
    unsigned char *ptr = IP_INADDR_DATA(addr);

    /* buf must contain the argument of LPRT (of the form af,n,h1,h2,...,hn,p1,p2). */
    assert(buflen >= 21 * 4);

    /* Construct the argument of LPRT (of the form af,n,h1,h2,...,hn,p1,p2). */
    switch (addr->family) {
    case AF_INET:
        snprintf(buf, buflen, "%d,%d,%d,%d,%d,%d,%d,%d,%d", 4, 4,
                 ptr[0], ptr[1], ptr[2], ptr[3], 2,
                 (port & 0xff00) >> 8, port & 0xff);
        break;
    case AF_INET6:
        snprintf(buf, buflen,
                 "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                 6, 16,
                 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
                 ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12],
                 ptr[13], ptr[14], ptr[15], 2, (port & 0xff00) >> 8,
                 port & 0xff);
        break;
    default:
        abort();
    }
}

/* Bind a port and send the appropriate PORT command to the FTP
   server.  Use acceptport after RETR, to get the socket of data
   ftp_connection.  */
uerr_t ftp_lprt(ftp_connection * conn, int *local_sock)
{
    uerr_t err;
    char *request, *respline;
    ip_address addr;
    int nwritten;
    int port;
    /* Must contain the argument of LPRT (of the form af,n,h1,h2,...,hn,p1,p2). */
    char bytes[21 * 4 + 1];

    /* Get the address of this side of the ftp_connection. */
    if (!socket_ip_address(conn, &addr, ENDPOINT_LOCAL))
        return FTPSYSERR;

    assert(addr.family == AF_INET || addr.family == AF_INET6);

    /* Setting port to 0 lets the system choose a free port.  */
    port = 0;

    /* Bind the port.  */
    *local_sock = bind_local(&addr, &port);
    if (*local_sock < 0)
        return FTPSYSERR;

    /* Construct the argument of LPRT (of the form af,n,h1,h2,...,hn,p1,p2). */
    ip_address_to_lprt_repr(&addr, port, bytes, sizeof(bytes));

    /* Send PORT request.  */
    request = ftp_request("LPRT", bytes);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        fd_close(*local_sock);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK) {
        fd_close(*local_sock);
        return err;
    }
    if (*respline != '2') {
        xfree(respline);
        fd_close(*local_sock);
        return FTPPORTERR;
    }
    xfree(respline);
    return FTPOK;
}

static void
ip_address_to_eprt_repr(const ip_address * addr, int port, char *buf,
                        size_t buflen)
{
    int afnum;

    /* buf must contain the argument of EPRT (of the form |af|addr|port|).
     * 4 chars for the | separators, INET6_ADDRSTRLEN chars for addr
     * 1 char for af (1-2) and 5 chars for port (0-65535) */
    assert(buflen >= 4 + INET6_ADDRSTRLEN + 1 + 5);

    /* Construct the argument of EPRT (of the form |af|addr|port|). */
    afnum = (addr->family == AF_INET ? 1 : 2);
    snprintf(buf, buflen, "|%d|%s|%d|", afnum, print_address(addr), port);
    buf[buflen - 1] = '\0';
}

/* Bind a port and send the appropriate PORT command to the FTP
   server.  Use acceptport after RETR, to get the socket of data
   ftp_connection.  */
uerr_t ftp_eprt(ftp_connection * conn, int *local_sock)
{
    uerr_t err;
    char *request, *respline;
    ip_address addr;
    int nwritten;
    int port;
    /* Must contain the argument of EPRT (of the form |af|addr|port|).
     * 4 chars for the | separators, INET6_ADDRSTRLEN chars for addr
     * 1 char for af (1-2) and 5 chars for port (0-65535) */
    char bytes[4 + INET6_ADDRSTRLEN + 1 + 5 + 1];

    /* Get the address of this side of the ftp_connection. */
    if (!socket_ip_address(conn, &addr, ENDPOINT_LOCAL))
        return FTPSYSERR;

    /* Setting port to 0 lets the system choose a free port.  */
    port = 0;

    /* Bind the port.  */
    *local_sock = bind_local(&addr, &port);
    if (*local_sock < 0)
        return FTPSYSERR;

    /* Construct the argument of EPRT (of the form |af|addr|port|). */
    ip_address_to_eprt_repr(&addr, port, bytes, sizeof(bytes));

    /* Send PORT request.  */
    request = ftp_request("EPRT", bytes);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        fd_close(*local_sock);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK) {
        fd_close(*local_sock);
        return err;
    }
    if (*respline != '2') {
        xfree(respline);
        fd_close(*local_sock);
        return FTPPORTERR;
    }
    xfree(respline);
    return FTPOK;
}
#endif

/* Similar to ftp_port, but uses `PASV' to initiate the passive FTP
   transfer.  Reads the response from server and parses it.  Reads the
   host and port addresses and returns them.  */
uerr_t ftp_pasv(ftp_connection * conn, ip_address * addr, int *port)
{
    char *request, *respline, *s;
    int nwritten, i;
    uerr_t err;
    unsigned char tmp[6];

    assert(addr != NULL);
    assert(port != NULL);

    xzero(*addr);

    /* Form the request.  */
    request = ftp_request("PASV", NULL);
    /* And send it.  */
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get the server response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline != '2') {
        xfree(respline);
        return FTPNOPASV;
    }
    /* Parse the request.  */
    s = respline;
    for (s += 4; *s && !c_isdigit(*s); s++);
    if (!*s) {
        xfree(respline);
        return FTPINVPASV;
    }
    for (i = 0; i < 6; i++) {
        tmp[i] = 0;
        for (; c_isdigit(*s); s++)
            tmp[i] = (*s - '0') + 10 * tmp[i];
        if (*s == ',')
            s++;
        else if (i < 5) {
            /* When on the last number, anything can be a terminator.  */
            xfree(respline);
            return FTPINVPASV;
        }
    }
    xfree(respline);

    addr->family = AF_INET;
    memcpy(IP_INADDR_DATA(addr), tmp, 4);
    *port = ((tmp[4] << 8) & 0xff00) + tmp[5];

    return FTPOK;
}

#ifdef ENABLE_IPV6
/* Similar to ftp_lprt, but uses `LPSV' to initiate the passive FTP
   transfer.  Reads the response from server and parses it.  Reads the
   host and port addresses and returns them.  */
uerr_t ftp_lpsv(ftp_connection * conn, ip_address * addr, int *port)
{
    char *request, *respline, *s;
    int nwritten, i, af, addrlen, portlen;
    uerr_t err;
    unsigned char tmp[16];
    unsigned char tmpprt[2];

    assert(addr != NULL);
    assert(port != NULL);

    xzero(*addr);

    /* Form the request.  */
    request = ftp_request("LPSV", NULL);

    /* And send it.  */
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);

    /* Get the server response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline != '2') {
        xfree(respline);
        return FTPNOPASV;
    }

    /* Parse the response.  */
    s = respline;
    for (s += 4; *s && !c_isdigit(*s); s++);
    if (!*s) {
        xfree(respline);
        return FTPINVPASV;
    }

    /* First, get the address family */
    af = 0;
    for (; c_isdigit(*s); s++)
        af = (*s - '0') + 10 * af;

    if (af != 4 && af != 6) {
        xfree(respline);
        return FTPINVPASV;
    }

    if (!*s || *s++ != ',') {
        xfree(respline);
        return FTPINVPASV;
    }

    /* Then, get the address length */
    addrlen = 0;
    for (; c_isdigit(*s); s++)
        addrlen = (*s - '0') + 10 * addrlen;

    if (!*s || *s++ != ',') {
        xfree(respline);
        return FTPINVPASV;
    }

    if (addrlen > 16) {
        xfree(respline);
        return FTPINVPASV;
    }

    if ((af == 4 && addrlen != 4)
        || (af == 6 && addrlen != 16)) {
        xfree(respline);
        return FTPINVPASV;
    }

    /* Now, we get the actual address */
    for (i = 0; i < addrlen; i++) {
        tmp[i] = 0;
        for (; c_isdigit(*s); s++)
            tmp[i] = (*s - '0') + 10 * tmp[i];
        if (*s == ',')
            s++;
        else {
            xfree(respline);
            return FTPINVPASV;
        }
    }

    /* Now, get the port length */
    portlen = 0;
    for (; c_isdigit(*s); s++)
        portlen = (*s - '0') + 10 * portlen;

    if (!*s || *s++ != ',') {
        xfree(respline);
        return FTPINVPASV;
    }

    if (portlen > 2) {
        xfree(respline);
        return FTPINVPASV;
    }

    /* Finally, we get the port number */
    tmpprt[0] = 0;
    for (; c_isdigit(*s); s++)
        tmpprt[0] = (*s - '0') + 10 * tmpprt[0];

    if (!*s || *s++ != ',') {
        xfree(respline);
        return FTPINVPASV;
    }

    tmpprt[1] = 0;
    for (; c_isdigit(*s); s++)
        tmpprt[1] = (*s - '0') + 10 * tmpprt[1];

    assert(s != NULL);

    if (af == 4) {
        addr->family = AF_INET;
        memcpy(IP_INADDR_DATA(addr), tmp, 4);
        *port = ((tmpprt[0] << 8) & 0xff00) + tmpprt[1];
        DEBUGP(("lpsv addr is: %s\n", print_address(addr)));
        DEBUGP(("tmpprt[0] is: %d\n", tmpprt[0]));
        DEBUGP(("tmpprt[1] is: %d\n", tmpprt[1]));
        DEBUGP(("*port is: %d\n", *port));
    } else {
        assert(af == 6);
        addr->family = AF_INET6;
        memcpy(IP_INADDR_DATA(addr), tmp, 16);
        *port = ((tmpprt[0] << 8) & 0xff00) + tmpprt[1];
        DEBUGP(("lpsv addr is: %s\n", print_address(addr)));
        DEBUGP(("tmpprt[0] is: %d\n", tmpprt[0]));
        DEBUGP(("tmpprt[1] is: %d\n", tmpprt[1]));
        DEBUGP(("*port is: %d\n", *port));
    }

    xfree(respline);
    return FTPOK;
}

/* Similar to ftp_eprt, but uses `EPSV' to initiate the passive FTP
   transfer.  Reads the response from server and parses it.  Reads the
   host and port addresses and returns them.  */
uerr_t ftp_epsv(ftp_connection * conn, ip_address * ip, int *port)
{
    char *request, *respline, *start, delim, *s;
    int nwritten, i;
    uerr_t err;
    int tport;

    assert(ip != NULL);
    assert(port != NULL);

    /* IP already contains the IP address of the control ftp_connection's
       peer, so we don't need to call socket_ip_address here.  */

    /* Form the request.  */
    /* EPSV 1 means that we ask for IPv4 and EPSV 2 means that we ask for IPv6. */
    request = ftp_request("EPSV", (ip->family == AF_INET ? "1" : "2"));

    /* And send it.  */
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);

    /* Get the server response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline != '2') {
        xfree(respline);
        return FTPNOPASV;
    }

    assert(respline != NULL);

    DEBUGP(("respline is %s\n", respline));

    /* Skip the useless stuff and get what's inside the parentheses */
    start = strchr(respline, '(');
    if (start == NULL) {
        xfree(respline);
        return FTPINVPASV;
    }

    /* Skip the first two void fields */
    s = start + 1;
    delim = *s++;
    if (delim < 33 || delim > 126) {
        xfree(respline);
        return FTPINVPASV;
    }

    for (i = 0; i < 2; i++) {
        if (*s++ != delim) {
            xfree(respline);
            return FTPINVPASV;
        }
    }

    /* Finally, get the port number */
    tport = 0;
    for (i = 1; c_isdigit(*s); s++) {
        if (i > 5) {
            xfree(respline);
            return FTPINVPASV;
        }
        tport = (*s - '0') + 10 * tport;
    }

    /* Make sure that the response terminates correcty */
    if (*s++ != delim) {
        xfree(respline);
        return FTPINVPASV;
    }

    if (*s != ')') {
        xfree(respline);
        return FTPINVPASV;
    }

    *port = tport;

    xfree(respline);
    return FTPOK;
}
#endif

/* Sends the TYPE request to the server.  */
uerr_t ftp_type(ftp_connection * conn, int type)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;
    char stype[2];

    /* Construct argument.  */
    stype[0] = type;
    stype[1] = 0;
    /* Send TYPE request.  */
    request = ftp_request("TYPE", stype);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline != '2') {
        xfree(respline);
        return FTPUNKNOWNTYPE;
    }
    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* Changes the working directory by issuing a CWD command to the
   server.  */
uerr_t ftp_cwd(ftp_connection * conn, const char *dir)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;

    /* Send CWD request.  */
    request = ftp_request("CWD", dir);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline == '5') {
        xfree(respline);
        return FTPNSFOD;
    }
    if (*respline != '2') {
        xfree(respline);
        return FTPRERR;
    }
    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* Sends REST command to the FTP server.  */
uerr_t ftp_rest(ftp_connection * conn, wgint offset)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;

    request = ftp_request("REST", number_to_static_string(offset));
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline != '3') {
        xfree(respline);
        return FTPRESTFAIL;
    }
    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* Sends RETR command to the FTP server.  */
uerr_t ftp_retr(ftp_connection * conn, const char *file)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;

    /* Send RETR request.  */
    request = ftp_request("RETR", file);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline == '5') {
        xfree(respline);
        return FTPNSFOD;
    }
    if (*respline != '1') {
        xfree(respline);
        return FTPRERR;
    }
    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* Sends the LIST command to the server.  If FILE is NULL, send just
   `LIST' (no space).  */
uerr_t ftp_list(ftp_connection * conn, const char *file, enum stype rs)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;
    bool ok = false;
    size_t i = 0;
    /* Try `LIST -a' first and revert to `LIST' in case of failure.  */
    const char *list_commands[] = { "LIST -a",
        "LIST"
    };

    /* 2008-01-29  SMS.  For a VMS FTP server, where "LIST -a" may not
       fail, but will never do what is desired here, skip directly to the
       simple "LIST" command (assumed to be the last one in the list).
     */
    if (rs == ST_VMS)
        i = countof(list_commands) - 1;

    do {
        /* Send request.  */
        request = ftp_request(list_commands[i], file);
        nwritten = fd_write(conn, request, strlen(request), -1);
        if (nwritten < 0) {
            xfree(request);
            return WRITEFAILED;
        }
        xfree(request);
        /* Get appropriate response.  */
        err = ftp_response(conn, &respline);
        if (err == FTPOK) {
            if (*respline == '5') {
                err = FTPNSFOD;
            } else if (*respline == '1') {
                err = FTPOK;
                ok = true;
            } else {
                err = FTPRERR;
            }
            xfree(respline);
        }
        ++i;
    } while (i < countof(list_commands) && !ok);

    return err;
}

/* Sends the SYST command to the server. */
uerr_t ftp_syst(ftp_connection * conn, enum stype * server_type)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;

    /* Send SYST request.  */
    request = ftp_request("SYST", NULL);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);

    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline == '5') {
        xfree(respline);
        return FTPSRVERR;
    }

    /* Skip the number (215, but 200 (!!!) in case of VMS) */
    strtok(respline, " ");

    /* Which system type has been reported (we are interested just in the
       first word of the server response)?  */
    request = strtok(NULL, " ");

    if (request == NULL)
        *server_type = ST_OTHER;
    else if (!strcasecmp(request, "VMS"))
        *server_type = ST_VMS;
    else if (!strcasecmp(request, "UNIX"))
        *server_type = ST_UNIX;
    else if (!strcasecmp(request, "WINDOWS_NT")
             || !strcasecmp(request, "WINDOWS2000"))
        *server_type = ST_WINNT;
    else if (!strcasecmp(request, "MACOS"))
        *server_type = ST_MACOS;
    else if (!strcasecmp(request, "OS/400"))
        *server_type = ST_OS400;
    else
        *server_type = ST_OTHER;

    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* Sends the PWD command to the server. */
uerr_t ftp_pwd(ftp_connection * conn, char **pwd)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;

    /* Send PWD request.  */
    request = ftp_request("PWD", NULL);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK)
        return err;
    if (*respline == '5') {
      err:
        xfree(respline);
        return FTPSRVERR;
    }

    /* Skip the number (257), leading citation mark, trailing citation mark
       and everything following it. */
    strtok(respline, "\"");
    request = strtok(NULL, "\"");
    if (!request)
        /* Treat the malformed response as an error, which the caller has
           to handle gracefully anyway.  */
        goto err;

    /* Has the `pwd' been already allocated?  Free! */
    xfree_null(*pwd);

    *pwd = xstrdup(request);

    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* Sends the SIZE command to the server, and returns the value in 'size'.
 * If an error occurs, size is set to zero. */
uerr_t ftp_size(ftp_connection * conn, const char *file, wgint * size)
{
    char *request, *respline;
    int nwritten;
    uerr_t err;

    /* Send PWD request.  */
    request = ftp_request("SIZE", file);
    nwritten = fd_write(conn, request, strlen(request), -1);
    if (nwritten < 0) {
        xfree(request);
        *size = 0;
        return WRITEFAILED;
    }
    xfree(request);
    /* Get appropriate response.  */
    err = ftp_response(conn, &respline);
    if (err != FTPOK) {
        *size = 0;
        return err;
    }
    if (*respline == '5') {
        /*
         * Probably means SIZE isn't supported on this server.
         * Error is nonfatal since SIZE isn't in RFC 959
         */
        xfree(respline);
        *size = 0;
        return FTPOK;
    }

    errno = 0;
    *size = str_to_wgint(respline + 4, NULL, 10);
    if (errno) {
        /*
         * Couldn't parse the response for some reason.  On the (few)
         * tests I've done, the response is 213 <SIZE> with nothing else -
         * maybe something a bit more resilient is necessary.  It's not a
         * fatal error, however.
         */
        xfree(respline);
        *size = 0;
        return FTPOK;
    }

    xfree(respline);
    /* All OK.  */
    return FTPOK;
}

/* If URL's params are of the form "type=X", return character X.
   Otherwise, return 'I' (the default type).  */
char ftp_process_type(const char *params)
{
    if (params && 0 == strncasecmp(params, "type=", 5)
        && params[5] != '\0')
        return c_toupper(params[5]);
    else
        return 'I';
}



char *concat_strings(const char *str0, ...)
{
    va_list args;
    int saved_lengths[5];       /* inspired by Apache's apr_pstrcat */
    char *ret, *p;

    const char *next_str;
    int total_length = 0;
    size_t argcount;

    /* Calculate the length of and allocate the resulting string. */

    argcount = 0;
    va_start(args, str0);
    for (next_str = str0; next_str != NULL;
         next_str = va_arg(args, char *)) {
        int len = strlen(next_str);
        if (argcount < countof(saved_lengths))
            saved_lengths[argcount++] = len;
        total_length += len;
    }
    va_end(args);
    p = ret = XALLOC(total_length + 1);

    /* Copy the strings into the allocated space. */

    argcount = 0;
    va_start(args, str0);
    for (next_str = str0; next_str != NULL;
         next_str = va_arg(args, char *)) {
        int len;
        if (argcount < countof(saved_lengths))
            len = saved_lengths[argcount++];
        else
            len = strlen(next_str);
        memcpy(p, next_str, len);
        p += len;
    }
    va_end(args);
    *p = '\0';

    return ret;
}

char *number_to_static_string(wgint number)
{
    char *buf = NULL;
    masprintf(&buf, "%llu", number);
    return buf;
}
