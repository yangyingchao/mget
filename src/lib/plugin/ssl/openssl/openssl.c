/** openssl.c --- implementation of ssl using openssl.
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
#include "../../../connection.h"
#include "../../../data_utlis.h"
#include "../ssl.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

typedef struct _ssl_wrapper
{
    SSL_CTX*    ctx;
    SSL*        ssl;
    BIO*        bio;
    int         sock;
    byte_queue* bq;
} ssl_wrapper;

#define berr_exit(msg)                                  \
    do {                                                \
        mlog(LL_ALWAYS, msg ", %s",                     \
             ERR_reason_error_string(ERR_get_error())); \
        exit(EXIT_FAILURE);                             \
    } while (0)

#define CHECK_PTR(X, msg) if (!X) berr_exit(msg)
#define CHECK_W_PTR(X)    if (!X) berr_exit("Failed to create "#X)
#define PAGE       4096

static bool g_initilized = false;
bool ssl_init()
{
    if (!g_initilized)
    {
        SSL_library_init();
        SSL_load_error_strings();
        ERR_load_BIO_strings();
        OpenSSL_add_all_algorithms();
        SSLeay_add_ssl_algorithms ();

        /* SSL_CTX_set_cipher_list */
        g_initilized = true;
    }

    return true;
}

uint32 secure_socket_read(int sk, char *buf, uint32 size, void *priv)
{
    ssl_wrapper* wrapper = (ssl_wrapper*)priv;
    int ret = 0;
    bool extended = false;
retry :
    if (!timed_wait(wrapper->sock, WT_READ, -1)) {
        mlog (LL_ALWAYS, "%s: socket not ready.\n", __func__);
        return 0;
    }

read:
    if (extended) {
        int tmp = SSL_read(wrapper->ssl, buf, size);
        mlog (LL_NOTQUIET, "%d bytes will be dropped..\n", tmp);
    }
    else
        ret = SSL_read(wrapper->ssl, buf, size);

    int e   = SSL_get_error(wrapper->ssl, ret);
    switch (e) {
        case SSL_ERROR_NONE:
            break;
        case SSL_ERROR_ZERO_RETURN:
            /* End of data */
            SSL_shutdown(wrapper->ssl);
            break;
        case SSL_ERROR_WANT_READ:
            goto retry;
        case SSL_ERROR_WANT_WRITE:
            if (!timed_wait(wrapper->sock, WT_WRITE, -1)) {
                mlog (LL_ALWAYS, "%s: socket not ready for write..\n", __func__);
            }
            goto retry;
        case SSL_ERROR_SYSCALL: {
            fprintf(stderr, "buf: %p, size: %u\n", buf, size);
            if (!ERR_get_error()) {
                if (!ret) {
                    mlog (LL_ALWAYS,
                          "EOF was observed that violates the protocol.\n");
                    return -1;
                }
                else {
                    mlog (LL_ALWAYS, "BIO reported an I/O error: %s\n",
                            strerror(errno));
                }
            }
            break;
        }

        default:
            berr_exit("SSL read problem");
    }

    if (SSL_pending(wrapper->ssl))  {
        PDEBUG ("pending data, size: %d, ret: %d...\n", size, ret);
        buf += ret;
        size -= ret;
        PDEBUG ("pending data, size: %d...\n", (int)size);
        if ((int)size <= 0)
        {
            bq_enlarge(wrapper->bq, PAGE);
            buf = wrapper->bq->w;
            size = wrapper->bq->x - wrapper->bq->w;
            mlog (LL_ALWAYS, "Data save to bq\n");
            extended = true;
        }
        goto read;
    }

    return ret;
}

uint32 secure_socket_write(int sk, char *buf, uint32 size, void *priv)
{
    ssl_wrapper* wrapper = (ssl_wrapper*)priv;

    /* Try to write */
retry:
    if (!timed_wait(wrapper->sock, WT_WRITE, -1)) {
        mlog (LL_ALWAYS, "%s: socket not ready.\n", __func__);
        return 0;
    }

    int r = SSL_write(wrapper->ssl, buf, size);
    int e = SSL_get_error(wrapper->ssl, r);
    switch(SSL_get_error(wrapper->ssl, r)) {
        case SSL_ERROR_NONE:
            break;
        case SSL_ERROR_WANT_WRITE:
            goto retry;
        case SSL_ERROR_WANT_READ:
            if (!timed_wait(wrapper->sock, WT_READ, -1))  {
                mlog (LL_ALWAYS, "%s: socket not ready for reading\n",
                      __func__);
            }
            goto retry;
        default:
            berr_exit("SSL write problem");
    }

    return r;
}

void *make_socket_secure(int sock)
{
    PDEBUG ("enter with sock: %d\n", sock);

    ssl_wrapper* wrapper = ZALLOC1(ssl_wrapper);
    wrapper->ctx  = SSL_CTX_new(SSLv23_client_method());
    CHECK_W_PTR(wrapper->ctx);
    SSL_CTX_set_verify (wrapper->ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_mode (wrapper->ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);


    wrapper->ssl  = SSL_new(wrapper->ctx);
    CHECK_W_PTR(wrapper->ssl);

    wrapper->bio  = BIO_new_socket(sock, BIO_NOCLOSE);
    CHECK_W_PTR(wrapper->bio);

    wrapper->sock = sock;
    wrapper->bq = bq_init(PAGE);

    PDEBUG ("ctx: %p ssl: %p, bio: %p\n", wrapper->ctx, wrapper->ssl,
            wrapper->bio);

    SSL_set_bio(wrapper->ssl, wrapper->bio, wrapper->bio);
    /* SSL_set_session(ssl,sess);          /\*And resume it*\/ */
    int ret = 0;

retry:
    ret = SSL_connect(wrapper->ssl);
    if(ret <= 0) {
        unsigned long e = SSL_get_error(wrapper->ssl, ret);
        if (e != SSL_ERROR_WANT_READ && e!= SSL_ERROR_WANT_WRITE) {
            berr_exit("SSL connect error (second connect)");
        }

        int type = WT_NONE;
        if (e == SSL_ERROR_WANT_READ)
            type |= WT_READ;
        else if (e == SSL_ERROR_WANT_WRITE)
            type |= WT_WRITE;

        if (timed_wait(sock, type, -1))
            goto retry;

        mlog (LL_ALWAYS, "Failed to select...\n");
        abort();
    }

    /* check_cert(ssl, host); */

    PDEBUG ("ret: %p\n", wrapper);

    return wrapper;
}

void ssl_destroy(void* priv)
{
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
