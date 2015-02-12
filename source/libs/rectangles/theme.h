#pragma once
#ifndef _INCLUDE_THEME_
#define _INCLUDE_THEME_

class rectprops_c;

enum subimage_e
{
	SI_LEFT_TOP,
	SI_RIGHT_TOP,
	SI_LEFT_BOTTOM,
	SI_RIGHT_BOTTOM,
	SI_LEFT,
	SI_TOP,
	SI_RIGHT,
	SI_BOTTOM,

	SI_CENTER,
    SI_BASE,

	SI_CAPSTART,
	SI_CAPREP,
	SI_CAPEND,

    SI_SBTOP,
    SI_SBREP,
    SI_SBBOT,

    SI_SMTOP,
    SI_SMREP,
    SI_SMBOT,

	SI_count,
    SI_any
};

enum theme_rect_fonts_e
{
	TRF_CAPTION,

	TRF_count
};

struct theme_conf_s
{
    bool fastborder = false;
    bool rootalphablend = true;
    bool specialborder = false;
};

struct theme_rect_s : ts::shared_object
{
	const ts::drawable_bitmap_c &src;
	ts::irect sis[SI_count];
    struct  
    {
        ts::TSCOLOR fillcolor = 0; // if (ALPHA(fillcolor) != 0)
        bool tile = true;
        bool loaded = false;
    } siso[SI_count];
	ts::flags32_s flags;
	ts::irect hollowborder; // widths of hollow border
    ts::irect maxcutborder; // widths of overgraphics, which must be cut in maximized mode
	ts::irect clientborder;
	int resizearea;
    int captexttab;
	int captop; // v position of caption bitmaps (from top part of border)
    int captop_max;
	int capheight; // logical height of caption
    int capheight_max; // logical height of caption in maximized rect
    ts::ivec2 capbuttonsshift;
    ts::ivec2 capbuttonsshift_max;
	ts::ivec2 minsize;
    ts::str_c capfont;
    ts::str_c deffont;
    ts::TSCOLOR deftextcolor;
    ts::tbuf0_t<ts::TSCOLOR> colors;
	
    mutable ts::packed_buf_c< 2, SI_count > alphablend; // cache
	
	void load_params(const ts::bp_t<char> * block);
	static theme_rect_s * build( const ts::drawable_bitmap_c &dbmp, const theme_conf_s &thconf )
	{
        // hack due TSNEW
		struct theme_rect_warp : public theme_rect_s
		{
			theme_rect_warp(const ts::drawable_bitmap_c &b, const theme_conf_s &thconf):theme_rect_s(b, thconf) {}
		};

		TS_STATIC_CHECK( sizeof(theme_rect_warp) == sizeof(theme_rect_s), "what_da_fak" );

		return TSNEW( theme_rect_warp, dbmp, thconf );
	}

    ts::ivec2 size_by_clientsize(const ts::ivec2 &sz, bool maximized) const	// calc rect's full size by raw client area
    {
        if (maximized || fastborder())
            return ts::ivec2(sz.x + clientborder.lt.x + clientborder.rb.x - maxcutborder.lt.x - maxcutborder.rb.x,
                             sz.y + clientborder.lt.y + clientborder.rb.y + capheight_max - maxcutborder.lt.y - maxcutborder.rb.y);
        return ts::ivec2(sz.x + clientborder.lt.x + clientborder.rb.x, sz.y + clientborder.lt.y + clientborder.rb.y + capheight);
    }

    ts::irect captionrect( const ts::irect &rr, bool maximized ) const;	// calc caption rect
	ts::irect clientrect( const ts::ivec2 &sz, bool maximized ) const;	// calc raw client area
	ts::irect hollowrect( const rectprops_c &rps ) const;	// calc client area and resize area

    bool is_alphablend( subimage_e si ) const
    {
        if (si == SI_any)
        {
            for(int i=0;i<SI_count;++i)
            {
                if (is_alphablend((subimage_e)i)) return true;
            }
            return false;
        }
        auto x = alphablend.get(si);
        if (x) return x == 1;
        x = src.is_alphablend( sis[si] ) ? 1 : 2;
        alphablend.set(si,x);
        return x == 1;
    }

    static const ts::flags32_s::BITS F_FASTBORDER = SETBIT(0);
    static const ts::flags32_s::BITS F_ROOTALPHABLEND = SETBIT(1);
    static const ts::flags32_s::BITS F_SPECIALBORDER = SETBIT(2);

private:
	// private constructor ensures that theme_rect_s will always be created in dynamic memory by theme_rect_s::build factory
	theme_rect_s(const ts::drawable_bitmap_c &b, const theme_conf_s &thconf):src(b), hollowborder(0), resizearea(2), captop(20), captop_max(30), captexttab(5), capheight(20), capheight_max(32)
	{
        flags.init(F_FASTBORDER, thconf.fastborder);
        flags.init(F_ROOTALPHABLEND, thconf.rootalphablend);
        flags.init(F_SPECIALBORDER, thconf.specialborder);
	}
    void init_subimage(subimage_e si, const ts::str_c &sidef);
public:
	~theme_rect_s();

