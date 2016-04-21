#pragma once

typedef fastdelegate::FastDelegate< void (const ts::str_c&) > MENUHANDLER;

void TSCALL menu_handler_stub(const ts::str_c &);


enum menu_item_flags_e
{
    MIF_MARKED      = SETBIT(0),
    MIF_DISABLED    = SETBIT(1),
};

struct menu_item_info_s
{
    ts::wstr_c  txt;
    ts::uint32 flags = 0;
    MENUHANDLER h = MENUHANDLER();
    ts::str_c prm;
    const ts::bitmap_c *bmp = nullptr;
};


class menu_c
{
    struct core_s
    {
        ts::array_inplace_t<ts::bitmap_c, 0> bmps;
        ts::wbp_c bp;
        int ref;
        core_s():ref(1) {}
        void release()
        {
            --ref;
            if (ref == 0) TSDEL(this);
        }

        uint register_bitmap( const ts::bitmap_c&bmp )
        {
            bmps.add() = bmp;
            return bmps.size();
        }

    } *core = nullptr;

    ts::wbp_c *curbp = nullptr;
    int level = -1;

    void prepare()
    {
        if (core) core->release();
        core = TSNEW(core_s); 
        level = 0;
        curbp = &core->bp;
    }
    menu_c(const menu_c &parent, ts::wbp_c *curbp) :curbp(curbp), level(parent.level+1)
    {
        if (core != parent.core)
        {
            if (core) core->release();
            core = parent.core;
            if (core) ++core->ref;
        }
    }

    static ts::str_c encode( ts::uint32 flags, MENUHANDLER h, const ts::asptr& param ) // encode handler and text param into single string
    {
        ts::str_c v;
        v.set_as_uint(flags).append_char('\1').encode_base64(&h, sizeof(MENUHANDLER)).append_char('\1').append(param);
        return v;
    }

    /* may never need
    
    static ts::wstr_c encode(const ts::wstr_c & text, MENUHANDLER h, const ts::asptr& param) // encode some text, handler and text param into single string
    {
        ts::str_c v(text);
        v.append_char('\1').encode_base64(&h, sizeof(MENUHANDLER)).append_char('\1').append(param);
        return v;
    }

    menu_c& decode_and_add(const ts::wsptr & encoded);
    */
public:
    menu_c(const menu_c &mcopy):core(mcopy.core), level(mcopy.level), curbp(mcopy.curbp)
    {
        if (core) ++core->ref;
    }
    menu_c() {}
    ~menu_c() { if (core) core->release(); }
    void reset() { if (core) { core->release(); core = nullptr; curbp = nullptr; level = -1; } }
    explicit operator bool() const {return curbp != nullptr;}
    bool operator >= (const menu_c &om) const {return core == om.core && level <= om.level;} // проверяет, является ли *this родителем om
    int lv() const {return level;}
    menu_c& add( const ts::wstr_c & text, ts::uint32 flags = 0, MENUHANDLER h = MENUHANDLER(), const ts::asptr& param = CONSTASTR(""), ts::bitmap_c *icon = nullptr );
    menu_c& add_separator();
    menu_c add_path( const ts::wstr_c & path ); // path1/path2/path3
    menu_c add_sub( const ts::wstr_c & text );
    menu_c get_sub( const ts::wbp_c &bp ) const;

    bool is_empty() const
    {
        return curbp == nullptr || curbp->is_empty();
    }

    ts::wstr_c get_text(int index) const;

    menu_c &operator=(const menu_c&m)
    {
        if (core != m.core)
        {
            if (core) core->release();
            core = m.core;
            if (core) ++core->ref;
        }
        level = m.level;
        curbp = m.curbp;

        return *this;
    }

    /* may never need
    void operator<<(const ts::wstrings_c &sarr); // serialization (load from string array)
    void operator>>(ts::wstrings_c &sarr); // serialization (save to string array)
    */

