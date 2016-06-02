#pragma once

#define TEXTWPAR( pn, defv ) ts::wstr_c pn() { if (!is_loaded()) return ts::wstr_c(CONSTWSTR("")); return from_utf8( get(CONSTASTR(#pn), CONSTASTR(defv)) ); } \
                            bool pn( const ts::wstr_c&un ) { return param( CONSTASTR(#pn), to_utf8(un) ); }

#define TEXTAPAR( pn, defv ) const ts::str_c &pn() { return get(CONSTASTR(#pn), CONSTASTR(defv)); } \
                            bool pn( const ts::str_c&un ) { return param( CONSTASTR(#pn), un ); }


#define INTPAR( pn, defv ) int pn() { return get(CONSTASTR(#pn), (int)(defv)); } \
                            bool pn( int un ) { return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_int(un) ); }

#define UINT32PAR( pn, defv ) ts::uint32 pn() { return get(CONSTASTR(#pn), (ts::uint32)(defv)); } \
                            bool pn( int un ) { return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_uint(un) ); }

#define INT64PAR( pn, defv ) int64 pn() { return get(CONSTASTR(#pn), (int64)(defv)); } \
                            bool pn( int64 un ) { return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_num<int64>(un) ); }

#define FLOATPAR( pn, defv ) float pn() { return get(CONSTASTR(#pn), (float)(defv)); } \
                            bool pn( float un ) { return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_float(un) ); }

#define DEFAULT_PROXY "localhost:9050"

class config_base_c
{
    GM_RECEIVER( config_base_c, ISOGM_ON_EXIT );

protected:
    ts::wstr_c path;
    ts::iweak_ptr<ts::sqlitedb_c> db;
    ts::hashmap_t<ts::str_c, ts::str_c> values;
    ts::astrings_c dirty;

    bool closed = false;

    bool cfg_reader( int row, ts::SQLITE_DATAGETTER );
    bool save_dirty( RID, GUIPARAM );
    virtual bool save();
    virtual void onclose() {};

    ts::SQLITE_TABLEREADER get_cfg_reader() { return DELEGATE( this, cfg_reader ); }

    template< typename T > struct cvts;
    template<> struct cvts<int>
    {
        static int cvt( bool init, ts::str_c& s, int def )
        {
            if ( init ) { s.set_as_int( def ); return def; }
            return s.as_int( def );
        }
    };
    template<> struct cvts<ts::uint32>
    {
        static int cvt( bool init, ts::str_c& s, ts::uint32 def )
        {
            if ( init ) { s.set_as_uint( def ); return def; }
            uint64 t = s.as_num<int64>( def );
            return ( ts::uint32 )( t & 0xffffffff );
        }
    };
    template<> struct cvts<int64>
    {
        static int64 cvt( bool init, ts::str_c& s, const int64&def )
        {
            if ( init ) { s.set_as_num( def ); return def; }
            return s.as_num<int64>( def );
        }
    };
    template<> struct cvts<uint64>
    {
        static uint64 cvt( bool init, ts::str_c& s, const uint64&def )
        {
            if ( init ) { s.set_as_num( def ); return def; }
            return s.get_char(0) == '-' ? (uint64)s.as_num<int64>( def ) : s.as_num<uint64>( def );
        }
    };
    template<> struct cvts<float>
    {
        static float cvt( bool init, ts::str_c& s, float def )
        {
            if ( init ) { s.set_as_float( def ); return def; }
            return s.as_float( def );
        }
    };
    template<> struct cvts<ts::ivec2>
    {
        static ts::ivec2 cvt( bool init, ts::str_c& s, const ts::ivec2&def )
        {
            if ( init ) { s.set_as_int( def.x ).append_char( ',' ).append_as_int( def.y ); return def; }
            ts::token<char> t( s, ',' );
            ts::ivec2 r;
            r.x = t ? t->as_int( def.x ) : def.x;
            ++t; r.y = t ? t->as_int( def.y ) : def.y;
            return r;
        }
    };
    template<> struct cvts<ts::irect>
    {
        static ts::irect cvt( bool init, ts::str_c& s, const ts::irect&def )
        {
            if ( init ) { s.set_as_int( def.lt.x ).append_char( ',' ).append_as_int( def.lt.y ).append_char( ',' ).append_as_int( def.rb.x ).append_char( ',' ).append_as_int( def.rb.y ); return def; }
            ts::token<char> t( s, ',' );
            ts::irect r;
            r.lt.x = t ? t->as_int( def.lt.x ) : def.lt.x;
            ++t; r.lt.y = t ? t->as_int( def.lt.y ) : def.lt.y;
            ++t; r.rb.x = t ? t->as_int( def.rb.x ) : def.rb.x;
            ++t; r.rb.y = t ? t->as_int( def.rb.y ) : def.rb.y;
            return r;
        }
    };

public:
    config_base_c() {}
    ~config_base_c();

    void close();

    void changed(bool save_all_now = false /* if true, save job will take lot of seconds (and will work in background) */); // initiate save procedure

    static void prepare_conf_table( ts::sqlitedb_c *db );

    ts::sqlitedb_c *get_db() { return  db;}
    bool is_loaded() const { return db != nullptr; }
    const ts::wstr_c &get_path() const {return path;};

    bool param( const ts::asptr& pn, const ts::asptr& vl );
    const ts::str_c &get(const ts::asptr& pn, const ts::asptr& def)
    {
        bool added = false;
        ts::str_c &v = values.add(pn, added);
        if (added) v = def;
        return v;
    }
    template<typename T> T get(const ts::asptr& pn, const T&def)
    {
        bool added = false;
        ts::str_c &v = values.add( pn, added );
        return cvts<T>::cvt(added,v,def);
    }

};

typedef fastdelegate::FastDelegate<void()> ONCLOSE_FUNC;

enum cfg_misc_flags_e
{
    MISCF_DISABLE64 = 1,
    MISCF_DISABLEBORDER = 2,
};

class config_c : public config_base_c
{
    ts::tbuf_t<ONCLOSE_FUNC> onclose_handlers;
    bool onclose_handlers_dead = false;
public:
    void load( const ts::wstr_c &path_override = ts::wstr_c() );
    /*virtual*/ void onclose() override
    {
        for(ONCLOSE_FUNC f : onclose_handlers)
            f();
        onclose_handlers.clear();
        ASSERT(!onclose_handlers_dead, "onclose item was died before close event!");
    }

    void onclosereg( ONCLOSE_FUNC f ) { onclose_handlers.set_replace(f,ONCLOSE_FUNC()); }
    void oncloseunreg( ONCLOSE_FUNC f ) { onclose_handlers.find_remove_fast(f); }
    void onclosedie( ONCLOSE_FUNC f ) { if (onclose_handlers.find_remove_fast(f)) onclose_handlers_dead = true; }

    INTPAR(build, 0);

    TEXTWPAR(profile, "")
    TEXTAPAR(language, "ru")
    TEXTWPAR(theme, "def")
    INTPAR(collapse_beh, 2)
    INTPAR(autoupdate, 1)
    //TEXTAPAR(autoupdate_newver, "")

    void autoupdate_newver( const ts::asptr&ver, bool bits64 )
    {
        if (bits64)
            param( CONSTASTR("autoupdate_newver"), ts::str_c(ver).append(CONSTASTR("/64")) );
        else
            param( CONSTASTR( "autoupdate_newver" ), ver );
    }
    ts::str_c autoupdate_newver( bool &bits64 )
    {
        ts::str_c v = get( CONSTASTR( "autoupdate_newver" ), CONSTASTR("0") );
        bits64 = v.ends( CONSTASTR( "/64" ) );
        if ( bits64 ) v.trunc_length(3);
        return v;
    }
    ts::str_c autoupdate_newver()
    {
        return get( CONSTASTR( "autoupdate_newver" ), CONSTASTR( "0" ) );
    }

    INTPAR(proxy, 0)
    TEXTAPAR(proxy_addr, DEFAULT_PROXY)

    TEXTWPAR(device_camera, "")
    TEXTAPAR(device_mic, "")
    TEXTAPAR(device_talk, "")
    TEXTAPAR(device_signal, "")

    FLOATPAR( vol_mic, 1.0f )
    FLOATPAR( vol_talk, 1.0f )
    FLOATPAR( vol_signal, 1.0f )

    INTPAR(dsp_flags, 0)

    TEXTAPAR(debug, "")
    INTPAR(allow_tools, 0)
    INTPAR( misc_flags, 0 )

    TEXTWPAR( temp_folder_sendimg, "%TEMP%\\$$$isotoxin\\sendimg\\" )
    TEXTWPAR( temp_folder_handlemsg, "%TEMP%\\$$$isotoxin\\handlemsg\\" )


#define SND(s) TEXTWPAR( snd_##s, #s ".ogg" )
    SOUNDS
#undef SND
#define SND(s) FLOATPAR( snd_vol_##s, 1.0f )
    SOUNDS
#undef SND

};

extern ts::static_setup<config_c,1000> cfg;