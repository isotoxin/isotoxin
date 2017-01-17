#include "isotoxin.h"

ts::static_setup<emoticons_c,1000> emoti;


namespace
{
    struct smile_element_s : gui_textedit_c::active_element_s
    {
        const emoticon_s *e;
        ts::bitmap_c bmp;

        smile_element_s &operator=(const smile_element_s &se) UNUSED;
        smile_element_s(const smile_element_s &se) UNUSED;
        smile_element_s(const emoticon_s *e, int maxh):e(e)
        {
            ts::irect frect = e->framerect();

            advance = frect.width(); //-V807
            if (maxh && frect.height() > maxh)
            {
                float k = (float)maxh / (float)frect.height();
                advance = ts::lround(k * advance);

                ts::irect frrect; 
                const ts::bitmap_c&frame =  e->curframe(frrect);

                ts::ivec2 tsz(advance, maxh);

                if (tsz.x < 16 || tsz.y < 16)
                {
                    // too small target bmp - lanczos3 resize will fail
                    ts::ivec2 lsz(tsz * 2);
                    while (lsz.x < 16 || lsz.y < 16) lsz = lsz * 2;

                    ts::bitmap_c tmp;
                    if ( lsz == frrect.size() )
                    {
                        if (tsz == lsz / 2)
                        {
                            bmp.create_ARGB(tsz);
                            frame.shrink_2x_to(frrect.lt, frrect.size(), bmp.extbody());
                            return;
                        } else
                        {
                            tmp.create_ARGB(lsz / 2);
                            frame.shrink_2x_to(frrect.lt, frrect.size(), tmp.extbody());
                            ASSERT(tmp.info().sz.x > tsz.x && tmp.info().sz.y > tsz.y);
                        }
                    } else
                    {
                        tmp.create_ARGB(lsz);
                        tmp.resize_from(frame.extbody(frrect), ts::FILTER_LANCZOS3);
                        ASSERT(tmp.info().sz.x > tsz.x && tmp.info().sz.y > tsz.y);
                    }

                    while(tmp.info().sz.x > tsz.x)
                    {
                        bmp.create_ARGB(tmp.info().sz / 2);
                        tmp.shrink_2x_to(ts::ivec2(0), tmp.info().sz, bmp.extbody());
                        tmp = bmp;
                    }
                    ASSERT(tmp.info().sz == bmp.info().sz);

                } else
                {   
                    bmp.create_ARGB(tsz);
                    bmp.resize_from(frame.extbody(frrect), ts::FILTER_LANCZOS3);
                }
            }
        }
        ~smile_element_s()
        {
        }

        /*virtual*/ ts::wstr_c to_wstr() const override
        {
            if ( !e ) return ts::wstr_c(CONSTWSTR("(???)"));
            return from_utf8(e->def);
        }
        /*virtual*/ ts::str_c to_utf8() const override
        {
            if (!e) return ts::str_c(CONSTASTR("(???)"));
            return e->unicode ? ts::str_c().append_unicode_as_utf8( e->unicode ) : e->def;
        }
        /*virtual*/ void update_advance(ts::font_c *font) override
        {
        }
        /*virtual*/ void setup( ts::font_c *font, const ts::ivec2 &pos, ts::glyph_image_s &gi) override
        {
            if (!e)
            {
                bmp.create_ARGB(ts::ivec2(16));
                bmp.fill( ts::ARGB(255, 0, 255 ) );
            }

            if (bmp)
            {
                gi.width = (ts::uint16)bmp.info().sz.x;
                gi.height = (ts::uint16)bmp.info().sz.y;
                gi.pitch = bmp.info().pitch;
                gi.pixels = bmp.body();
                gi.pos().x = (ts::int16)pos.x;
                gi.pos().y = (ts::int16)(pos.y - font->ascender );
            } else
            {
                ts::irect frrect;
                const ts::bitmap_c&frame = e->curframe(frrect);

                int a = 0;
                if ( frrect.height() < font->height )
                    a = ( font->height - frrect.height() ) / 2;

                gi.width = (ts::uint16)frrect.width();
                gi.height = (ts::uint16)frrect.height();
                gi.pitch = frame.info().pitch;
                gi.pixels = frame.body( frrect.lt );
                gi.pos().x = (ts::int16)pos.x;
                gi.pos().y = (ts::int16)(pos.y - font->ascender + a);
            }
            gi.color = 0;
            gi.thickness = 0;
        }
        /*virtual*/ bool equals(const active_element_s *se) const override
        {
            if (se == this)
                return true;
            const smile_element_s *ses = dynamic_cast<const smile_element_s *>(se);
            return ses && ses->e == e;
        }
    };
}

