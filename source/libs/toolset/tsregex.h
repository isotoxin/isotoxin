#pragma once


namespace ts
{
    class regex_c
    {
        uint8 data[96];
    public:
        regex_c( const wsptr& template_string );
        ~regex_c();

        bool present( const wsptr& ) const; // regex_search
    };
}
