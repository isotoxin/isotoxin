#pragma once

#ifdef _WIN32
#include "curl/include/curl/curl.h"
#endif
#ifdef _NIX
#include <curl/curl.h>
#endif // _NIX


class https_c
{
    CURL *curl = nullptr;

public:
    https_c();
    ~https_c();


    bool get(const char* address, ts::buf_c& resultad, const ts::buf_c* post = nullptr); // binary data
    bool get(const char* address, ts::buf_c& resultad, const ts::asptr &post); // key=value&key2=value2
};