bool emoticon_s::load( const ts::wsptr &fn )
{
    ts::blob_c gifb = ts::g_fileop->load(fn);
    return load(gifb);
}

/*virtual*/ emoticon_s::~emoticon_s() 
{
    if ( ee )
    {
        smile_element_s *se = ts::ptr_cast<smile_element_s *>( ee.get() );
        se->e = nullptr;
        se->advance = 0;
        ee = nullptr;
    }
    if (ispreframe)
        TSDEL( frame );
}

gui_textedit_c::active_element_s * emoticon_s::get_edit_element(int maxh)
{
    if (!ee)
        ee = TSNEW( smile_element_s, this, maxh );

    return ee;
}

/*virtual*/ bool emoticons_c::emo_gif_s::load(const ts::blob_c &body, ts::IMG_LOADING_PROGRESS /*progress*/ )
{
    ispreframe = true;
    frame = TSNEW( ts::bitmap_c );
    if (load_only_gif(*frame, body))
    {
        adapt_bg( frame );
        return true;
    }
    return false;
}

void emoticons_c::emo_gif_s::adapt_bg(const ts::bitmap_c *bmpx)
{
    ts::auint c = ts::GRAYSCALE_C(GET_THEME_VALUE(common_bg_color));
    if (c > 128)
        return; // background is light: assume gif smiles designed for light background, so do nothing now

    ts::irect r;
    const ts::bitmap_c *f = bmpx ? nullptr : &curframe(r);
    ts::bitmap_t<ts::bmpcore_exbody_s> b(bmpx ? bmpx->extbody() : f->extbody(r) );

    b.apply_filter(ts::ivec2(0), b.info().sz, [&](ts::uint8 *p, ts::bitmap_t<ts::bmpcore_exbody_s>::FMATRIX &m) {

        for (int x = 0; x < 3; ++x)
        {
            for (int y = 0; y < 3; ++y)
            {
                if (x == 1 && y == 1)
                    continue;
                if ( m[x][y] == nullptr )
                    continue;

                if (m[x][y][3] == 0)
                {
                    if ( ts::GRAYSCALE_C( *(ts::TSCOLOR *)p ) > 100 )
                    {
                        int minA = ts::tmax( 255 - p[0], 255 - p[1], 255 - p[2] );
                        if (minA == 0)
                        {
                            p[0] = c & 0xff;
                            p[1] = c & 0xff;
                            p[2] = c & 0xff;
                        } else
                        {
                            float invA = 255.0f / (float)minA;
                            float c1 = (p[0] + minA - 255.0f) * invA;
                            float c2 = (p[1] + minA - 255.0f) * invA;
                            float c3 = (p[2] + minA - 255.0f) * invA;

                            *(ts::TSCOLOR *)p = ts::PREMULTIPLY( ts::ARGB<uint>( ts::lround(c3), ts::lround(c2), ts::lround(c1), minA ) );
                        }

                    }
                    return;
                }
            }
        }
    });



}

