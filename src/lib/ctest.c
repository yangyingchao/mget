
/** ctest.c --- internal test of libmget.
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

#include <stdio.h>
#include <curl/multi.h>
#include <curl/easy.h>
#include <string.h>
#include "typedefs.h"
#include "debug.h"
#include "metadata.h"
#include "connection.h"
#include "libmget.h"
#include <unistd.h>
#include "http.h"
#include "data_utlis.h"
#include "timeutil.h"
#include <signal.h>

static uint64 sz = 0;		// total size
static uint64 rz = 0;		// received size;

void show_progress(metadata * md)
{
    static int idx = 0;
    static uint32 ts = 0;
    static uint64 last_recv = 0;

    if (md->hd.status == RS_SUCCEEDED) {
	printf("Download finished, total cost: %s....\n",
	       stringify_time(md->hd.acc_time));
	return;
    }

    int threshhold = 78;

    if (idx++ < threshhold) {
	if (!ts) {
	    ts = get_time_ms();
	}
	fprintf(stderr, ".");
    } else {
	data_chunk *dp = md->body;
	uint64 total = md->hd.package_size;
	uint64 recv = 0;

	for (int i = 0; i < md->hd.nr_chunks; ++i) {
	    recv += dp->cur_pos - dp->start_pos;
	    dp++;
	}

	uint64 diff_size = recv - last_recv;
	char *s2 = strdup(stringify_size(diff_size));
	uint32 c_time = get_time_ms();

	fprintf(stderr,
		"Progress: received %s in %.02f seconds, %.02f percent, %.02fKB/s\n",
		s2, (double) (c_time - ts) / 1000,
		(double) recv / total * 100,
		(double) (diff_size) * 1000 / K / (c_time - ts));
	idx = 0;
	last_recv = recv;
	ts = c_time;
	free(s2);
    }
}

bool control_byte = false;

void sigterm_handler(int signum)
{
    control_byte = true;
    fprintf(stderr, "Flag: %p, %d, Saving temporary data...\n",
	    &control_byte, control_byte);
}

int main(int argc, char *argv[])
{

    FILE *fp = fopen("/tmp/testa.txt", "w");
    const char *url =
	"http://mirrors.sohu.com/gentoo/distfiles/curl-7.33.0.tar.bz2";

    url_info *ui;

    if (!parse_url(url, &ui)) {
	fprintf(stderr, "Failed to parse url: %s\n", url);
    }

    url_info_display(ui);

    /* connection* sk = connection_get(ui, NULL, NULL); */
    /* printf("Socket: %p: %d\n",sk, sk? sk->sock : -1); */
    /* // Prepare to select. */
    /* write(sk->sock, "AAAAAAAAAAAAAAAA", 10); */
    /* char buf[1024]; */
    /* memset(buf, 0, 1024); */
    /* ssize_t r = read(sk->sock, buf, 1024); */
    /* printf ("r: %lu, msg: %s\n", r, buf); */

    struct sigaction act;

    act.sa_handler = sigterm_handler;
    act.sa_sigaction = NULL;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    int ret = sigaction(SIGINT, &act, NULL);

    PDEBUG("ret = %d\n", ret);

    signal(SIGINT, sigterm_handler);

    PDEBUG("cb: %p\n", &control_byte);

    process_http_request(ui, ".", 9, show_progress, &control_byte);
    fclose(fp);
    return 0;
}
