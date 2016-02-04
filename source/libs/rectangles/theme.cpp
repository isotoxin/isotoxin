#include "rectangles.h"

using namespace ts;

const theme_rect_s *cached_theme_rect_c::operator()( ts::uint32 st ) const
{
    if (themerect.is_empty())
        return nullptr;

	if (theme_ver != gui->theme().ver())
	{
        if (theme_ver>=0)
        {
            for (int i = 0; i < RST_ALL_COMBINATIONS; ++i)
                rects[i] = nullptr;
        }
		theme_ver = gui->theme().ver();

		rects[0] = gui->theme().get_rect(themerect);
        rects[ RST_HIGHLIGHT ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".h")));
        rects[ RST_ACTIVE ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".a")));
        rects[ RST_FOCUS ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".f")));

        rects[ RST_ACTIVE | RST_HIGHLIGHT ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".ah")));
        if (!rects[ RST_ACTIVE | RST_HIGHLIGHT ]) rects[ RST_ACTIVE | RST_HIGHLIGHT ] = rects[ RST_HIGHLIGHT ];

        rects[ RST_FOCUS | RST_HIGHLIGHT ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".fh")));
        if (!rects[ RST_FOCUS | RST_HIGHLIGHT ]) rects[ RST_FOCUS | RST_HIGHLIGHT ] = rects[ RST_HIGHLIGHT ];

        rects[ RST_FOCUS | RST_ACTIVE ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".af")));
        if (!rects[ RST_FOCUS | RST_ACTIVE ]) rects[ RST_FOCUS | RST_ACTIVE ] = rects[ RST_ACTIVE ];

        rects[ RST_FOCUS | RST_ACTIVE | RST_HIGHLIGHT ] = gui->theme().get_rect(tmp_str_c(themerect).append(CONSTASTR(".afh")));
        if (!rects[RST_FOCUS | RST_ACTIVE | RST_HIGHLIGHT]) rects[RST_FOCUS | RST_ACTIVE | RST_HIGHLIGHT] = rects[RST_ACTIVE | RST_HIGHLIGHT];

        for(int i=0;i<RST_ALL_COMBINATIONS;++i)
            if (!rects[i]) rects[i] = rects[0];
	}

	return rects[ASSERT(st < RST_ALL_COMBINATIONS) ? st : 0].get();
}

theme_rect_s::~theme_rect_s()
{
}

void theme_rect_s::init_subimage(subimage_e si, const str_c &sidef, const colors_map_s &colsmap)
{
	siso[si].fillcolor = 0;
    siso[si].filloutcolor = 0;

    str_c tail;
	sis[si] = parserect(sidef, ts::irect(0), &tail);
    siso[si].tile = !tail.equals(CONSTASTR("stretch"));
    if (siso[si].tile && tail.begins(CONSTASTR("#")))
    {
        token<char> tt(tail,',');
        siso[si].fillcolor = colsmap.parse(*tt, ARGB(0,0,0,255));
        ++tt;
        if (tt) 
            siso[si].filloutcolor = colsmap.parse(*tt, ARGB(0,0,0,255));
    }
    siso[si].loaded = !sidef.is_empty();
}

