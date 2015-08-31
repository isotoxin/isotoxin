#include "isotoxin.h"

ts::static_setup<emoticons_c,1000> emoti;


namespace
{
    struct smile_element_s : gui_textedit_c::active_element_s
    {
        const emoticon_s *e;
        int ref = 1;
        ts::bitmap_c bmp;

        smile_element_s(const emoticon_s *e, int maxh):e(e)
        {
            advance = e->framesize().x; //-V807
            if (maxh && e->framesize().y > maxh)
            {
                float k = (float)maxh / (float)e->framesize().y;
                advance = lround(k * e->framesize().x);
                e->curframe().resize_to(bmp, ts::ivec2(advance, maxh), ts::FILTER_LANCZOS3);
                
            }
        }
        /*virtual*/ void release() override
        {
            --ref;
            ASSERT(ref >= 0);
            if (ref == 0)
                __super::release();
        }

        /*virtual*/ ts::wstr_c to_wstr() const override
        {
            return from_utf8(e->def);
        }
        /*virtual*/ ts::str_c to_utf8() const override
        {
            return e->unicode ? ts::str_c().append_unicode_as_utf8( e->unicode ) : e->def;
        }
        /*virtual*/ void update_advance(ts::font_c *font) override
        {
        }
        /*virtual*/ void setup(const ts::ivec2 &pos, ts::glyph_image_s &gi) override
        {
            if (bmp)
            {
                gi.width = (ts::uint16)bmp.info().sz.x;
                gi.height = (ts::uint16)bmp.info().sz.y;
                gi.pitch = bmp.info().pitch;
                gi.pixels = bmp.body();
                gi.pos.x = (ts::int16)pos.x;
                gi.pos.y = (ts::int16)(pos.y - bmp.info().sz.y)+2;
            } else
            {
                gi.width = (ts::uint16)e->framesize().x; //-V807
                gi.height = (ts::uint16)e->framesize().y;
                gi.pitch = e->curframe().info().pitch;
                gi.pixels = e->curframe().body();
                gi.pos.x = (ts::int16)pos.x;
                gi.pos.y = (ts::int16)(pos.y - e->framesize().y);
            }
            gi.color = 0;
            gi.thickness = 0;
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
    if (ee)
    {
        smile_element_s *se = ts::ptr_cast<smile_element_s *>(ee);
        se->e = nullptr;
        se->advance = 0;
        ee->release();
    }
}

gui_textedit_c::active_element_s * emoticon_s::get_edit_element(int maxh)
{
    if (ee)
    {
        smile_element_s *se = ts::ptr_cast<smile_element_s *>(ee);
        ++se->ref;
        return se;
    }
    smile_element_s *se = TSNEW( smile_element_s, this, maxh );
    ++se->ref;
    ee = se;
    return se;
}

bool emoticons_c::emo_static_image_s::load(const ts::blob_c &b)
{
    ts::bitmap_c bmp;
    if (!bmp.load_from_file(b.data(), b.size()))
        return false;

    if (bmp.info().sz.y > emoti().maxheight)
    {
        float k = (float)emoti().maxheight / (float)bmp.info().sz.y;
        int neww = lround( k * bmp.info().sz.x );
        current_frame.create( ts::ivec2(neww, emoti().maxheight) );
        bmp.resize_to( current_frame.extbody(), ts::FILTER_LANCZOS3 );
    } else
    {
        current_frame.create( bmp.info().sz );
        current_frame.copy( ts::ivec2(0), bmp.info().sz, bmp.extbody(), ts::ivec2(0) );
    }

    current_frame.premultiply();

    return true;
}


namespace
{
    struct loader_s
    {
        ts::wstr_c curset;
        ts::wstr_c fn;
        ts::blob_c emojiset;
        ts::wstrings_c fns;
        ts::wstrings_c setlist;

        bool current_pack = true;

        bool build_set_list(const ts::arc_file_s &f)
        {
            ts::str_c fnd(f.fn); fnd.case_down();
            if (fnd.ends(CONSTASTR(".decl")))
                setlist.add(ts::fn_get_path(to_wstr(fnd)));
            return true;
        }