/*virtual*/ bool emoticons_c::emo_static_image_s::load(const ts::blob_c &b, ts::IMG_LOADING_PROGRESS /*progress*/ )
{
    ts::bitmap_c bmp;
    if (!bmp.load_from_file(b.data(), b.size()))
        return false;

    if (bmp.info().sz.y > emoti().emoji_maxheight)
    {
        float k = (float)emoti().emoji_maxheight / (float)bmp.info().sz.y;
        int neww = ts::lround( k * bmp.info().sz.x );

        ispreframe = true;
        frame = TSNEW(ts::bitmap_c);
        frame->create_ARGB( ts::ivec2(neww, emoti().emoji_maxheight ) );

        bmp.resize_to( frame->extbody(), ts::FILTER_LANCZOS3 );
        frame->premultiply();

    } else
    {
        bmp.premultiply();
        ispreframe = true;
        frame = TSNEW(ts::bitmap_c);
        *frame = bmp;
    }


    return true;
}

/*virtual*/ bool emoticons_c::emo_tiled_animation_s::load( const ts::blob_c &b, ts::IMG_LOADING_PROGRESS /*progress*/ )
{
    if ( !source.load_from_file( b.data(), b.size() ) )
        return false;

    numframes = source.info().sz.y / source.info().sz.x;

    if ( source.info().sz.x > emoti().emoji_maxheight ) // width of image compared with height - its ok due source is vertical tiled animation of square frames
    {
        ts::imgdesc_s sinf( source.info(), ts::ivec2( source.info().sz.x ) );
        ts::bitmap_c tmp;
        tmp.create_ARGB( ts::ivec2( emoti().emoji_maxheight, emoti().emoji_maxheight * numframes ) );
        for ( int i = 0; i < numframes; ++i )
        {
            ts::img_helper_resize( tmp.extbody( ts::irect::from_lt_and_size( ts::ivec2( 0, emoti().emoji_maxheight * i ), ts::ivec2( emoti().emoji_maxheight ) ) ),
                source.body( ts::ivec2(0, source.info().sz.x * i) ),
                sinf,
                ts::FILTER_BOX_LANCZOS3
                );
        }
        source = tmp;
    }

    frect = ts::irect(0, 0, source.info().sz.x , source.info().sz.x /* not error - it's square*/ );

    source.premultiply();
    ispreframe = true;
    frame = nullptr;

    return true;
}

/*virtual*/ bool emoticons_c::emo_tiled_animation_s::animation_tick()
{
    ts::Time curt = ts::Time::current();

    if ( curt >= next_frame_tick )
    {
        next_frame_tick += nextframe();
        if ( curt >= next_frame_tick )
            next_frame_tick = curt;
        return true;
    }

    return false;
}

int emoticons_c::emo_tiled_animation_s::nextframe()
{
    frect.lt.y += source.info().sz.x;
    frect.rb.y += source.info().sz.x;
    if ( frect.lt.y >= source.info().sz.y )
    {
        frect.lt.y = 0;
        frect.rb.y = source.info().sz.x; // not error - frame is square
    }

    return animperiod;
}




namespace
{
    struct loader_s
    {
        ts::wstr_c curset;
        ts::wstr_c fn;
        ts::blob_c emojiset;
        ts::astrmap_c emojisettings;
        ts::wstrings_c fns;
        ts::wstrings_c setlist;

        int animperiod = 0;
        bool current_pack = true;

        bool build_set_list(const ts::arc_file_s &f)
        {
            ts::str_c fnd(f.fn); fnd.case_down();
            if (fnd.ends(CONSTASTR("emoji.decl")))
                setlist.add(ts::fn_get_path(to_wstr(fnd)));
            else if ( fnd.ends( CONSTASTR( "settings.decl" ) ) )
                emojisettings.parse( f.get().cstr() );

            return true;
        }

        bool process_pak_file(const ts::arc_file_s &f)
        {
            ts::str_c fnd(f.fn); fnd.case_down();
            if (!ts::fn_get_path(to_wstr(fnd)).equals(curset))
                return true;
            if ( fnd.ends( CONSTASTR( ".decl" ) ) )
            {
                if ( fnd.ends( CONSTASTR( "settings.decl" ) ) )
                    ;
                else
                    emojiset = f.get();

            } else if ( fnd.ends( CONSTASTR( ".gif" ) ) )
            {
                ts::str_c ifn = emoti().load_gif_smile( ts::to_wstr(fnd), f.get(), current_pack );
                if (!ifn.is_empty()) fns.add( ifn );
            } else if (fnd.ends(CONSTASTR(".png")))
            {
                ts::str_c ifn = emoti().load_png_smile(ts::to_wstr(fnd), f.get(), current_pack, animperiod );
                if (!ifn.is_empty())
                    fns.add( ifn );
            }

            return true;
        }
    };
}