void theme_rect_s::load_params(abp_c * block, const colors_map_s &colsmap)
{
#ifdef _DEBUG
    if (block->get_int(CONSTASTR("break")))
        __debugbreak();
#endif // _DEBUG

    static const char *sins[SI_count] = { "lt", "rt", "lb", "rb", "l", "t", "r", "b", "c", "base",
        "capstart", "caprep", "capend",
        "sbtop", "sbrep", "sbbot", "smtop", "smrep", "smbot" };

    const abp_c * byrect = block->get( CONSTASTR("byrect") );

    for (int i = 0; i < SI_count; ++i)
        init_subimage((subimage_e)i, block->get_string(sins[i]), colsmap);

	resizearea = block->get_int("resizearea", 2);
	hollowborder = parserect( block->get_string(CONSTASTR("hollowborder")), ts::irect(0) );
    maxcutborder = parserect( block->get_string(CONSTASTR("maxcutborder")), ts::irect(0) );
	clientborder = parserect( block->get_string(CONSTASTR("clientborder")), ts::irect(0) );
	minsize = parsevec2( block->get_string(CONSTASTR("minsize")), ts::ivec2(0) );
    capbuttonsshift = parsevec2( block->get_string(CONSTASTR("bshift")), ts::ivec2(0) );
    capbuttonsshift_max = parsevec2( block->get_string(CONSTASTR("bshiftmax")), ts::ivec2(0) );
    activesbshift = parsevec2( block->get_string(CONSTASTR("smh")), ts::ivec2(0) );

    captextadd = parsevec2( block->get_string(CONSTASTR("captextadd")), ts::ivec2(5,0) );
	captop = block->get_int(CONSTASTR("captop"), 0);
    captop_max = block->get_int(CONSTASTR("captop_max"), 0);
	capheight = block->get_int(CONSTASTR("capheight"), 0);
    capheight_max = block->get_int(CONSTASTR("capheight_max"), 0);

    capfont = &gui->get_font( block->get_string(CONSTASTR("capfont"), gui->default_font_name()) );
    deffont = &gui->get_font( block->get_string(CONSTASTR("deffont"), gui->default_font_name()) );

    deftextcolor = colsmap.parse( block->get_string(CONSTASTR("deftextcolor")).as_sptr(), ARGB(0,0,0) );
    ts::token<char> cols( block->get_string(CONSTASTR("colors")), ',' );
    colors.clear();
    for(;cols;++cols)
        colors.add() = colsmap.parse(*cols, deftextcolor);

    if (abp_c *addi = block->get(CONSTASTR("addition")))
    {
        addition = std::move(*addi);
    }

    if (byrect)
    {
        ts::irect byrectr = parserect( byrect->as_string(), ts::irect(0) );

        if (!siso[SI_LEFT_TOP].loaded) sis[SI_LEFT_TOP] = ts::irect( byrectr.lt, byrectr.lt + clientborder.lt );
        if (!siso[SI_RIGHT_TOP].loaded) sis[SI_RIGHT_TOP] = ts::irect( byrectr.rb.x - clientborder.rb.x, byrectr.lt.y, byrectr.rb.x, byrectr.lt.y + clientborder.lt.y );
        if (!siso[SI_LEFT_BOTTOM].loaded) sis[SI_LEFT_BOTTOM] = ts::irect( byrectr.lt.x, byrectr.rb.y - clientborder.rb.y, byrectr.lt.x + clientborder.lt.x, byrectr.rb.y );
        if (!siso[SI_RIGHT_BOTTOM].loaded) sis[SI_RIGHT_BOTTOM] = ts::irect( byrectr.rb - clientborder.rb, byrectr.rb );
        if (!siso[SI_LEFT].loaded)
        {
            sis[SI_LEFT] = ts::irect( byrectr.lt.x, byrectr.lt.y + clientborder.lt.y, byrectr.lt.x + clientborder.lt.x, byrectr.rb.y - clientborder.rb.y );
            siso[SI_LEFT].tile = true;
            siso[SI_LEFT].fillcolor = 0;
        }
        if (!siso[SI_TOP].loaded)
        {
            sis[SI_TOP] = ts::irect( byrectr.lt.x + clientborder.lt.x, byrectr.lt.y, byrectr.rb.x - clientborder.rb.x, byrectr.lt.y + clientborder.lt.y );
            siso[SI_TOP].tile = true;
            siso[SI_TOP].fillcolor = 0;
        }
        if (!siso[SI_RIGHT].loaded)
        {
            sis[SI_RIGHT] = ts::irect( byrectr.rb.x - clientborder.rb.x, byrectr.lt.y + clientborder.lt.y, byrectr.rb.x, byrectr.rb.y - clientborder.rb.y );
            siso[SI_RIGHT].tile = true;
            siso[SI_RIGHT].fillcolor = 0;
        }
        if (!siso[SI_BOTTOM].loaded)
        {
            sis[SI_BOTTOM] = ts::irect( byrectr.lt.x + clientborder.lt.x, byrectr.rb.y - clientborder.rb.y, byrectr.rb.x - clientborder.rb.x, byrectr.rb.y );
            siso[SI_BOTTOM].tile = true;
            siso[SI_BOTTOM].fillcolor = 0;
        }
        if (!siso[SI_CENTER].loaded)
        {
            sis[SI_CENTER] = ts::irect(byrectr.lt + clientborder.lt, byrectr.rb - clientborder.rb);
            siso[SI_CENTER].tile = true;
            siso[SI_CENTER].fillcolor = 0;
        }

        if (!siso[SI_BASE].loaded)
        {
            sis[SI_BASE] = ts::irect( byrectr.lt + clientborder.lt, byrectr.rb - clientborder.rb );
            siso[SI_BASE].tile = true;
            siso[SI_BASE].fillcolor = 0;
        }
    }
}

