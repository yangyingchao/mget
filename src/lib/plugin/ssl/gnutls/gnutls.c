/** gnutls.c --- implementation of ssl using gnutls.
 *
 * Copyright (C) 2014 Yang,Ying-chao
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

#include "../../../logutils.h"
#include "../../../mget_macros.h"
#include "../ssl.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* A very basic TLS client, with anonymous authentication. */


/* Note: some of the functions private to this file have names that
   begin with "gnutls_" (e.g. gnutls_read) so that they wouldn't be
   confused with actual gnutls functions -- such as the gnutls_read
   preprocessor macro.  */

static gnutls_certificate_credentials_t credentials;
bool ssl_init()
{
    /* Becomes true if GnuTLS is initialized. */
    static bool ssl_initialized = false;

    /* GnuTLS should be initialized only once. */
    if (ssl_initialized)
        return true;

    gnutls_global_init();

    gnutls_certificate_allocate_credentials(&credentials);
    gnutls_certificate_set_verify_flags(credentials,
                                        GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

    ssl_initialized = true;

    return true;
}

static void gnutls_close(int fd, void *arg)
{
    gnutls_session_t *session = arg;

    /*gnutls_bye (ctx->session, GNUTLS_SHUT_RDWR); */
    gnutls_deinit(*session);
    /* xfree (ctx); */
    close(fd);
}

int secure_socket_read(int sk, char *buf, uint32 size, void *priv)
{
    if (priv)                   //@todo: need to read all content in tls buffer!!!
    {
        gnutls_session_t *session = (gnutls_session_t *) priv;

        return gnutls_record_recv(*session, buf, size);
    }

    return 0;
}

int secure_socket_write(int sk, char *buf, uint32 size, void *priv)
{
    if (priv) {
        gnutls_session_t *session = (gnutls_session_t *) priv;

        return gnutls_record_send(*session, buf, size);
    }
    return 0;
}

void *make_socket_secure(int sk)
{
    gnutls_session_t *session = ZALLOC1(gnutls_session_t);

    if (!session) {
        goto free;
    }

    int err = 0;

    gnutls_init(session, GNUTLS_CLIENT);

    gnutls_set_default_priority(*session);
    gnutls_credentials_set(*session, GNUTLS_CRD_CERTIFICATE, credentials);
    gnutls_transport_set_ptr(*session, (gnutls_transport_ptr_t) sk);
    /* gnutls_transport_set_int2 (*session, sk, sk); */

    int allowed_protocols[4] = { 0, 0, 0, 0 };
    allowed_protocols[0] = GNUTLS_SSL3;
    err = gnutls_protocol_set_priority(*session, allowed_protocols);

    if (err < 0) {
        mlog(NOTQUIET, "GnuTLS: (set_priority) %s\n",
             gnutls_strerror(err));
        goto err;
    }

    err = gnutls_handshake(*session);
    if (err < 0) {
        mlog(NOTQUIET, "GnuTLS: (handshake) %s\n",
             gnutls_strerror(err));
        goto err;
    } else {
        goto ret;
    }
err:
    gnutls_deinit(*session);
free:
    FIFZ(&session);
ret:
    return session;
}

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
