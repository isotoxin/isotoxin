#include "toolset.h"

#undef delete
#include <regex>

namespace ts
{
    namespace
    {
        struct regex_internal_s
        {
            std::wregex templ;
            regex_internal_s( const wsptr& s ) :templ( s.s, s.l, std::regex_constants::egrep | std::regex_constants::optimize | std::regex_constants::icase )
            {
            }
        };
    }


    regex_c::regex_c( const wsptr& template_string )
    {
        TS_STATIC_CHECK( sizeof(data) >= sizeof(regex_internal_s), "size!" );
        regex_internal_s *d = (regex_internal_s *)data;
        TSPLACENEW( d, template_string );
    }
    regex_c::~regex_c()
    {
        regex_internal_s *d = (regex_internal_s *)data;
        d->~regex_internal_s();
    }

    bool regex_c::present( const wsptr&s ) const
    {
        regex_internal_s *d = (regex_internal_s *)data;
        return std::regex_search( std::basic_string<wchar_t>(s.s, s.l), d->templ );
    }

} // namespace ts