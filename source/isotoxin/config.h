#pragma once

#define TEXTWPAR( pn, defv ) ts::wstr_c pn() { if (!is_loaded()) return ts::wstr_c(CONSTWSTR("")); return ts::wstr_c().set_as_utf8( get(CONSTASTR(#pn), CONSTASTR(defv)) ); } \
                            bool pn( const ts::wstr_c&un ) { return param( CONSTASTR(#pn), to_utf8(un) ); }

#define TEXTAPAR( pn, defv ) const ts::str_c &pn() { return get(CONSTASTR(#pn), CONSTASTR(defv)); } \
                            bool pn( const ts::str_c&un ) { return param( CONSTASTR(#pn), un ); }


#define INTPAR( pn, defv ) int pn() { return get(CONSTASTR(#pn), (int)(defv)); } \
                            bool pn( int un ) { return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_int(un) ); }

#define INTPARC( pn, defv ) int pn() { if (!pn##_cache_initialized) {pn##_cache = get(CONSTASTR(#pn), (int)(defv)); pn##_cache_initialized = true;} return pn##_cache; } \
                            bool pn( int un ) { pn##_cache_initialized = true; pn##_cache = un; return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_int(un) ); }

#define INT64PAR( pn, defv ) int64 pn() { return get(CONSTASTR(#pn), (int64)(defv)); } \
                            bool pn( int64 un ) { return param( CONSTASTR(#pn), ts::tmp_str_c().set_as_num<int64>(un) ); }

#define INTPAR_CACHE(pn) int pn##_cache = 0; bool pn##_cache_initialized = false;

#define DEFAULT_PROXY "localhost:9050"

class config_base_c
{
    SIMPLE_SYSTEM_EVENT_RECEIVER(config_base_c, SEV_CLOSE);

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

public:
    config_base_c() {}
    ~config_base_c();

    void changed(bool save_all_now = false /* if true, save job will take lot of seconds (and will work in background) */); // initiate save procedure

    static void prepare_conf_table( ts::sqlitedb_c *db );

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
    template<typename T> T get(const ts::asptr& pn, const T&def);
    template<> int get(const ts::asptr& pn, const int&def)
    {
        bool added = false;
        ts::str_c &v = values.add(pn, added);
        if (added) { v.set_as_int(def); return def;}
        return v.as_int(def);
    }
    template<> int64 get(const ts::asptr& pn, const int64&def)
    {
        bool added = false;
        ts::str_c &v = values.add(pn, added);
        if (added) { v.set_as_num<int64>(def); return def; }
        return v.as_num<int64>(def);
    }
    template<> ts::ivec2 get(const ts::asptr& pn, const ts::ivec2&def)
    {
        bool added = false;
        ts::str_c &v = values.add(pn, added);
        if (added) { v.set_as_int(def.x).append_char(',').append_as_int(def.y); return def; }
        ts::token<char> t( v, ',' );
        ts::ivec2 r;
        r.x = t->as_int(def.x);
        ++t; r.y = t->as_int(def.y);
        return r;
    }
};

typedef fastdelegate::FastDelegate<void()> ONCLOSE_FUNC;

class config_c : public config_base_c
{
    ts::tbuf_t<ONCLOSE_FUNC> onclose_handlers;
    bool onclose_handlers_dead = false;
public:
    void load(bool config_must_be_present = false);
    /*virtual*/ void onclose() override
    {
        for(ONCLOSE_FUNC f : onclose_handlers)
            f();
        onclose_handlers.clear();
        ASSERT(!onclose_handlers_dead, "onclose item was died before close event!");
    }

    void onclosereg( ONCLOSE_FUNC f ) { onclose_handlers.get_index_replace(f,ONCLOSE_FUNC()); }
    void oncloseunreg( ONCLOSE_FUNC f ) { onclose_handlers.find_remove_fast(f); }
    void onclosedie( ONCLOSE_FUNC f ) { if (onclose_handlers.find_remove_fast(f)) onclose_handlers_dead = true; }

    TEXTWPAR(profile, "")
    TEXTAPAR(language, "ru")
    TEXTWPAR(theme, "def")
    INTPAR(collapse_beh, 2)
    INTPAR(autoupdate, 0)
    INTPAR(autoupdate_proxy, 0)
    TEXTAPAR(autoupdate_proxy_addr, DEFAULT_PROXY)
    INT64PAR(autoupdate_next, 0)
    TEXTAPAR(autoupdate_newver, "")
    
    TEXTAPAR(device_mic, "")
    TEXTAPAR(device_talk, "")
    TEXTAPAR(device_signal, "")

};

extern ts::static_setup<config_c,1000> cfg;