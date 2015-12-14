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
	const ts::bitmap_c &src;
	ts::irect sis[SI_count];
    struct  
    {
        ts::TSCOLOR fillcolor = 0; // if (ALPHA(fillcolor) != 0)
        ts::TSCOLOR filloutcolor = 0;
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
    ts::ivec2 activesbshift;
    ts::ivec2 capbuttonsshift;
    ts::ivec2 capbuttonsshift_max;
	ts::ivec2 minsize;
    const ts::font_desc_c *capfont = &ts::g_default_text_font;
    const ts::font_desc_c *deffont = &ts::g_default_text_font;
    ts::TSCOLOR deftextcolor;
    ts::tbuf0_t<ts::TSCOLOR> colors;
    ts::abp_c addition;
	
    mutable ts::packed_buf_c< 2, SI_count > alphablend; // cache
	
    int sbwidth() const {return ts::tmax(sis[SI_SBREP].width(), sis[SI_SMREP].width());}

	void load_params(ts::bp_t<char> * block);

    ts::ivec2 size_by_clientsize(const ts::ivec2 &sz, bool maximized) const	// calc rect's full size by raw client area
    {
        if (maximized || fastborder())
            return ts::ivec2(sz.x + clientborder.lt.x + clientborder.rb.x - maxcutborder.lt.x - maxcutborder.rb.x,
                             sz.y + clientborder.lt.y + clientborder.rb.y + capheight_max - maxcutborder.lt.y - maxcutborder.rb.y);
        return ts::ivec2(sz.x + clientborder.lt.x + clientborder.rb.x, sz.y + clientborder.lt.y + clientborder.rb.y + capheight);
    }

    int clborder_x() const { if (fastborder()) return clientborder.lt.x + clientborder.rb.x - maxcutborder.lt.x - maxcutborder.rb.x; else return clientborder.lt.x + clientborder.rb.x; }
    int clborder_y() const { if (fastborder()) return clientborder.lt.y + clientborder.rb.y + capheight_max - maxcutborder.lt.y - maxcutborder.rb.y; else return clientborder.lt.y + clientborder.rb.y + capheight; }

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
        x = src.is_alpha_blend( sis[si] ) ? 1 : 2;
        alphablend.set(si,x);
        return x == 1;
    }

    static const ts::flags32_s::BITS F_FASTBORDER = SETBIT(0);
    static const ts::flags32_s::BITS F_ROOTALPHABLEND = SETBIT(1);
    static const ts::flags32_s::BITS F_SPECIALBORDER = SETBIT(2);

    DECLARE_DYNAMIC_BEGIN(theme_rect_s)
    theme_rect_s(const ts::bitmap_c &b, const theme_conf_s &thconf):src(b), hollowborder(0), resizearea(2), captop(20), captop_max(30), captexttab(5), capheight(20), capheight_max(32)
	{
        flags.init(F_FASTBORDER, thconf.fastborder);
        flags.init(F_ROOTALPHABLEND, thconf.rootalphablend);
        flags.init(F_SPECIALBORDER, thconf.specialborder);
	}
    DECLARE_DYNAMIC_END(private)

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
struct button_desc_s;
struct generated_button_data_s
{
    ts::bitmap_c src;
    static generated_button_data_s *generate( const ts::abp_c *gen );
    generated_button_data_s() {}
    virtual ~generated_button_data_s() {}
    virtual void setup(button_desc_s &desc);
    
    generated_button_data_s(const generated_button_data_s&) UNUSED;
    generated_button_data_s &operator=(const generated_button_data_s&) UNUSED;
};

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
    UNIQUE_PTR( generated_button_data_s ) genb;
    ts::ivec2 size;
    const ts::bitmap_c &src;
    ts::wstr_c text;
    ts::irect rects[numstates];
    ts::shared_ptr<theme_rect_s> rectsf[numstates];
    ts::TSCOLOR colors[numstates];
    mutable ts::packed_buf_c< 2, numstates > alphablend;
    ts::uint32 align = UNDEFINED;

    void load_params(theme_c *th, const ts::bp_t<char> * block);

    bool is_alphablend(states s) const
    {
        auto x = alphablend.get(s);
        if (x) return x == 1;
        x = src.is_alpha_blend(rects[s]) ? 1 : 2;
        alphablend.set(s, x);
        return x == 1;
    }

    ts::ivec2 draw( rectengine_c *engine, states st, const ts::irect& area, ts::uint32 defalign );

    DECLARE_DYNAMIC_BEGIN(button_desc_s)
    // private constructor ensures that theme_rect_s will always be created in dynamic memory by theme_rect_s::build factory
    button_desc_s(const ts::bitmap_c &b) :src(b)
    {
    }
    DECLARE_DYNAMIC_END(public)

    ~button_desc_s();
};

struct theme_image_s : ts::image_extbody_c
{
    const ts::bitmap_c *dbmp = nullptr;
    ts::irect rect;
    void draw( rectengine_c &eng, const ts::ivec2 &p ) const;
};

typedef fastdelegate::FastDelegate<void(const ts::str_c&, ts::font_params_s&)> FONTPAR;

class theme_c
{
    
    ts::hashmap_t<ts::str_c, theme_image_s> images;
	ts::hashmap_t<ts::wstr_c, ts::bitmap_c> bitmaps;
	ts::hashmap_t<ts::str_c, ts::shared_ptr<theme_rect_s> > rects;
    ts::hashmap_t<ts::str_c, ts::shared_ptr<button_desc_s> > buttons;
	ts::wstr_c m_name;
    ts::abp_c m_conf;
    int iver = -1;

	const ts::bitmap_c &loadimage( const ts::wsptr &name );

    struct theme_font_s
    {
        ts::str_c fontname;
        ts::font_params_s fontparam;
        theme_font_s(const ts::str_c &fontname, const ts::font_params_s &fontparam):fontname(fontname), fontparam(fontparam) {}
    };
    ts::array_inplace_t<theme_font_s, 1> prefonts;

public:
	theme_c();
	~theme_c();

    int ver() const {return iver;}

    template<typename FF> void font_params( const ts::asptr &fname, const FF &ff ) const
    {
        for (const theme_font_s &thf : prefonts)
            if (thf.fontname.begins(fname))
                return ff(thf.fontparam);
    }
    void reload_fonts(FONTPAR fp);
	bool load( const ts::wsptr &name, FONTPAR fp );
	ts::shared_ptr<theme_rect_s> get_rect(const ts::asptr &rname) const
	{
		const ts::shared_ptr<theme_rect_s> *p = rects.get(rname);
		return p ? (*p) : ts::shared_ptr<theme_rect_s>();
	}
    ts::shared_ptr<button_desc_s> get_button(const ts::asptr &bname) const
    {
        if (bname.l == 0)
            return buttons.begin().value();
        const ts::shared_ptr<button_desc_s> *p = buttons.get(bname);
        return p ? (*p) : ts::shared_ptr<button_desc_s>();
    }

    const theme_image_s * get_image(const ts::asptr &iname) const
    {
        return images.get(iname);
    }

    const ts::abp_c &conf() const {return m_conf;}

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
	mutable ts::shared_ptr<theme_rect_s> rects[RST_ALL_COMBINATIONS];
    mutable int theme_ver = -1;

public:
	cached_theme_rect_c() {}

    bool set_themerect( const ts::asptr &name )
    {
        if (themerect == name) return false;
        themerect.set(name);
        theme_ver = -1;
        return true;
    }
    const ts::str_c & get_themerect() const {return themerect;}

	const theme_rect_s *operator()(ts::uint32 st) const;
};

#endif