button_desc_s::~button_desc_s()
{
}
void button_desc_s::load_params(theme_c *th, const abp_c * block, const colors_map_s &colsmap, bool load_face)
{
    const ts::asptr stnames[numstates] = { CONSTASTR("normal"), CONSTASTR("hover"), CONSTASTR("press"), CONSTASTR("disabled") };
    bool load_colors = true;
    if (load_face)
    {
        ts::str_c bx = block->get_string(CONSTASTR("build3"));
        int bxn = 3;
        if (bx.is_empty())
        {
            bx = block->get_string(CONSTASTR("build4"));
            bxn = 4;
        }
        if (!bx.is_empty())
        {
            // automatic build
            ts::token<char> build3(bx, ',');
            ts::ivec2 p, sz, d;
            p.x = build3->as_int(); ++build3;
            p.y = build3->as_int(); ++build3;
            sz.x = build3->as_int(); ++build3;
            sz.y = build3->as_int(); ++build3;
            d.x = build3->as_int(); ++build3;
            d.y = build3->as_int();

            for (int i = 0; i < bxn; ++i)
            {
                rects[i].lt = p;
                rects[i].rb = p + sz;
                p += d;
            }
            if (bxn == 3)
                rects[DISABLED] = rects[NORMAL];

        }
        else
        {
            for (int i = 0; i < numstates; ++i)
            {
                ts::str_c x = block->get_string(stnames[i]);
                rectsf[i] = th->get_rect(x);
                if (!rectsf[i])
                    rects[i] = parserect(x, ts::irect(0));
                colors[i] = colsmap.parse(block->get_string(sstr_t<32>(stnames[i], CONSTASTR("textcolor"))), ARGB(0, 0, 0));
            }
            load_colors = false;
        }
    }

    if (load_colors)
    {
        for (int i = 0; i < numstates; ++i)
        {
            ts::str_c x = block->get_string(stnames[i]);
            colors[i] = colsmap.parse(block->get_string(sstr_t<32>(stnames[i], CONSTASTR("textcolor"))), ARGB(0, 0, 0));
        }
    }

    text = to_wstr(block->get_string(CONSTASTR("text")));

    ts::token<char> salign( block->get_string(CONSTASTR("align")), ',' );
    for( ;salign; ++salign )
        if ( salign->equals(CONSTASTR("left") ) ) align |= ALGN_LEFT;
        else if ( salign->equals(CONSTASTR("top") ) ) align |= ALGN_TOP;
        else if ( salign->equals(CONSTASTR("right") ) ) align |= ALGN_RIGHT;
        else if ( salign->equals(CONSTASTR("bottom") ) ) align |= ALGN_BOTTOM;

    size = ts::ivec2(0);
    for (int i = 0; i < numstates; ++i)
    {
        if (rects[i].width() > size.x) //-V807
            size.x = rects[i].width();
        if (rects[i].height() > size.y)
            size.y = rects[i].height();

        ASSERT(size.x * src.info().bytepp() <= ts::tabs(src.info().pitch));
        ASSERT(rects[i].lt.x + size.x <= src.info().sz.x);
        ASSERT(rects[i].lt.y + size.y <= src.info().sz.y);
    }
    if (const ts::abp_c *sz = block->get(CONSTASTR("size")))
        size = parsevec2(sz->as_string(), size);
}