static int getunicode( const ts::str_c&fn )
{
    int unic = fn.as_uint(1);
    return unic;
}

ts::str_c emoticons_c::load_gif_smile( const ts::wstr_c& fn, const ts::blob_c &body, bool current_pack )
{
    ts::str_c n = to_utf8(ts::fn_get_name(fn));
    int code = getunicode(n);
    for (const emoticon_s *e : arr)
        if (e->unicode == code)
            return n;

    emo_gif_s *gif = TSNEW(emo_gif_s, code);
    gif->current_pack = current_pack;
    if (gif->load(body))
    {
        gif->def = n;
        arr.add() = gif;
        return n;
    }

    TSDEL(gif);
    return ts::str_c();
}

ts::str_c emoticons_c::load_png_smile(const ts::wstr_c& fn, const ts::blob_c &body, bool current_pack, int animperiod )
{
    ts::str_c n = to_utf8(ts::fn_get_name(fn));
    int code = getunicode(n);
    for( const emoticon_s *e : arr )
        if (e->unicode == code)
            return n;

    if ( animperiod > 0 )
    {
        emo_tiled_animation_s *img = TSNEW( emo_tiled_animation_s, code, animperiod );
        img->current_pack = current_pack;
        if ( img->load( body ) )
        {
            img->def = n;
            arr.add() = img;
            return n;
        }

        TSDEL( img );
    } else
    {
        emo_static_image_s *img = TSNEW( emo_static_image_s, code );
        img->current_pack = current_pack;
        if ( img->load( body ) )
        {
            img->def = n;
            arr.add() = img;
            return n;
        }

        TSDEL( img );
    }

    return ts::str_c();

}

namespace
{
    struct backup_s : public ts::movable_flag<true>
    {
        ts::shared_ptr<smile_element_s> se;
        int unicode;
    };
}