    template<typename F, typename PAR> void iterate_items( F & f, PAR &par ) const
    {
        bool normal_item = false;
        bool need_separator = false;
        for (auto it = curbp->begin(); it; ++it)
        {
            ts::token<ts::wchar> t(it->as_string(), L'\1');
            if (t->get_length() == 0)
            {
                ++t;
                switch (t->get_char(0))
                {
                case 's':
                {
                    if (!it.name().is_empty())
                    {
                        // text separator - now
                        if (!f(par, it.name())) return; // just text - separator
                        normal_item = false; // to prevent simple separator
                    } else if (normal_item)
                    {
                        // simple separator -  not now. only if normal item after it
                        need_separator = true; 
                    }
                    break;
                }
                case 't':
                    {
                        if (need_separator)
                        {
                            if (!f(par, ts::wstr_c())) return; // simple separator
                            need_separator = false;
                        }
                        normal_item = true;
                        if (!f(par, it.name(), get_sub(*it))) return; // text and menu_c - submenu
                        break;
                    }
                }
            }
            else
            {
                if (need_separator)
                {
                    if (!f(par, ts::wstr_c())) return; // simple separator
                    need_separator = false;
                }
                normal_item = true;

                menu_item_info_s inf;
                inf.flags = t->as_uint();
                uint index = inf.flags >> 16;
                if ( index > 0 )
                {
                    inf.bmp = &core->bmps.get( index - 1 );
                    inf.flags = 0x0000ffff & inf.flags;
                }

                ++t; t->decode_base64(&inf.h, sizeof( inf.h ));
                ++t;

                inf.txt = it.name();
                inf.prm = ts::to_str( *t );

                if (!f(par, inf)) return; // text, handler, text param - menu item
            }
        }
    }

    // can change item's state
    template<typename F, typename PAR> void iterate_items(F & f, PAR &par)
    {
        if (!curbp) return;
        bool normal_item = false;
        bool need_separator = false;
        for (auto it = curbp->begin(); it; ++it)
        {
            ts::token<ts::wchar> t(it->as_string(), L'\1');
            if (t->get_length() == 0)
            {
                ++t;
                switch (t->get_char(0))
                {
                    case 's':
                    {
                        if (!it.name().is_empty())
                        {
                            // text separator - now
                            if (!f(par, it.name())) return; // just text - separator
                            normal_item = false; // to prevent simple separator
                        }
                        else if (normal_item)
                        {
                            // simple separator -  not now. only if normal item after it
                            need_separator = true;
                        }
                        break;
                    }
                    case 't':
                    {
                        if (need_separator)
                        {
                            if (!f(par, ts::wstr_c())) return; // simple separator
                            need_separator = false;
                        }
                        normal_item = true;
                        menu_c sm = get_sub(*it);
                        if (!f(par, it.name(), sm)) return; // text and menu_c - submenu
                        break;
                    }
                }
            }
            else
            {
                if (need_separator)
                {
                    if (!f(par, ts::wstr_c())) return; // simple separator
                    need_separator = false;
                }
                normal_item = true;
                
                menu_item_info_s inf;
                inf.flags = t->as_uint();
                uint index = inf.flags >> 16;
                if ( index > 0 )
                {
                    inf.bmp = &core->bmps.get( index - 1 );
                    inf.flags = 0x0000ffff & inf.flags;
                }

                ++t; t->decode_base64( &inf.h, sizeof( inf.h ) );
                ++t;

                inf.txt = it.name();
                inf.prm = ts::to_str( *t );

                ts::uint32 sflags = inf.flags;
                ts::str_c prm(ts::to_str(*t));
                bool cont = f(par, inf); // text, handler, text param - menu item
                if (sflags != inf.flags)
                {
                    it->set_value(to_wstr(encode(inf.flags | (index<<16), inf.h, inf.prm)));
                }

                if (!cont) return;
            }
        }
    }

};