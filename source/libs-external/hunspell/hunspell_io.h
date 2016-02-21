#pragma once

#define _CSTDIO_
#define _HUNZIP_HXX_

#define BUFSIZE  65536
#define HZIP_EXTENSION ".hz"
#define MSG_OPEN ""

#define HUNSPELL_WARNING(...) (1,false)

#define FILE hunspell_file_s
#define fclose(f) f->ptr = 0
#define fread(b,s,c,f) f->read( b, s * c )
#define fgets(s,n,f) f->gets( s, n )
#define _wfopen(...) nullptr
#define fopen(...) nullptr


// Hunzip dummy. we no need Hunzip
class Hunzip
{
public:
    Hunzip(char const *, char const *) {}
    ~Hunzip() {}
    char const * getline() { return nullptr; }
};


struct hunspell_file_s
{
    size_t dummy = 0; // zero terminator

    unsigned char *data;
    size_t size;
    size_t ptr = 0;
    hunspell_file_s(unsigned char *data, size_t size):data(data), size(size) {}
    operator const char *() {return (const char *)this;}

    size_t read( void *buffer, size_t size );
    char *gets( char *str, int n );
};

#if defined _DICTMGR_HXX_ || defined _FILEMGR_HXX_
__forceinline hunspell_file_s *hunspell_file_s_open(const char* path, const char*)
{
    return (hunspell_file_s *)path;
}
#define myfopen hunspell_file_s_open
#endif