static ts::ivec2 calc_p( const ts::ivec2 &mysz, const ts::irect& area, ts::uint32 align )
{
    ts::ivec2 p = area.lt;
    switch (align & (ALGN_LEFT | ALGN_RIGHT))
    {
    case ALGN_RIGHT:
        p.x = area.rb.x - mysz.x;
        break;
    case ALGN_LEFT | ALGN_RIGHT:
        p.x += (area.width() - mysz.x) / 2;
        break;
    }
    switch (align & (ALGN_TOP | ALGN_BOTTOM))
    {
    case ALGN_BOTTOM:
        p.y = area.rb.y - mysz.y;
        break;
    case ALGN_TOP | ALGN_BOTTOM:
        p.y += (area.height() - mysz.y) / 2;
        break;
    }
    return p;
}

ts::ivec2 button_desc_s::draw( rectengine_c *engine, states st, const ts::irect& area, ts::uint32 defalign )
{
    ts::uint32 a = align;
    if (0 == (a & (ALGN_LEFT | ALGN_RIGHT))) a |= defalign & (ALGN_LEFT | ALGN_RIGHT);
    if (0 == (a & (ALGN_TOP | ALGN_BOTTOM))) a |= defalign & (ALGN_TOP | ALGN_BOTTOM);

    ts::ivec2 p = calc_p( rects[st].size(), area, a );
    engine->draw(p,src.extbody(rects[st]),is_alphablend(st));
    return p;
}

void theme_image_s::draw(rectengine_c &eng, const ts::irect& area, ts::uint32 align) const
{
    ts::ivec2 p = calc_p(info().sz, area, align);
    eng.draw(p, dbmp->extbody(rect), true);
}


theme_c::theme_c()
{
}

theme_c::~theme_c()
{

}

ts::bitmap_c &theme_c::prepareimageplace( const ts::wsptr &name )
{
    bitmap_c &dbmp = bitmaps[name];
    return dbmp;
}

void theme_c::clearimageplace(const ts::wsptr &name)
{
    bitmaps.remove(name);
}

const bitmap_c &theme_c::loadimage( const wsptr &name )
{
	bitmap_c &dbmp = prepareimageplace(name);
    if (dbmp.info().pitch != 0)
        return dbmp;
    bitmap_c bmp;
    if (blob_c b = load_image(name))
    {
        if (!bmp.load_from_file(b.data(),b.size()))
            bmp.create_ARGB(ts::ivec2(128,128)).fill(ARGB(255,0,255));
    } else
        bmp.create_ARGB(ts::ivec2(128,128)).fill(ARGB(255, 0, 255));

    dbmp = std::move(bmp);
	return dbmp;
}

void theme_c::reload_fonts(FONTPAR fp)
{
    for(const theme_font_s &thf : prefonts)
    {
        font_params_s fpt( thf.fontparam );
        fp(thf.fontname, fpt);
        add_font(thf.fontname, fpt);
    }
    ts::g_default_text_font = gui->get_font(gui->default_font_name());

}

static bool try_load_parent( ts::wstr_c &path, const wstr_c &parent, const wstr_c &colorsdecl, ts::abp_c &bpc)
{
    ASSERT(path.begins(CONSTWSTR("themes/")));
    path.set_length(7).append(parent).append_char('/');
    int pl = path.get_length();

    path.append(colorsdecl);
    if (g_fileop->load(path, bpc))
        return true;

    path.set_length(pl).append(CONSTWSTR("struct.decl"));

    abp_c bp;
    if (!g_fileop->load(path, bp)) return false;
    abp_c * pbp = bp.get(CONSTASTR("parent"));
    if (!pbp) return false;

    return try_load_parent(path, to_wstr(pbp->as_string()), colorsdecl, bpc);
}