        bool process_pak_file(const ts::arc_file_s &f)
        {
            ts::str_c fnd(f.fn); fnd.case_down();
            if (!ts::fn_get_path(to_wstr(fnd)).equals(curset))
                return true;
            if (fnd.ends(CONSTASTR(".decl")))
                emojiset = f.get();
            else if (fnd.ends(CONSTASTR(".gif")))
            {
                fns.add( emoti().load_gif_smile( ts::to_wstr(fnd), f.get(), current_pack ) );
            }
            else if (fnd.ends(CONSTASTR(".png")))
            {
                fns.add(emoti().load_png_smile(ts::to_wstr(fnd), f.get(), current_pack));
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

    emo_gif_s *gif = TSNEW(emo_gif_s, getunicode(n));
    gif->current_pack = current_pack;
    gif->load(body);
    gif->def = n;
    arr.add() = gif;
    return gif->def;
}

ts::str_c emoticons_c::load_png_smile(const ts::wstr_c& fn, const ts::blob_c &body, bool current_pack)
{
    ts::str_c n = to_utf8(ts::fn_get_name(fn));

    emo_static_image_s *img = TSNEW(emo_static_image_s, getunicode(n));
    img->current_pack = current_pack;
    img->load(body);
    img->def = n;
    arr.add() = img;
    return img->def;
}

void emoticons_c::reload()
{
    if (!prf().is_loaded())
        return;

    maxheight = 0;

    arr.clear();
    packs.clear();
    matchs.clear();

    loader_s ldr;

    auto insert_match_point = [this](const ts::asptr &mps, emoticon_s *e)
    {
        int cnt = matchs.size();
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
        int cnt = fns.size();
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
    maxheight = g_app->theme().conf().get_int(CONSTASTR("emomaxheight"));

    for (const ts::wstr_c &fn : fns)
    {
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
            if ( !setname.is_empty() && ldr.setlist.find(setname.as_sptr()) < 0 )
                prf().emoticons_set(CONSTWSTR(""));
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

        ts::zip_open(pak.data(), pak.size(), DELEGATE(&ldr,process_pak_file));

        if (ldr.emojiset)
        {
            packs.add(fn);
            // some meta information about smiles
            int index = 0;
            ts::token<char> emos( ldr.emojiset.cstr(), '\n' );
            for (auto s : emos)
            {
                int eqi = s.find_pos('=');
                if (eqi < 0) continue;

                int unicode = s.substr(0,eqi).as_uint();

                int cnt = arr.size();
                for (int i = 0; i < cnt; ++i)
                {
                    emoticon_s *e = arr.get(i);
                    if (e->unicode == unicode)
                    {
                        ts::token<char> tkn(s.substr(eqi+1).get_trimmed().as_sptr(), ',');
                        e->def = *tkn;
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

    int cnt = arr.size();
    for (int i = 0; i < cnt; ++i)
    {
        emoticon_s *e = arr.get(i);

        e->repl.set(CONSTASTR("<rect="));
        e->repl.append_as_int(i);
        e->repl.append_char(',');
        e->repl.append_as_int(e->framesize().x);
        e->repl.append_char(',');
        e->repl.append_as_int(-e->framesize().y);
        e->repl.append_char('>');

    }
}

bool find_link( ts::str_c &message, int from, ts::ivec2 & rslt );

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
        for (int i = 0; find_link(t, i, linkinds);)
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


    for( const match_point_s&mp : matchs )
    {
        int sf = 0;
        for(;;)
        {
            int i = t.find_pos( sf, mp.s );
            if (i >= 0)
            {
                int r0 = i;
                int r1 = i + mp.s.get_length();
                sf = r1;

                int insi = rpl.count();
                bool fail = false;
                for( const rpl_s & r : rpl )
                {
                    if (r0 < r.index)
                    {
                        int ii = &r - rpl.begin();
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