void emoticons_c::reload()
{
    if (!prf().is_loaded())
        return;

    ts::tmp_array_inplace_t<backup_s, 16> bse;
    for( emoticon_s *e : arr )
    {
        if (e->ee)
        {
            backup_s &b = bse.add();
            b.se = ts::ptr_cast<smile_element_s *>( e->ee.get() );;
            b.se->e = nullptr;
            b.unicode = e->unicode;
            e->ee = nullptr;
        }
    }

    emoji_maxheight = 30;
    selector_min_width = 200;

    arr.clear();
    packs.clear();
    matchs.clear();

    loader_s ldr;

    auto insert_match_point = [this](const ts::asptr &mps, emoticon_s *e)
    {
        ts::aint cnt = matchs.size();
        for (int i = 0; i < cnt; ++i)
        {
            ts::str_c m = matchs.get(i).s;
            if (mps.l > m.get_length())
            {
                auto &mp = matchs.insert(i);
                mp.s = mps;
                mp.e = e;
                return;
            }
            if (m.equals(mps))
                return;
        }

        auto &mp = matchs.add();
        mp.s = mps;
        mp.e = e;

    };

    ts::wstrings_c fns;
    ts::g_fileop->find(fns, CONSTWSTR("smiles/*.zip"), true);
    for (ts::wstr_c &fn : fns)
        ts::fix_path(fn, FNO_NORMALIZE);
    fns.sort(true);
    if (!prf().emoticons_pack().is_empty())
    {
        ts::wstr_c pack_name = prf().emoticons_pack();
        ts::aint cnt = fns.size();
        for(int i=0;i<cnt;++i)
        {
            if ( fns.get(i).equals(pack_name) )
            {
                // move current pack name to top of list
                fns.remove_slow(i);
                fns.insert(0, pack_name);
                break;
            }
        }
    }

    ts::wstr_c appear = to_wstr(g_app->theme().conf().get_string(CONSTASTR("appearance")));
    
    emoji_maxheight = 30;
    selector_min_width = 200;
    bool defpack_ = true;

    for (const ts::wstr_c &fn : fns)
    {
        bool defpack = defpack_; defpack_ = false;

        ldr.emojisettings.clear();
        ldr.emojiset.clear();
        ldr.fn = fn;
        ldr.setlist.clear();
        ts::blob_c pak = ts::g_fileop->load(fn);
        ts::zip_open(pak.data(), pak.size(), DELEGATE(&ldr,build_set_list));

        ldr.curset.clear();
        if (ldr.setlist.size() == 1)
            ldr.curset = ldr.setlist.get(0); // only one set in pack - load it
        else if ( prf().emoticons_pack().equals(ts::fn_get_name(fn)) )
        {
            ts::wstr_c setname = prf().emoticons_set();
            if ( !setname.is_empty() && ldr.setlist.find(setname) < 0 )
                prf().emoticons_set(ts::wstr_c());
            ldr.curset = setname;
        }
        if (ldr.curset.is_empty())
        {
            for (const ts::wstr_c &sn : ldr.setlist)
                if (sn.find_pos(appear) >= 0)
                {
                    ldr.curset = sn;
                    break;
                }
        }

        if (ldr.curset.is_empty())
            continue;

        ldr.animperiod = ldr.emojisettings.get(CONSTASTR("animated"), ts::str_c(CONSTASTR("0"))).as_int();

        ts::zip_open(pak.data(), pak.size(), DELEGATE(&ldr,process_pak_file));

        if (ldr.emojiset)
        {
            if ( defpack )
            {
                emoji_maxheight = ldr.emojisettings.get(CONSTASTR("emoji-max-height"), ts::str_c(CONSTASTR("30"))).as_int();
                if ( emoji_maxheight < 16 ) emoji_maxheight = 16;
                selector_min_width = ldr.emojisettings.get(CONSTASTR("min-width"), ts::str_c(CONSTASTR("200"))).as_int();
                if ( selector_min_width < 200 ) selector_min_width = 200;
            }

            packs.add(fn);
            // some meta information about smiles
            int index = 0;
            ts::token<char> emos( ldr.emojiset.cstr(), '\n' );
            for (auto s : emos)
            {
                int eqi = s.find_pos('=');
                if (eqi < 0) continue;

                int unicode = s.substr(0,eqi).as_uint();

                ts::aint cnt = arr.size();
                for (int i = 0; i < cnt; ++i)
                {
                    emoticon_s *e = arr.get(i);
                    if (e->unicode == unicode)
                    {
                        ts::token<char> tkn(s.substr(eqi+1).get_trimmed().as_sptr(), ',');
                        e->def = *tkn;
                        if (defpack)
                            e->sort_factor = index;
                        for (auto &ss : tkn)
                            insert_match_point(ss.as_sptr(), e);
                        if (e->unicode > 0)
                            insert_match_point( ts::str_c().append_unicode_as_utf8(e->unicode), e );
                        break;
                    }
                }
                ++index;
            }
            ldr.current_pack = false;

        }
    }

    arr.sort( []( const emoticon_s *e1, const emoticon_s *e2 )->bool { return e1->sort_factor < e2->sort_factor; } );

    ts::aint cnt = arr.size();
    for (int i = 0; i < cnt; ++i)
    {
        emoticon_s *e = arr.get(i);
        ASSERT(e->ispreframe);

        ts::ivec2 csz = e->frame ? e->frame->info().sz : e->framerect().size();

        e->repl.set(CONSTASTR("<rect="));
        e->repl.append_as_int(i);
        e->repl.append_char(',');
        e->repl.append_as_int(csz.x);
        e->repl.append_char(',');
        e->repl.append_as_int(-csz.y);
        e->repl.append_char('>');
    }

    generate_full_frame();

    for( backup_s &b : bse )
    {
        for ( emoticon_s *e : arr )
        {
            if (e->unicode == b.unicode)
            {
                ASSERT( e->ee == nullptr );
                e->ee = b.se;
                b.se->e = e;
                b.se = nullptr;
                break;
            }
        }
    }
}

