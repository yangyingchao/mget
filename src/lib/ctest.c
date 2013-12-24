#include <stdio.h>
#include <curl/multi.h>
#include <curl/easy.h>
#include <string.h>
#include "typedefs.h"
#include "debug.h"
#include "metadata.h"
#include "mget_sock.h"
#include "libmget.h"
#include "mget_sock.h"
#include <unistd.h>
#include "mget_http.h"

static uint64 sz = 0; // total size
static uint64 rz = 0; // received size;

extern int dissect_header(const char* buffer, size_t length, size_t* dsize, hash_table** ht);

typedef struct _crs_param
{
    void*       addr;                   //base addr;
    data_chunk* dp;
    bool        header_finished;
    hash_table* ht;
} crs_param;

int ctest_read_sock(int sock, void* priv)
{
    if (!priv)
    {
        return -1;
    }

    crs_param* param = (crs_param*) priv;
    data_chunk* dp = (data_chunk*)param->dp;
    void* addr = param->addr + dp->cur_pos;

    if (!param->header_finished)
    {
        char buf[4096] = {'\0'};
        memset(buf, 0, 4096);
        size_t rd = read(sock, buf, 4095);
        if (rd)
        {
            size_t length = strlen(buf);
            char* ptr = strstr(buf, "\r\n\r\n");
            if (ptr != NULL)
            {
                length = ptr - buf;
                param->header_finished = true;
            }

            size_t d;
            int r = dissect_header(buf, length, &d, &param->ht);
            if (r != 206)
            {
                fprintf(stderr, "status code is %d!\n", r);
                exit(1);
            }

            if (param->header_finished)
            {
                size_t sz = strlen(buf)-length;
                memcpy(addr, buf+length, sz);
                dp->cur_pos += sz;
            }
        }
    }


    size_t rd = read(sock, param->addr+dp->cur_pos, dp->end_pos - dp->cur_pos);
    if (rd != -1)
    {
        dp->end_pos += rd;
    }

    PDEBUG ("Sock: %d, gets %lu bytes...\n", sock, rd);
    return rd;
}

typedef struct _ctest_param
{
    url_info* ui;
    data_chunk* dp;
} ctest_param;


int ctest_write_sock(int sock, void* priv)
{
    if (!priv)
    {
        return -1;
    }

    ctest_param* cp = (ctest_param*)priv;
    data_chunk*  dp = cp->dp;
    url_info*    ui = cp->ui;

    char buffer[4096];
    memset(buffer, 0, 2096);

    sprintf(buffer,
            "%s %s HTTP/1.1\r\nHost: %s\r\nAccept: *\r\nRange: bytes=%llu-%llu\r\n\r\n",
            "GET", ui->uri, ui->host, dp->start_pos, dp->end_pos);

    size_t written = write(sock, buffer, strlen(buffer));
    PDEBUG ("Sock: %d, gets %lu bytes...\n", sock, written);
    return written;
}

int main(int argc, char *argv[])
{

    FILE* fp = fopen("/tmp/testa.txt", "w");
    const char* url = "http://mirrors.sohu.com/gentoo/distfiles/curl-7.33.0.tar.bz2";

    url_info* ui;
    if (!parse_url(url, &ui))
    {
        fprintf(stderr, "Failed to parse url: %s\n", url);
    }

    url_info_display(ui);

    /* msock* sk = socket_get(ui, NULL, NULL); */
    /* printf("Socket: %p: %d\n",sk, sk? sk->sock : -1); */
    /* // Prepare to select. */
    /* write(sk->sock, "AAAAAAAAAAAAAAAA", 10); */
    /* char buf[1024]; */
    /* memset(buf, 0, 1024); */
    /* ssize_t r = read(sk->sock, buf, 1024); */
    /* printf ("r: %lu, msg: %s\n", r, buf); */

    bool stop = false;
    process_http_request(ui, "/tmp", 9, NULL, &stop);
    fclose(fp);
    return 0;
}
