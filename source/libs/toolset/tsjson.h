#pragma once

namespace ts
{

    class json_c
    {
        struct elem_s
        {
            str_c s;
            UNIQUE_PTR( json_c ) v;
            elem_s & operator=( elem_s &&o)
            {
                s = o.s;
                v = std::move(o.v);
                return *this;
            }

            elem_s() {}
            elem_s( elem_s && ) UNUSED;
            elem_s( const elem_s & ) UNUSED;
            elem_s & operator=( const elem_s &) UNUSED;
        };

        array_inplace_t<elem_s,0> elems;

        str_c str;
        double n = 0.0;
        aint ni;

        enum special_e
        {
            SV_FALSE,
            SV_TRUE,
            SV_NULL,
            SV_OBJ,
            SV_ARRAY,
            SV_NUMBER,
            SV_STRING,
        } spec = SV_NULL;

        aint parse_obj( asptr s );
        aint parse_arr( asptr s );
        aint parse_num( bool negative,  asptr s );

    public:
        json_c() {}
        ~json_c() {}

        aint parse( asptr s );

        template<typename F> void iterate( const F &f ) const
        {
            if (SV_ARRAY == spec || SV_OBJ == spec)
            {
                for (const elem_s &e : elems)
                    f( e.s, *e.v );
            }
        }

        bool is_array() const {return SV_ARRAY == spec;}
        bool is_string() const {return SV_STRING == spec;}
        const str_c &as_string() const { return str; }
    
    };




} // namespace ts