void emoticons_c::parse( ts::str_c &t, bool to_unicode )
{
    struct rpl_s
    {
        int index;
        int sz;
        const emoticon_s *e;
    };
    ts::tmp_tbuf_t<rpl_s> rpl;

    // detect forbidden areas (do not parse emoticons inside these areas)

    if (to_unicode)
    {
        // raw text
        // forbidden areas == links

        ts::ivec2 linkinds;
        for (int i = 0; text_find_link(t.as_sptr(), i, linkinds);)
        {
            rpl_s &r = rpl.add();
            r.index = linkinds.r0;
            r.sz = linkinds.r1 - linkinds.r0;
            r.e = nullptr;

            i = linkinds.r1;
        }

    } else
    {
        // parsed text
        // forbidden areas == parsed links

        for (int sfi = 0;;)
        {
            int i = t.find_pos(sfi, CONSTASTR("<cstm=a"));
            if (i >= 0)
            {
                sfi = i + 7;
                int j = t.find_pos(sfi, CONSTASTR("<cstm=b"));
                if (j > i)
                {
                    int k = t.find_pos(j, '>');
                    if (k > j)
                    {
                        sfi = k + 1;
                        rpl_s &r = rpl.add();
                        r.index = i;
                        r.sz = sfi - i;
                        r.e = nullptr;
                        continue;
                    }
                }
            }
            break;
        }
    }

    bool skip_short = !prf_options().is(MSGOP_REPLACE_SHORT_SMILEYS);

    for( const match_point_s&mp : matchs )
    {
        if (skip_short)
        {
            if (mp.s.get_length() > 4)
                goto process;
            if (mp.s.get_char(0) == mp.s.get_last_char() && ')' != mp.s.get_char(0) && '(' != mp.s.get_char(0))
                goto process;
            if (skip_utf8_char(mp.s, 0) == mp.s.get_length())
                goto process;
            continue;
        }

        process:

        int sf = 0;
        for(;;)
        {
            int i = t.find_pos( sf, mp.s );
            if (i >= 0)
            {
                if ( mp.s.get_length() == 2 )
                {
                    // special hardcode
                    // 2 char length smiles must be space-separated from other text
                    if (i > 0 && t.get_char(i-1) != ' ')
                    {
                        sf += 2;
                        continue;
                    }
                    if ( i+2 < t.get_length() && t.get_char( i+2 ) != ' ' )
                    {
                        sf += 2;
                        continue;
                    }
                }


                int r0 = i;
                int r1 = i + mp.s.get_length();
                sf = r1;

                ts::aint insi = rpl.count();
                bool fail = false;
                for( const rpl_s & r : rpl )
                {
                    if (r0 < r.index)
                    {
                        ts::aint ii = &r - rpl.begin();
                        if (ii < insi)
                            insi = ii;
                    }

                    if (r1 > r.index && r0 < (r.index+r.sz))
                    {
                        fail = true;
                        break;
                    }
                }
                if (fail)
                    continue;

                rpl_s &r = rpl.insert(insi);
                r.index = r0;
                r.sz = r1 - r0;
                r.e = mp.e;
                continue;
            }
            break;
        }
    }

    int addindex = 0;
    ts::sstr_t<-16> unicodes; 
    for( const rpl_s & r : rpl )
    {
        if (!r.e)
            continue; // just forbidden area

        if (to_unicode)
        {
            if (r.e->unicode)
            {
                unicodes.clear().append_unicode_as_utf8(r.e->unicode);
                t.replace(r.index + addindex, r.sz, unicodes);
                addindex = addindex - r.sz + unicodes.get_length();
            }

        } else
        {
            t.replace(r.index + addindex, r.sz, r.e->repl);
            addindex = addindex - r.sz + r.e->repl.get_length();
        }
    }
}


void emoticons_c::draw( rectengine_root_c *e, const ts::ivec2 &p, int smlnum )
{
    arr.get(smlnum)->draw(e,p);
}

