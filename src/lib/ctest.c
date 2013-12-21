#include <stdio.h>
#include <curl/multi.h>
#include <curl/easy.h>
#include <string.h>
#include "typedefs.h"
#include "debug.h"
#include "metadata.h"

static uint64 sz = 0; // total size
static uint64 rz = 0; // received size;

size_t write_data(void *buffer,  size_t size, size_t nmemb,
                  void *userp)
{
    static int i = 0;
    /* static size_t received = 0; */
    /* received += size*nmemb; */
    rz += size*nmemb;
    if (i++ % 10 == 0 || rz == sz)
    {
        printf("IDX: %08X, addr: %p, zize: %lu, nmemb: %lu, total: %llu\n",
               i, buffer, size, nmemb, rz);

    }
    /* size_t got = fwrite(buffer, size, nmemb, userp); */
    /* rz += (uint64)got; */
    return size*nmemb;
}

size_t drop_content(void *buffer,  size_t size, size_t nmemb,
                    void *userp)
{
    return size*nmemb;
}

size_t write_header(void *buffer,  size_t size, size_t nmemb,
                    void *userp)
{
    if (nmemb <= 2)
    {
        return nmemb;
    }

    const char* ptr = (const char*)buffer;
    if ((strstr(ptr, "Content-Range")) != NULL)
    {
        printf("Get range: %s\n", ptr);
        uint64 s = 0, e = 0;
        int num = sscanf(ptr, "Content-Range: bytes %Lu-%Lu/%Lu",
                         &s, &e, (uint64*)userp);
        printf ("num = %d, s: %llu, e:  %llu, t: %llu\n", num, s, e,sz);
    }
    return size*nmemb;
}


uint64 get_remote_file_size(const char* url, CURL* eh)
{
    uint64 size = 0;
    curl_easy_setopt(eh, CURLOPT_URL, url);
    curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(eh, CURLOPT_HEADERDATA, &size);

    struct curl_slist* slist = NULL;
    slist = curl_slist_append(slist, "Accept: */*");
    slist = curl_slist_append(slist, "Range: bytes=0-0");
    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, drop_content);

    PDEBUG ("Get file size ....\n");

    curl_easy_perform(eh);
    long stat = 0;
    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &stat);
    curl_slist_free_all(slist);
    return size;
}

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    FILE* fp = fopen("/tmp/testa.txt", "w");
    const char* url = "http://www.python.org/ftp/python/2.7.6/python-2.7.6.msi";

    sz = get_remote_file_size(url, NULL);
    PDEBUG ("TOTAL_SIZE: %lluBytes, (%.02f)M\n", sz, (float)sz/(1<<20));

    // Prepare to select.
    fclose(fp);
    return 0;
}