bool theme_c::load( const ts::wsptr &name, FONTPAR fp, bool summon_ch_signal)
{
    wstr_c colorsdecl;
    int dd = ts::pwstr_c(name).find_pos('@');
    if ( dd >= 0 )
    {
        colorsdecl.set( name.s + dd + 1, name.l - dd - 1 ).append(CONSTWSTR(".decl"));
    } else
    {
        dd = name.l;
        colorsdecl = CONSTWSTR("defcolors.decl");
    }
	wstr_c path(CONSTWSTR("themes/"));
    path.append(ts::wsptr(name.s, dd)).append_char('/');
	int pl = path.get_length();
	abp_c bp;
	if (!g_fileop->load(path.append(CONSTWSTR("struct.decl")), bp)) return false;

    abp_c * parent = bp.get(CONSTASTR("parent"));
    abp_c bpc;
    for(;;)
    {
        path.set_length(pl).append(colorsdecl);
        if (g_fileop->load(path, bpc))
            break;

        ts::wstr_c x(path);
        if (parent && try_load_parent(x, to_wstr(parent->as_string()), colorsdecl, bpc))
            break;

        if ( colorsdecl == CONSTWSTR("defcolors.decl") )
            return false;
        colorsdecl = CONSTWSTR("defcolors.decl");
    }

    bp.merge( bpc, 0 );

    clear_glyphs_cache();
    ++iver;
	path.set_length(pl);

    set_fonts_dir(path);
    set_images_dir(path);

    bitmaps.clear();
    rects.clear();
    buttons.clear();

    abp_c bp_temp;

    while (parent)
    {
        ts::wstr_c pfolder = to_wstr(parent->as_string());
        if (!pfolder.is_empty())
        {
            abp_c pbp;
            wstr_c ppath(CONSTWSTR("themes/"));
            ppath.append(pfolder).append_char('/');
            set_fonts_dir(ppath, true);
            set_images_dir(ppath, true);
            if (!g_fileop->load(ppath.append(CONSTWSTR("struct.decl")), pbp)) return false;
            bp.merge(pbp,abp_c::SKIP_EXIST);
            if (pbp.get_remove(CONSTASTR("parent"), bp_temp))
                parent = &bp_temp;
            else 
                parent = nullptr;
        }
    }

    colors_map_s colsmap;
    if (abp_c * colors = bp.get("colors"))
    {
        for (auto it = colors->begin(); it; ++it)
        {
            ts::TSCOLOR col = parsecolor<char>( it->as_string(), ts::ARGB(0,0,0) );
            colsmap.add( it.name() ) = col;
        }
    }

    ts::tmp_pointers_t<ts::bitmap_c, 16> premultiply;

    if (const abp_c * corrs = bp.get("corrections"))
    {
        for (auto it = corrs->begin(); it; ++it)
        {
            bitmap_c &dbmp = const_cast<bitmap_c &>(loadimage(to_wstr(it.name())));

            for (auto itf = it->begin(); itf; ++itf)
            {
                if (itf.name().equals(CONSTASTR("premultiply")) && itf->as_int())
                    premultiply.add(&dbmp);
                else
                {
                    irect r = ts::parserect(itf.name(), irect(ivec2(0), dbmp.info().sz));
                    token<char> t(itf->as_string());

                    if (t->equals(CONSTASTR("zeroalpha")))
                    {
                        dbmp.fill_alpha(r.lt, r.size(), 1);

                    } else if (t->equals(CONSTASTR("fill")))
                    {
                        ++t;
                        ts::TSCOLOR fc = colsmap.parse(*t, ARGB(0, 0, 0, 255));
                        if (ts::ALPHA(fc) == 255)
                            dbmp.fill(r.lt, r.size(), fc);
                        else
                            dbmp.overfill(r.lt, r.size(), ts::PREMULTIPLY(fc));
                    } else if (t->equals(CONSTASTR("mulcolor")))
                    {
                        ++t;
                        ts::TSCOLOR mc = colsmap.parse(*t, 0xffffffff);
                        dbmp.mulcolor(r.lt, r.size(), mc);
                    } else if (t->equals(CONSTASTR("replace")))
                    {
                        ts::TSCOLOR c1, c2;
                        ++t;
                        c1 = colsmap.parse(*t, 0xffffffff);
                        ++t;
                        c2 = colsmap.parse(*t, 0xffffffff);
                        if (c1 != c2)
                            dbmp.process_pixels(r.lt, r.size(), [&](ts::TSCOLOR &c) { if (c == c1) c = c2; });

                    } else if (t->equals(CONSTASTR("invert")))
                    {
                        dbmp.process_pixels(r.lt, r.size(), [](ts::TSCOLOR &c) { c = (c & 0xff000000) | (0x00ffffff - (c & 0x00ffffff)); });
                    } 
                }
            }
        }
    }

    for (ts::bitmap_c *b : premultiply)
        b->premultiply();

    theme_conf_s thc;
    if (abp_c * conf = bp.get("conf"))
    {
        thc.fastborder = conf->get_int(CONSTASTR("fastborder"), 0) != 0;
        thc.rootalphablend = conf->get_int(CONSTASTR("rootalphablend"), 1) != 0;
        thc.specialborder = conf->get_int(CONSTASTR("specialborder"), 0) != 0;
        m_conf = std::move(*conf);
        
        sstr_t<-32> rs;
        for (auto it = m_conf.begin(); it; ++it)
        {
            str_c s = it->as_string();
            int x = s.find_pos(CONSTASTR("<color=##"));
            if (x >= 0)
            {
                int y = s.find_pos(x + 9,'>');
                if (y > x)
                {
                    TSCOLOR c = colsmap.parse( s.substr(x+7,y), ts::ARGB(0,0,0) );
                    rs.clear();
                    rs.append_as_hex( RED(c) );
                    rs.append_as_hex( GREEN(c) );
                    rs.append_as_hex( BLUE(c) );
                    if (ALPHA(c) != 0xff)
                        rs.append_as_hex( ALPHA(c) );
                    s.replace(x+8, y - x - 8, rs);
                    it->set_value(s);
                }
            }
            if ( it.name().find_pos(CONSTASTR("color")) >= 0 && s.begins(CONSTASTR("##")) )
            {
                make_color(s, colsmap.parse(s, ts::ARGB(0, 0, 0)));
                it->set_value(s);
            }
        }
    }


    if (const abp_c * fonts = bp.get("fonts"))
    {
        for (auto it = fonts->begin(); it; ++it)
        {
            ts::font_params_s fpar;
            fpar.setup( it->as_string() );
            prefonts.addnew( it.name(), fpar);
        }
    }
    reload_fonts(fp);

	if (const abp_c * rs = bp.get("rects"))
	{
		for (auto it = rs->begin(); it; ++it)
		{
			shared_ptr<theme_rect_s> &r = rects[ it.name() ];
            str_c bn = it->as_string();
            while (!bn.is_empty())
            {
                const abp_c *parnt = rs->get(bn);
                bn.clear();
                if (parnt)
                {
                    it->merge(*parnt, abp_c::SKIP_EXIST);
                    bn = parnt->as_string();
                }
            }
            const bitmap_c &dbmp = loadimage(to_wstr(it->get_string("src")));
			r = TSNEW( theme_rect_s, dbmp, thc );

			r->load_params(it, colsmap);
		}
	}

    if (const abp_c * imgs = bp.get("images"))
    {
        for (auto it = imgs->begin(); it; ++it)
        {
            irect r = irect(0);
            const bitmap_c *dbmp = nullptr;
            if (ts::abp_c *gen = it->get(CONSTASTR("gen")))
            {
                if (generated_button_data_s *bgenstuff = generated_button_data_s::generate(gen, colsmap, true))
                {
                    bitmap_c &b = prepareimageplace(to_wstr(it.name()).append_char('?'));
                    b = bgenstuff->src;
                    dbmp = &b;
                    TSDEL(bgenstuff);
                    r = irect(ivec2(0), dbmp->info().sz);
                }
            }

            if (nullptr == dbmp)
            {
                str_c src = it->get_string(CONSTASTR("src"));
                if (ASSERT(!src.is_empty()))
                {
                    token<char> t(src.as_sptr());
                    dbmp = &loadimage(to_wstr(t->as_sptr()));
                    ++t;
                    r = ts::parserect(t, irect(ivec2(0), dbmp->info().sz));
                }
            }

            theme_image_s &img = images.add(it.name());
            img.dbmp = dbmp;
            img.rect = r;
            (ts::image_extbody_c &)img = std::move(ts::image_extbody_c(dbmp->body(r.lt), imgdesc_s(r.size(), 32, dbmp->info().pitch)));
            ts::add_image(to_wstr(it.name()), img.body(), img.info(), false /* no need to copy */);

        }
    }

    if (const abp_c * btns = bp.get("buttons"))
    {
        for (auto it = btns->begin(); it; ++it)
        {
            shared_ptr<button_desc_s> &bd = buttons[it.name()];
            if (bd) continue;

            str_c par = it->as_string();
            while (!par.is_empty())
            {
                const abp_c *parnt = btns->get(par);
                par.clear();
                if (parnt)
                {
                    it->merge(*parnt, abp_c::SKIP_EXIST);
                    par = parnt->as_string();
                }
            }

            if (ts::abp_c *gen = it->get(CONSTASTR("gen")))
            {
                if (!gen->as_string().equals(CONSTASTR("disable")))
                if (generated_button_data_s *bgenstuff = generated_button_data_s::generate(gen, colsmap, false))
                {
                    bitmap_c &b = prepareimageplace(to_wstr(it.name()).append(CONSTWSTR("?btn?")));
                    b = bgenstuff->src;
                    bd = TSNEW(button_desc_s, b);

                    int h = b.info().sz.y / bgenstuff->num_states;
                    bd->size.x = b.info().sz.x;
                    bd->size.y = h;
                    for (int i = 0; i < bgenstuff->num_states; ++i)
                    {
                        bd->rects[i].lt = ts::ivec2(0, h * i);
                        bd->rects[i].rb = ts::ivec2(bd->size.x, h * i + h);
                    }
                    if (bgenstuff->num_states < button_desc_s::numstates)
                        bd->rects[button_desc_s::DISABLED] = bd->rects[button_desc_s::NORMAL];

                    bd->load_params(this, it, colsmap, false);
                    TSDEL( bgenstuff );
                }
            }

            if (!bd)
            {
                ts::str_c src = it->get_string(CONSTASTR("src"));
                if (src.is_empty())
                {
                    bd = TSNEW(button_desc_s, make_dummy<bitmap_c>(true));
                    bd->load_params(this, it, colsmap, true);

                }
                else
                {
                    const bitmap_c &dbmp = loadimage(to_wstr(src));
                    bd = TSNEW(button_desc_s, dbmp);
                    bd->load_params(this, it, colsmap, true);
                }
            }

        }
    }

	m_name = name;
    if (iver > 0 && summon_ch_signal) // 1st load (iver == 0) is not change
        gmsg<GM_UI_EVENT>(UE_THEMECHANGED).send();
	return true;
}

