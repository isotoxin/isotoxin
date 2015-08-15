#include "stdafx.h"

using namespace ts;


https_c::https_c()
{
    curl = curl_easy_init();
}

https_c::~https_c()
{
    if (curl) curl_easy_cleanup(curl);
}

static size_t header_callback(char *buffer,   size_t size,   size_t nitems,   void *userdata)
{
    return size * nitems;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    ts::buf_c *resultad = (ts::buf_c *)userdata;
    resultad->append_buf(ptr, size * nmemb);
    return size * nmemb;
}

bool https_c::get(const char* address, ts::buf_c& resultad, const ts::buf_c* post)
{
    if (!curl) return false;

    void *d = &resultad;

    int rslt = 0;
    rslt = curl_easy_setopt(curl, CURLOPT_URL, address);
    rslt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, d);
    rslt = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    rslt = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    rslt = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    rslt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
     
    if (post && post->size())
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, post->size());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->data());
    }

    rslt = curl_easy_perform(curl);


    return rslt == CURLE_OK;
}

bool https_c::get(const char* address, ts::buf_c& resultad, const ts::asptr &post)
{
    if (!curl) return false;

    void *d = &resultad;

    int rslt = 0;
    rslt = curl_easy_setopt(curl, CURLOPT_URL, address);
    rslt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, d);
    rslt = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    rslt = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    rslt = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    rslt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    if (post.l)
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post.l);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.s);
    }

    rslt = curl_easy_perform(curl);


    return rslt == CURLE_OK;
}