    bool fastborder() const {return flags.is(F_FASTBORDER);}
    bool rootalphablend() const {return flags.is(F_ROOTALPHABLEND);}
    bool specialborder() const {return flags.is(F_SPECIALBORDER);}

    ts::TSCOLOR color(int index) const
    {
        if (index >= 0 && index < colors.count()) return colors.get(index);
        return deftextcolor;
    }

};
class theme_c;
struct button_desc_s : ts::shared_object
{
    enum states
    {
        NORMAL,
        HOVER,
        PRESS,
        DISABLED,

        numstates
    };

    enum
    {
        UNDEFINED,
        ALEFT = SETBIT(0),
        ATOP = SETBIT(1),
        ARIGHT = SETBIT(2),
        ABOTTOM = SETBIT(3),
    };

    ts::ivec2 size;
    const ts::drawable_bitmap_c &src;
    ts::wstr_c text;
    ts::irect rects[numstates];
    ts::shared_ptr<theme_rect_s> rectsf[numstates];
    ts::TSCOLOR colors[numstates];
    mutable ts::packed_buf_c< 2, numstates > alphablend;
    ts::uint32 align = UNDEFINED;

    void load_params(theme_c *th, const ts::bp_t<char> * block);
    static button_desc_s * build(const ts::drawable_bitmap_c &dbmp)
    {
        struct button_desc_warp : public button_desc_s
        {
            button_desc_warp(const ts::drawable_bitmap_c &b) :button_desc_s(b) {}
        };

        TS_STATIC_CHECK(sizeof(button_desc_warp) == sizeof(button_desc_s), "what_da_fak" );

        return TSNEW(button_desc_warp, dbmp);
    }

    bool is_alphablend(states s) const
    {
        auto x = alphablend.get(s);
        if (x) return x == 1;
        x = src.is_alphablend(rects[s]) ? 1 : 2;
        alphablend.set(s, x);
        return x == 1;
    }

    ts::ivec2 draw( rectengine_c *engine, states st, const ts::irect& area, ts::uint32 defalign );

private:
    // private constructor ensures that theme_rect_s will always be created in dynamic memory by theme_rect_s::build factory
    button_desc_s(const ts::drawable_bitmap_c &b) :src(b)
    {
    }
public:
    ~button_desc_s();
};

class theme_c
{
	ts::hashmap_t<ts::wstr_c, ts::drawable_bitmap_c> bitmaps;
	ts::hashmap_t<ts::str_c, ts::shared_ptr<theme_rect_s> > rects;
    ts::hashmap_t<ts::str_c, ts::shared_ptr<button_desc_s> > buttons;
	ts::wstr_c m_name;

	const ts::drawable_bitmap_c &loadimage( const ts::wsptr &path, const ts::wsptr &name );

public:
	theme_c();
	~theme_c();

	bool load( const ts::wsptr &name );
	ts::shared_ptr<theme_rect_s> get_rect(const ts::asptr &rname) const
	{
		const ts::shared_ptr<theme_rect_s> *p = rects.get(rname);
		return p ? (*p) : ts::shared_ptr<theme_rect_s>();
	}
    ts::shared_ptr<button_desc_s> get_button(const ts::asptr &bname) const
    {
        const ts::shared_ptr<button_desc_s> *p = buttons.get(bname);
        return p ? (*p) : ts::shared_ptr<button_desc_s>();
    }
};

enum rect_state_e
{
    RST_HIGHLIGHT   = SETBIT(0),
    RST_ACTIVE      = SETBIT(1),
    RST_FOCUS       = SETBIT(2),

    RST_ALL_COMBINATIONS = 8 // update if add new state
};

class cached_theme_rect_c
{
    ts::str_c themerect;
	mutable const theme_c *theme;
	mutable ts::shared_ptr<theme_rect_s> rects[RST_ALL_COMBINATIONS];

public:
	cached_theme_rect_c():theme(nullptr) {}

    bool set_themerect( const ts::asptr &name )
    {
        if (themerect == name) return false;
        themerect.set(name);
        theme = nullptr;
        return true;
    }
    const ts::str_c & get_themerect() const {return themerect;}

	const theme_rect_s *operator()(ts::uint32 st) const;
};

#endif