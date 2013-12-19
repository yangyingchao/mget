#include <stdio.h>
#include <curl/multi.h>
#include <curl/easy.h>
#include <string.h>
#include "typedefs.h"
#include "debug.h"

static uint64 sz = 0; // total size
static uint64 rz = 0; // received size;

size_t write_data(void *buffer,  size_t size, size_t nmemb,
                  void *userp)
{
    static int i = 0;
    static size_t received = 0;
    received += size*nmemb;
    printf("IDX: %08X, addr: %p, zize: %lu, nmemb: %lu, total: %lu\n",
           i++, buffer, size, nmemb, received);

    size_t got = fwrite(buffer, size, nmemb, userp);
    rz += (uint64)got;
    return got;
}

size_t cb_first_write(void *buffer,  size_t size, size_t nmemb,
                      void *userp)
{
    return size*nmemb;
}

size_t write_header(void *buffer,  size_t size, size_t nmemb,
                  void *userp)
{
    if (nmemb <= 2 || sz)
    {
        return nmemb;
    }

    const char* ptr = (const char*)buffer;
    if ((strstr(ptr, "Content-Range")) != NULL)
    {
        printf("Get range: %s\n", ptr);
        uint64 s = 0, e = 0;
        int num = sscanf(ptr, "Content-Range: bytes %Lu-%Lu/%Lu", &s, &e, &sz);
        printf ("num = %d, s: %llu, e:  %llu, t: %llu\n", num, s, e,sz);
    }
    return size*nmemb;
}


int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    FILE* fp = fopen("/tmp/testa.txt", "w");

    CURL* eh = curl_easy_init();
    const char* url =
            "https://launchpadlibrarian.net/149149704/python-mode.el-6.1.2.tar.gz";
    curl_easy_setopt(eh, CURLOPT_URL, url);
    curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(eh, CURLOPT_HEADERDATA, NULL);

    struct curl_slist* slist = NULL;
    slist = curl_slist_append(slist, "Accept: */*");
    slist = curl_slist_append(slist, "Range: bytes=0-0");
    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, slist);

    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb_first_write);
    curl_easy_setopt(eh,   CURLOPT_WRITEDATA, fp);

    PDEBUG ("Get file size ....\n");

    CURLcode ret = curl_easy_perform(eh);
    long stat = 0;
    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &stat);
    printf ("ret = %d, stat: %ld, total size: %llu\n",ret, stat, sz);

    curl_slist_free_all(slist);
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, fp);


    uint64 sg = sz/2;
    char rg[64] = {'\0'};
    sprintf(rg, "Range: bytes=0-%llu", sg-1);
    slist = curl_slist_append(NULL, "Accept: */*");
    slist = curl_slist_append(slist, rg);
    PDEBUG ("sz = %llu, Get first part: %s ...\n", sz, rg);

    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, slist);

    ret = curl_easy_perform(eh);
    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &stat);
    printf ("ret = %d, stat: %ld, total size: %llu\n",ret, stat, sz);

    curl_slist_free_all(slist);

    sprintf(rg, "Range: bytes=%llu-%llu", sg, sz);
    slist = curl_slist_append(NULL, "Accept: */*");
    slist = curl_slist_append(slist, rg);
    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, slist);

    PDEBUG ("sz = %llu, Get second part: %s ...\n", sz, rg);
    ret = curl_easy_perform(eh);
    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &stat);
    printf ("ret = %d, stat: %ld, total size: %llu\n",ret, stat, sz);
    curl_slist_free_all(slist);

    fclose(fp);
    return 0;
}
