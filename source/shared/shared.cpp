#include "stdafx.h"
#include "curl/include/curl/curl.h"


void set_common_curl_options( CURL *curl )
{
    curl_easy_setopt( curl, CURLOPT_USERAGENT, "curl" );
    curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );

    curl_easy_setopt( curl, CURLOPT_PROXY, nullptr );
    curl_easy_setopt( curl, CURLOPT_MAXREDIRS, 50 );
    curl_easy_setopt( curl, CURLOPT_TCP_KEEPALIVE, 1 );
    curl_easy_setopt( curl, CURLOPT_USERAGENT, "curl" );

    curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 0 );
    curl_easy_setopt( curl, CURLOPT_HEADER, 0 );
    curl_easy_setopt( curl, CURLOPT_PROXY, nullptr );
    curl_easy_setopt( curl, CURLOPT_PROXYUSERPWD, nullptr );
    curl_easy_setopt( curl, CURLOPT_USERPWD, nullptr );
    curl_easy_setopt( curl, CURLOPT_KEYPASSWD, nullptr );
    curl_easy_setopt( curl, CURLOPT_RANGE, nullptr );
    curl_easy_setopt( curl, CURLOPT_HTTPPROXYTUNNEL, 0 );
    curl_easy_setopt( curl, CURLOPT_NOPROXY, nullptr );
    curl_easy_setopt( curl, CURLOPT_FAILONERROR, 0 );
    curl_easy_setopt( curl, CURLOPT_UPLOAD, 0 );
    curl_easy_setopt( curl, CURLOPT_DIRLISTONLY, 0 );
    curl_easy_setopt( curl, CURLOPT_APPEND, 0 );
    curl_easy_setopt( curl, CURLOPT_NETRC, CURL_NETRC_IGNORED );
    curl_easy_setopt( curl, CURLOPT_TRANSFERTEXT, 0 );

    //char errorbuffer[CURL_ERROR_SIZE];
    //curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuffer);
    curl_easy_setopt( curl, CURLOPT_TIMEOUT_MS, 0ull );
    curl_easy_setopt( curl, CURLOPT_UNRESTRICTED_AUTH, 0 );
    curl_easy_setopt( curl, CURLOPT_REFERER, nullptr );
    curl_easy_setopt( curl, CURLOPT_AUTOREFERER, 0 );
    curl_easy_setopt( curl, CURLOPT_HTTPHEADER, nullptr );
    curl_easy_setopt( curl, CURLOPT_POSTREDIR, 0 );
    curl_easy_setopt( curl, CURLOPT_FTPPORT, 0 );
    curl_easy_setopt( curl, CURLOPT_LOW_SPEED_LIMIT, 0 );
    curl_easy_setopt( curl, CURLOPT_LOW_SPEED_TIME, 0 );
    curl_easy_setopt( curl, CURLOPT_MAX_SEND_SPEED_LARGE, 0ull );
    curl_easy_setopt( curl, CURLOPT_MAX_RECV_SPEED_LARGE, 0ull );
    curl_easy_setopt( curl, CURLOPT_RESUME_FROM_LARGE, 0ull );
    curl_easy_setopt( curl, CURLOPT_SSLCERT, nullptr );
    curl_easy_setopt( curl, CURLOPT_SSLCERTTYPE, nullptr );
    curl_easy_setopt( curl, CURLOPT_SSLKEY, nullptr );
    curl_easy_setopt( curl, CURLOPT_SSLKEYTYPE, nullptr );
    curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0 );
    curl_easy_setopt( curl, CURLOPT_SSL_VERIFYHOST, 0 );
    curl_easy_setopt( curl, CURLOPT_SSLVERSION, 0 );
    curl_easy_setopt( curl, CURLOPT_CRLF, 0 );
    curl_easy_setopt( curl, CURLOPT_QUOTE, nullptr );
    curl_easy_setopt( curl, CURLOPT_POSTQUOTE, nullptr );
    curl_easy_setopt( curl, CURLOPT_PREQUOTE, nullptr );
    curl_easy_setopt( curl, CURLOPT_COOKIESESSION, 0 );
    curl_easy_setopt( curl, CURLOPT_TIMECONDITION, 0 );
    curl_easy_setopt( curl, CURLOPT_TIMEVALUE, 0 );
    curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, nullptr );
    //curl_easy_setopt(curl, CURLOPT_STDERR, stdout);
    curl_easy_setopt( curl, CURLOPT_INTERFACE, nullptr );
    curl_easy_setopt( curl, CURLOPT_KRBLEVEL, nullptr );
    curl_easy_setopt( curl, CURLOPT_TELNETOPTIONS, nullptr );
    curl_easy_setopt( curl, CURLOPT_RANDOM_FILE, nullptr );
    curl_easy_setopt( curl, CURLOPT_EGDSOCKET, nullptr );
    curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT_MS, 0 );
    curl_easy_setopt( curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 0 );
    curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER );
    curl_easy_setopt( curl, CURLOPT_FTP_ACCOUNT, nullptr );
    curl_easy_setopt( curl, CURLOPT_IGNORE_CONTENT_LENGTH, 0 );
    curl_easy_setopt( curl, CURLOPT_FTP_SKIP_PASV_IP, 0 );
    curl_easy_setopt( curl, CURLOPT_FTP_FILEMETHOD, 0 );
    curl_easy_setopt( curl, CURLOPT_FTP_ALTERNATIVE_TO_USER, nullptr );
}
