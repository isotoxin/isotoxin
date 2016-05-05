#pragma once

#define _CSTDIO_
#define _FILEMGR_HXX_
//#define _SSTREAM_

#define BUFSIZE  65536
#define HZIP_EXTENSION ".hz"
#define MSG_OPEN ""

#define HUNSPELL_WARNING(...) (1,false)

//#define FILE hunspell_file_s
//#define fclose(f) f->ptr = 0
//#define fread(b,s,c,f) f->read( b, s * c )
//#define fgets(s,n,f) f->gets( s, n )
//#define _wfopen(...) nullptr
//#define fopen(...) nullptr

#include <string>

struct hunspell_file_s
{
    size_t dummy = 0; // zero terminator

    unsigned char *data;
    size_t size;
    size_t ptr = 0;
    hunspell_file_s(unsigned char *data, size_t size):data(data), size(size) {}
    operator const char *() {return (const char *)this;}

    bool gets( std::string& );
};

class FileMgr
{
    FileMgr( const FileMgr& );
    FileMgr& operator=( const FileMgr& );

    hunspell_file_s *f;
public:
    FileMgr( const char* filename, const char* key = NULL ) { f = (hunspell_file_s *)filename; }
    ~FileMgr() {}

    bool getline( std::string&s ) { return f->gets( s ); }
    int getlinenum() { return -1; }
};


#if defined _FILEMGR_HXX_
__forceinline hunspell_file_s *hunspell_file_s_open(const char* path, const char*)
{
    return (hunspell_file_s *)path;
}
#define myfopen hunspell_file_s_open
#endif

