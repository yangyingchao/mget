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

#include "../../log.h"
#include "../../mget_macros.h"
#include "ssl.h"
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

/* A very basic TLS client, with anonymous authentication.
 */

#define MAX_BUF 1024

void log_func(int l, const char *msg)
{
    fprintf(stderr, "Msg: %s\n", msg);
}

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

#if 0
    gnutls_global_set_log_level(10);
    gnutls_global_set_log_function(log_func);
#endif				// End of #if 0

    gnutls_certificate_allocate_credentials(&credentials);
    gnutls_certificate_set_verify_flags(credentials,
                                        GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

#if 0
    DIR *dir;
    const char *ca_directory = "/etc/ssl/certs";

    dir = opendir(ca_directory);
    if (dir == NULL) {
        logprintf(LOG_NOTQUIET, "ERROR: Cannot open directory %s.\n",
                  ca_directory);
    } else {
        struct dirent *dent;

        while ((dent = readdir(dir)) != NULL) {
            struct stat st;
            char *ca_file;

            asprintf(&ca_file, "%s/%s", ca_directory, dent->d_name);

            stat(ca_file, &st);

            if (S_ISREG(st.st_mode))
                gnutls_certificate_set_x509_trust_file(credentials,
                                                       ca_file,
                                                       GNUTLS_X509_FMT_PEM);

            free(ca_file);
        }

        closedir(dir);
    }
#endif				// End of #if 0

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

uint32 secure_socket_read(int sk, char *buf, uint32 size, void *priv)
{
    if (priv)			//@todo: need to read all content in tls buffer!!!
    {
        gnutls_session_t *session = (gnutls_session_t *) priv;

        return gnutls_record_recv(*session, buf, size);
    }

    return 0;
}

uint32 secure_socket_write(int sk, char *buf, uint32 size, void *priv)
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
        logprintf(LOG_NOTQUIET, "GnuTLS: %s\n", gnutls_strerror(err));
        goto err;
    }

    err = gnutls_handshake(*session);
    if (err < 0) {
        logprintf(LOG_NOTQUIET, "GnuTLS: %s\n", gnutls_strerror(err));
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