menu_c emoticons_c::get_list_smile_pack(const ts::wstr_c &curpack, MENUHANDLER mh)
{
    menu_c m;

    bool f = true;
    for(const ts::wstr_c &pn : packs)
    {
        m.add( ts::fn_get_name(pn), ((curpack.is_empty()&&f) || curpack == pn) ? MIF_MARKED : 0, mh, to_utf8(pn) );
        f = false;
    }

    return m;
}

void emoticons_c::generate_full_frame()
{
    ts::tmp_tbuf_t<ts::ivec3> sizes;

    for (emoticon_s *e : arr)
    {
        ASSERT(e->ispreframe);
        if ( nullptr == e->frame )
            continue;
        
        ts::ivec2 csz = e->frame->info().sz;

        bool present = false;
        for( ts::ivec3 &s : sizes )
            if (s.xy() == csz)
            {
                ++s.z;
                present = true;
            }
        if (!present)
            sizes.add() = ts::ivec3( csz, 1 );
    }

    sizes.q_sort<ts::ivec3>( []( const ts::ivec3 *s1, const ts::ivec3 *s2 ) { return s1->z == s2->z ? (s1->y > s2->y) : (s1->z > s2->z); } );
    
    ts::tmp_tbuf_t<ts::irect> rects;

    auto find_rect = [&]( const ts::ivec2 &sz )->ts::irect
    {
        ts::aint cnt = rects.count();
        for(int i=0;i<cnt;++i)
        {
            ts::irect rc = rects.get(i);
            if (rc.size() == sz)
            {
                rects.remove_fast(i);
                return rc;
            }
        }
        FORBIDDEN();
        UNREACHABLE();
    };

    static const int max_width = 1024;
    ts::ivec2 pos(0);
    int lineheight = 0; int maxw = 0;

    if (sizes.count() == 1)
    {
        ts::ivec3 sz = sizes.get(0);
        int n_per_line = max_width/sz.x;
        int n_lines = (sz.z+n_per_line-1) / n_per_line;
        int crap = (n_per_line * n_lines)-sz.z;
        ASSERT(crap >= 0);
        ts::ivec2 gridsize(n_per_line, n_lines);
        if (sz.z > n_per_line)
        {
            --n_per_line;
            for (; crap != 0 && n_per_line > 2; --n_per_line)
            {
                n_lines = (sz.z + n_per_line - 1) / n_per_line;
                int newcrap = (n_per_line * n_lines) - sz.z;
                if (newcrap < crap)
                {
                    crap = newcrap;
                    gridsize.x = n_per_line;
                    gridsize.y = n_lines;
                }
            }
        }
        maxw = sz.x * gridsize.x;
        lineheight = sz.y * gridsize.y;

        for( int i=0;i<sz.z;++i )
        {
            int iy = i / gridsize.x;
            int ix = i - iy * gridsize.x;
            ts::ivec2 p(ix * sz.x, iy * sz.y);
            rects.add(ts::irect(p, p + sz.xy()));
        }

    } else
    {
        for (ts::ivec3 &s : sizes)
        {
            for (int i = 0; i<s.z; ++i)
            {
                if ((pos.x + s.x) > max_width)
                    pos.x = 0, pos.y += lineheight, lineheight = 0;

                rects.add(ts::irect(pos, pos + s.xy()));
                pos.x += s.x;
                if (s.y > lineheight) lineheight = s.y;
                if (pos.x > maxw) maxw = pos.x;
            }
        }
    }

    ts::ivec2 fullframesize( maxw, pos.y + lineheight );
    if ( fullframe.info().sz != fullframesize )
        fullframe.create_ARGB( fullframesize );

    for (emoticon_s *e : arr)
    {
        if ( nullptr == e->frame )
            continue;

        ts::bitmap_c *bmp = e->frame;
        e->ispreframe = false;
        e->frame = &fullframe;
        e->frect = find_rect(bmp->info().sz);
        e->frame->copy(e->frect.lt, bmp->info().sz, bmp->extbody(), ts::ivec2(0));
        TSDEL(bmp);
    }
}