/*
void theme_c::add_image( const asptr & tag, bitmap_c &bmp )
{
    static int num = 1;
    bitmap_c &dbmp = bitmaps[wstr_c(CONSTWSTR("???")).append_as_uint(num++)];
    dbmp = bmp;
    theme_image_s &img = images.add(tag);
    img.dbmp = &dbmp;
    img.rect.lt = ts::ivec2(0);
    img.rect.rb = dbmp.info().sz;
}
*/

irect theme_rect_s::captionrect( const irect &rr, bool maximized ) const
{
	irect r;
    if (maximized || fastborder())
    {
        r.lt = clientborder.lt - maxcutborder.lt + rr.lt;
        r.rb.x = rr.width() - clientborder.rb.x + maxcutborder.rb.x + rr.lt.x;
        r.rb.y = r.lt.y + capheight_max;
    } else
    {
        r.lt = clientborder.lt + rr.lt;
        r.rb.x = rr.width() - clientborder.rb.x + rr.lt.x;
        r.rb.y = r.lt.y + capheight;
    }
	return r;

}

irect theme_rect_s::clientrect( const ts::ivec2 &sz, bool maximized ) const		// calc raw client area
{
	irect r;
    if (maximized || fastborder())
    {
        r.lt = clientborder.lt - maxcutborder.lt;
        r.rb = sz - clientborder.rb + maxcutborder.rb;
        r.lt.y += capheight_max;
    } else
    {
        r.lt = clientborder.lt;
        r.rb = sz - clientborder.rb;
        r.lt.y += capheight;
    }
	return r;
}

irect theme_rect_s::hollowrect( const rectprops_c &rps ) const	// calc client area and resize area
{
	irect r;
    if (rps.is_maximized() || fastborder())
    {
        r.lt = ts::ivec2(0);
        r.rb = rps.currentsize();
    } else
    {
        r.lt = hollowborder.lt;
        r.rb = rps.currentsize() - hollowborder.rb;
    }
	return r;
}
