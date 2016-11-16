#pragma once

#define HUNSPELL_STATIC
#define LIBHUNSPELL_DLL_EXPORTED

#define _CSTDIO_
#define FILEMGR_HXX_

#define BUFSIZE  65536
#define HZIP_EXTENSION ".hz"
#define MSG_OPEN ""

#define HUNSPELL_WARNING(...) (1,false)

#include <string>

struct hunspell_file_s
{
    size_t dummy = 0; // zero terminator

    const unsigned char *data;
    size_t size;
    hunspell_file_s( const unsigned char *data, size_t size ) :data( data ), size( size ) {}
    operator const char *( ) { return ( const char * )this; }

    bool gets( size_t&ptr, std::string&, char breakchar = 0 );
};

namespace std
{
    // WHY YOU USE THESE FAKE STREAMS?
    // BECAUSE MICROSOFT std::*stream CLASSES SO UGLY!!!
    // deinitialization of internal stuff on exit can cause crash inside custom (dlmalloc) memory free function

    template<typename T1, typename T2> class basic_ifstream
    {
    public:
        void open( ... ) {}
    };

    template<typename T1, typename T2, typename T3> class basic_ostringstream
    {
        string s;
    public:
        //basic_ostringstream() {}
        //basic_ostringstream( const string &s ):s(s) {}
        const string &str() const { return s; }
        basic_ostringstream &operator<<( unsigned short f )
        {
            char ss[ 32 ];
            _itoa_s(f, ss, 31, 10);
            s.append(ss);
            return *this;
        }
    };
    template<typename T1, typename T2, typename T3> class basic_stringstream
    {
    public:
        hunspell_file_s f;
        size_t ptr = 0;
        basic_stringstream( const string &s ) :f( (const unsigned char *)s.c_str(), s.length() )
        {
        }
    };

    inline bool getline( stringstream& strem, string &s, char breakchar )
    {
        return strem.f.gets( strem.ptr, s, breakchar);
    }
}

class FileMgr
{
    FileMgr( const FileMgr& );
    FileMgr& operator=( const FileMgr& );

    hunspell_file_s *f;
    size_t ptr = 0;
    std::string temp;
public:
    FileMgr( const char* filename, const char* key = NULL ) { f = (hunspell_file_s *)filename; }
    ~FileMgr() {}

    bool getline( std::string&s ) { return f->gets( ptr, s ); }
    char* getline() { if (!getline( temp )) return nullptr; return (char *)temp.data(); }
    int getlinenum() { return -1; }
};

