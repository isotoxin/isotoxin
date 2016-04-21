#include "isotoxin.h"


/*virtual*/ int dialog_prepareimage_c::saver_s::iterate(int pass)
{
    if (!dlg || bitmap2save.info().sz < ts::ivec2(1) ) return R_CANCEL;

    saved_jpg.clear();
    ts::buf_c sb;
    bitmap2save.save_as_png(sb);
    if (bitmap2save.is_alpha_blend())
    {
        saved_img_format = ts::if_png;
        saved_jpg.append_buf(sb);
        return R_DONE;
    }

    ts::buf_c sbjpg;
    bitmap2save.save_as_jpg( sbjpg );
    if ( sb.size() < sbjpg.size() )
    {
        saved_img_format = ts::if_png;
        saved_jpg.append_buf(sb);
        return R_DONE;
    }

    saved_img_format = ts::if_jpg;
    saved_jpg.append_buf(sbjpg);
    return R_DONE;
}

/*virtual*/ void dialog_prepareimage_c::saver_s::done(bool canceled)
{
    if (!canceled && dlg)
        dlg->saver_job_done(this);

    __super::done(canceled);
}

namespace
{
    struct enum_video_devices_s : public ts::task_c
    {
        vsb_list_t video_devices;
        ts::safe_ptr<dialog_prepareimage_c> dlg;

        enum_video_devices_s(dialog_prepareimage_c *dlg) :dlg(dlg) {}

        /*virtual*/ int iterate(int pass) override
        {
            enum_video_capture_devices(video_devices, true);
            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (!canceled && dlg)
                dlg->set_video_devices(std::move(video_devices));

            __super::done(canceled);
        }

    };
}


dialog_prepareimage_c::dialog_prepareimage_c(MAKE_ROOT<dialog_prepareimage_c> &data) :gui_isodialog_c(data), croprect(0), ck(data.ck)
{
    deftitle = title_prepare_image;
    bitmap = data.bitmap;

    ts::irect msz = ts::wnd_get_max_size( gui->get_cursor_pos() );
    wsz = ts::tmin( msz.size() * 80 / 100, ts::ivec2(800,600) );

    g_app->add_task(TSNEW(enum_video_devices_s, this));

    shadow = gui->theme().get_rect(CONSTASTR("shadow"));

    gui->register_kbd_callback( DELEGATE( this, paste_hotkey_handler ), ts::SSK_V, ts::casw_ctrl);
    gui->register_kbd_callback( DELEGATE( this, paste_hotkey_handler ), ts::SSK_INSERT, ts::casw_shift);
    gui->register_kbd_callback( DELEGATE( this, space_key ), ts::SSK_SPACE, 0);

    animation(RID(), nullptr);
}

dialog_prepareimage_c::~dialog_prepareimage_c()
{
    if (saver)
        saver->dlg = nullptr;

    if (gui)
    {
        gui->delete_event(DELEGATE(this,animation));
        gui->delete_event(DELEGATE(this, prepare_working_image));
        gui->unregister_kbd_callback(DELEGATE(this, paste_hotkey_handler));
        gui->unregister_kbd_callback(DELEGATE(this, space_key));
    }
}

/*virtual*/ void dialog_prepareimage_c::created()
{
    set_theme_rect(CONSTASTR("prepimg"), false);
    fd.prepare( get_default_text_color(3), get_default_text_color(4) );
    __super::created();
    tabsel( CONSTASTR("1") );

    if (bitmap.info().sz >> 0)
    {
        selrectvisible = true;
        animated = false;
        loaded_image.clear();
        loaded_img_format = ts::if_none;
        on_newimage();
    }
}

/*virtual*/ int dialog_prepareimage_c::additions(ts::irect &)
{
    descmaker dm(this);
    dm << 1;

    ts::wstr_c l(CONSTWSTR("<p=c>"));

    ts::wstr_c ctlopen, ctlpaste, ctlcam;

    ts::ivec2 bsz(50,25);
    const button_desc_s *b = gui->theme().get_button("save");
    if (b)
        bsz = b->size;

    ts::wstr_c openimgbuttonface(CONSTWSTR("open"));
    ts::wstr_c pasteimgbuttonface(CONSTWSTR("paste"));
    ts::wstr_c startcamera(CONSTWSTR("capture"));

    ts::ivec2 szopen(200, 25);
    if (const button_desc_s *avopen = gui->theme().get_button(CONSTASTR("avopen")))
        szopen = avopen->size, openimgbuttonface = CONSTWSTR("face=avopen");

    ts::ivec2 szpaste(200, 25);
    if (const button_desc_s *avpaste = gui->theme().get_button(CONSTASTR("avpaste")))
        szpaste = avpaste->size, pasteimgbuttonface = CONSTWSTR("face=avpaste");

    ts::ivec2 szcapture(200, 25);
    if (const button_desc_s *avcapture = gui->theme().get_button(CONSTASTR("avcapture")))
        szcapture = avcapture->size, startcamera = CONSTWSTR("face=avcapture");

    dm().button(ts::wstr_c(), openimgbuttonface, DELEGATE(this, open_image)).width(szopen.x).height(szopen.y).subctl(0, ctlopen).sethint(loc_text(loc_loadimagefromfile));
    dm().button(ts::wstr_c(), pasteimgbuttonface, DELEGATE(this, paste_hotkey_handler)).width(szpaste.x).height(szpaste.y).subctl(1, ctlpaste).sethint(loc_text(loc_pasteimagefromclipboard));
    dm().button(ts::wstr_c(), startcamera, DELEGATE(this, start_capture_menu)).width(szcapture.x).height(szcapture.y).setname(CONSTASTR("startc")).subctl(2, ctlcam).sethint(loc_text(loc_capturecamera));


    l.append(ctlopen).append(CONSTWSTR("<nbsp>"));
    l.append(ctlpaste).append(CONSTWSTR("<nbsp>"));
    l.append(ctlcam);
    dm().label(l);
    dm().hiddenlabel(ts::wstr_c(), false).setname(CONSTASTR("info"));
    dm().vspace(1).setname(CONSTASTR("last"));
    return 0;
}

bool dialog_prepareimage_c::animation(RID, GUIPARAM)
{
    if (!videodevicesloaded)
        ctlenable( CONSTASTR("startc"), false );

    ++tickvalue;
    DEFERRED_UNIQUE_CALL(0.05, DELEGATE(this, animation), nullptr);
    getengine().redraw();

    if (camera)
    {
        allow_ok(false);

        if (camst == CAMST_WAIT_ORIG_SIZE && camera->get_video_size() == camera->get_desired_size() && camera->updated())
            if (ts::drawable_bitmap_c *b = camera->lockbuf(nullptr))
            {
                if (b->info().sz == camera->get_video_size())
                {
                    selrectvisible = true;
                    animated = false;
                    bitmap.create_RGB(b->info().sz);
                    bitmap.copy(ts::ivec2(0), b->info().sz, b->extbody(), ts::ivec2(0));
                    camera->unlock(b);
                    on_newimage();
                } else
                    camera->unlock(b);
            }
    }

    if (animated)
    {
        nextframe();
        allow_ok(true);
    }

    allow_ok(saved_image.size() > 0);

    return true;
}

void dialog_prepareimage_c::saver_job_done(saver_s *s)
{
    ASSERT(saver == s);
    saved_image = s->saved_jpg;
    saved_img_format = s->saved_img_format;
    saver = nullptr;
    allow_ok(true);
}


void dialog_prepareimage_c::set_video_devices(vsb_list_t &&_video_devices)
{
    video_devices = std::move(_video_devices);
    videodevicesloaded = true;
    ctlenable( CONSTASTR("startc"), video_devices.size() > 0 );
}

void dialog_prepareimage_c::start_capture_menu_sel(const ts::str_c& prm)
{
    ts::wstr_c id = from_utf8(prm);
    for (const vsb_descriptor_s &vd : video_devices)
    {
        if (vd.id == id)
        {
            start_capture(vd);
            return;
        }
    }
}

void dialog_prepareimage_c::start_capture(const vsb_descriptor_s &desc)
{
    loaded_image.clear();
    loaded_img_format = ts::if_none;

    selrectvisible = false;
    animated = false;
    if (saver)
    {
        saver->dlg = nullptr; // say goodbye to current saver
        saver = nullptr;
    }

    camera.reset( vsb_c::build(desc, ts::wstrmap_c()) );
    getengine().redraw();

    camst = CAMST_NONE;
    update_info();
}


bool dialog_prepareimage_c::start_capture_menu(RID, GUIPARAM)
{
    if (video_devices.size() == 1)
    {
        start_capture( video_devices.get(0) );
        return true;
    }

    if (RID b = find(CONSTASTR("startc")))
    {
        ts::irect br = HOLD(b)().getprops().screenrect();
        menu_c m;

        for (const vsb_descriptor_s &vd : video_devices)
        {
            m.add(vd.desc, 0, DELEGATE(this, start_capture_menu_sel), to_utf8(vd.id));
        }

        gui_popup_menu_c::show(menu_anchor_s(br, menu_anchor_s::RELPOS_TYPE_BD), m);
    }

    return true;
}

ts::uint32 dialog_prepareimage_c::gm_handler(gmsg<GM_DROPFILES> &p)
{
    if (p.root == getrootrid())
    {
        load_image(p.fn);
        return GMRBIT_ABORT;
    }
    return 0;
}

bool dialog_prepareimage_c::paste_hotkey_handler(RID, GUIPARAM)
{
    selrectvisible = true;
    animated = false;
    loaded_image.clear();
    loaded_img_format = ts::if_none;

    ts::bitmap_c bmp = ts::get_clipboard_bitmap();
    if (bmp.info().sz >> 0)
    {
        bitmap = bmp;
        on_newimage();
    }

    return true;
}

bool dialog_prepareimage_c::space_key(RID, GUIPARAM)
{
    if (camera && camst == CAMST_VIEW)
    {
        camera->fit_to_size( ts::ivec2(0) ); // reset fit size
        camst = CAMST_WAIT_ORIG_SIZE;
    }

    return false;
}

bool dialog_prepareimage_c::open_image(RID, GUIPARAM)
{
    ts::wstr_c fromdir;
    if (prf().is_loaded())
        fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());

    ts::wstr_c title( loc_text(loc_image) );

    ts::extension_s extsarr[2];

    ts::wstr_c sprtfmts, sprtfmts2(CONSTWSTR("*."));
    ts::enum_supported_formats([&](const char *fmt) {sprtfmts.append(':', ts::to_wstr(fmt)); });
    sprtfmts2.append(sprtfmts);
    sprtfmts.replace_all(CONSTWSTR(":"), CONSTWSTR(", "));
    sprtfmts2.replace_all(CONSTWSTR(":"), CONSTWSTR(";*."));

    extsarr[0].desc = loc_text(loc_images);
    extsarr[0].desc.append(CONSTWSTR(" (")).append(sprtfmts).append_char(')');
    extsarr[0].ext = sprtfmts2;

    extsarr[1].desc = loc_text(loc_anyfiles);
    extsarr[1].desc.append(CONSTWSTR(" (*.*)"));
    extsarr[1].ext = CONSTWSTR("*.*");

    ts::extensions_s exts(extsarr, 2, 0);

    ts::wstr_c fn = getroot()->load_filename_dialog(fromdir, CONSTWSTR(""), exts, title);

    if (!fn.is_empty())
    {
        load_image(fn);
        if (prf().is_loaded()) prf().last_filedir(ts::fn_get_path(fn));
    }

    return true;
}

void dialog_prepareimage_c::nextframe()
{
    ts::Time curt = ts::Time::current();

    bool redraw_it = false;
    if (curt >= next_frame_tick)
    {
        next_frame_tick += anm.nextframe(bitmap.extbody());
        redraw_it = true;
        if (curt >= next_frame_tick)
            next_frame_tick = curt;
    }

    if (redraw_it)
        prepare_working_image();
}

void dialog_prepareimage_c::update_info()
{
    bool v = false;
    ts::wstr_c t(CONSTWSTR("<p=c>"));
    if (camera && !camera->still_initializing())
        t.append(loc_text(loc_space2takeimage)), v = true, camst = CAMST_VIEW;

    set_label_text(CONSTASTR("info"), t);

    if (RID no = find(CONSTASTR("info")))
        MODIFY(no).visible(v);

    gui->repos_children( this );
    dirty = true;
}

void dialog_prepareimage_c::on_newimage()
{
    camera.reset();
    camst = CAMST_NONE;
    update_info();

    user_offset = ts::ivec2(0);
    dirty = true;
    bitmapcroprect = ts::irect(0, bitmap.info().sz);
    image.clear();
    prepare_working_image();
}

void dialog_prepareimage_c::load_image(const ts::wstr_c &fn)
{
    selrectvisible = true;
    loaded_img_format = ts::if_none;
    loaded_image.load_from_file(fn);
    if (loaded_image.size())
    {
        ts::bitmap_c newb;
        if (ts::img_format_e fmt = newb.load_from_file(loaded_image))
        {
            loaded_img_format = fmt;
            saved_image = loaded_image;
            saved_img_format = fmt;
            bitmap = newb;
            animated = false;
            if (ts::if_gif == fmt)
                if (false != (animated = anm.load(loaded_image.data(), loaded_image.size())))
                {
                    selrectvisible = false;
                    next_frame_tick = ts::Time::current() + anm.firstframe(bitmap);
                }
            on_newimage();
        }
    }
}

void dialog_prepareimage_c::backscale_croprect()
{
    bitmapcroprect.lt.x = ts::lround(bckscale * croprect.lt.x);
    bitmapcroprect.lt.y = ts::lround(bckscale * croprect.lt.y);
    bitmapcroprect.rb.x = ts::tmin( bitmap.info().sz.x, ts::lround(bckscale * croprect.rb.x) );
    bitmapcroprect.rb.y = ts::tmin( bitmap.info().sz.y, ts::lround(bckscale * croprect.rb.y) );
}

bool dialog_prepareimage_c::prepare_working_image( RID, GUIPARAM )
{

    if (bitmap.info().sz >> 0) // make bitmap 32 bit per pixel
    {
        if (bitmap.info().bytepp() != 4)
        {
            ts::bitmap_c b_temp;
            b_temp = bitmap.extbody();
            bitmap = b_temp;
        }
    } else
        return true;

    float scale = ts::tmin((float)viewrect.width() / (float)bitmap.info().sz.x, (float)viewrect.height() / (float)bitmap.info().sz.y);
    if (scale < 1.0)
    {
        float mink = ts::tmax((16.0f / (float)bitmap.info().sz.x), (16.0f / (float)bitmap.info().sz.y));
        if (scale < mink) scale = mink;
        bckscale = 1.0f / scale;

        bitmap.resize_to(image, ts::ivec2(ts::lround(scale * bitmap.info().sz.x), ts::lround(scale * bitmap.info().sz.y)), ts::FILTER_LANCZOS3);

        croprect.lt.x = ts::lround(scale * bitmapcroprect.lt.x);
        croprect.lt.y = ts::lround(scale * bitmapcroprect.lt.y);
        croprect.rb.x = ts::lround(scale * bitmapcroprect.rb.x);
        croprect.rb.y = ts::lround(scale * bitmapcroprect.rb.y);

        bool backcorr = false;
        if (croprect.rb.x > image.info().sz.x)
        {
            backcorr = true;
            croprect.rb.x = image.info().sz.x;
            if (croprect.width() < 16) croprect.lt.x = croprect.rb.x - 16;
        }
        if (croprect.rb.y > image.info().sz.y)
        {
            backcorr = true;
            croprect.rb.y = image.info().sz.y;
            if (croprect.height() < 16) croprect.lt.y = croprect.rb.y - 16;
        }
        if (croprect.width() < 16) { backcorr = true; croprect.lt.x = croprect.center().x - 8; croprect.setwidth(16); }
        if (croprect.height() < 16) { backcorr = true; croprect.lt.y = croprect.center().y - 8; croprect.setheight(16); }
        if (backcorr) backscale_croprect();

    } else
    {
        croprect = bitmapcroprect;
        image = bitmap;
    }

    resave();
    alpha = image.premultiply();
    dirty = true;
    getengine().redraw();
    return true;
}

void dialog_prepareimage_c::prepare_stuff()
{
    if (!dirty) return;
    dirty = false;
    int y0 = 120;
    ts::ivec2 sz1( getprops().size() - ts::ivec2(30) );
    if (RID b = find(CONSTASTR("last")))
        y0 = HOLD(b)().getprops().pos().y + 2;
    if (RID b = find( CONSTASTR("dialog_button_1") ))
        sz1 = HOLD(b)().getprops().pos() - ts::ivec2(2,10);

    ts::irect ca = get_client_area();
    viewrect = ts::irect( ca.lt.x + 10, y0 + 2, ca.rb.x - 10, sz1.y );

    imgrect.lt = ts::ivec2(0);
    imgrect.rb = image.info().sz;
    out.lt = (viewrect.size() - imgrect.size()) / 2;
    offset = viewrect.lt + out.lt + user_offset;

    //out.rb = out.lt + imgrect.rb - viewrect.size();
    //out.lt = -out.lt;

    imgrect.intersect( viewrect - offset );
    if (viewrect.lt.x > offset.x) offset.x = viewrect.lt.x;
    if (viewrect.lt.y > offset.y) offset.y = viewrect.lt.y;

    if (imgrect.lt.x > 0)
        out.lt.x = imgrect.lt.x;
    else
        out.lt.x = viewrect.lt.x - offset.x;

    if (imgrect.lt.y > 0)
        out.lt.y = imgrect.lt.y;
    else
        out.lt.y = viewrect.lt.y - offset.y;

    if (imgrect.rb.x < image.info().sz.x)
        out.rb.x = image.info().sz.x - imgrect.rb.x;
    else
        out.rb.x = offset.x + imgrect.width() - viewrect.rb.x;

    if (imgrect.rb.y < image.info().sz.y)
        out.rb.y = image.info().sz.y - imgrect.rb.y;
    else
        out.rb.y = offset.y + imgrect.height() - viewrect.rb.y;

    inforect.lt.x = ca.lt.x + 5;
    inforect.lt.y = sz1.y + 2;

    inforect.rb.x = sz1.x;
    inforect.rb.y = ca.rb.y - 2;

    clamp_user_offset();
}

void dialog_prepareimage_c::allow_ok( bool allow )
{
    if (disabled_ok == allow)
    {
        if (RID bok = find(CONSTASTR("dialog_button_1")))
        {
            bok.call_enable(allow);
            disabled_ok = !allow;
        }
    }
}

void dialog_prepareimage_c::resave()
{
    if (animated)
        return;

    if (loaded_image.size() && bitmapcroprect == ts::irect(0,bitmap.info().sz))
    {
        saved_img_format = loaded_img_format;
        saved_image = loaded_image;
        allow_ok(true);
        return;
    }


    if (saver)
        saver->dlg = nullptr; // say goodbye to current saver

    if (nullptr == gui->mtrack(getrid(), MTT_APPDEFINED1))
    {
    }

    ts::bitmap_c b;
    b.create_ARGB( bitmapcroprect.size() );
    b.copy(ts::ivec2(0), b.info().sz, bitmap.extbody(), bitmapcroprect.lt );
    saver = TSNEW(saver_s, this, b);
    b.clear(); // ref count decreased // saver now can safely work in other thread

    saved_image.clear();

    allow_ok(false);

    g_app->add_task(saver); // run saver
}

void dialog_prepareimage_c::clamp_user_offset()
{
    if (gui->mtrack(getrid(), MTT_MOVECONTENT)) return;
    if (!imgrect)
    {
        if (user_offset != ts::ivec2(0))
        {
            user_offset = ts::ivec2(0);
            dirty = true;
            getengine().redraw();
        }
        return;
    }
    ts::ivec2 o = user_offset;
    if (image.info().sz.x <= viewrect.width())
    {
        user_offset.x /= 2;

    } else if (out.lt.x < 0)
    {
        if (out.rb.x > 0)
            user_offset.x += out.lt.x / 2;
    } else if (out.rb.x < 0)
    {
        if (out.lt.x > 0)
            user_offset.x -= out.rb.x / 2;
    }
    if (image.info().sz.y <= viewrect.height())
    {
        user_offset.y /= 2;
    } else if (out.lt.y < 0)
    {
        if (out.rb.y > 0)
            user_offset.y += out.lt.y / 2;
    } else if (out.rb.y < 0)
    {
        if (out.lt.y > 0)
            user_offset.y -= out.rb.y / 2;
    }
    if (o != user_offset)
    {
        dirty = true;
        getengine().redraw();
    }
}

void dialog_prepareimage_c::draw_process(ts::TSCOLOR col, bool cam, bool cambusy)
{
    pa.render();

    if (cam)
    {
        ts::wstr_c ainfo(loc_text(loc_initialization));
        if (cambusy) ainfo.append(CONSTWSTR("<br>")).append(loc_text(loc_camerabusy));
        draw_initialization(&getengine(), pa.bmp, viewrect, col, ainfo );
    } else
    {
        /*
        draw_data_s &dd = getengine().begin_draw();
        text_draw_params_s tdp;
        tdp.forecolor = &col;
        ts::flags32_s f(ts::TO_END_ELLIPSIS | ts::TO_VCENTER);
        tdp.textoptions = &f;
        getengine().draw(inforect.lt + ts::ivec2(0, (inforect.height() - pa.bmp.info().sz.y) / 2), pa.bmp.extbody(), ts::irect(0, pa.bmp.info().sz), true);
        int paw = pa.bmp.info().sz.x + 2;
        dd.offset = inforect.lt + ts::ivec2(paw, 0);
        dd.size = inforect.size(); dd.size.x -= paw;
        getengine().draw(TTT("Compressing...",219), tdp);
        getengine().end_draw();
        */
    }

}

void dialog_prepareimage_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);

    if (bcr.tag == 1)
    {
        bcr.btext = TTT("Send image",374);
    }
}


/*virtual*/ bool dialog_prepareimage_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid == getrid())
    {
        bool camdraw = false;

        switch (qp)
        {
        case SQ_DRAW:
            __super::sq_evt(qp, rid, data);
            prepare_stuff();

            selrectvisible = false;
            allowdndinfo = true;

            if (camera && camst != CAMST_WAIT_ORIG_SIZE)
            {
                allowdndinfo = false;
                if (!camera->still_initializing())
                {
                    if (camst != CAMST_VIEW)
                        update_info();

                    ts::ivec2 sz = viewrect.size();
                    ts::ivec2 shadow_size(0);
                    if (shadow)
                    {
                        shadow_size = ts::ivec2(shadow->clborder_x(), shadow->clborder_y());
                        sz -= shadow_size;
                    }

                    sz = camera->fit_to_size(ts::tmin(sz, camera->get_video_size() ));
                    if (ts::drawable_bitmap_c *b = camera->lockbuf(nullptr))
                    {
                        if (sz == b->info().sz)
                        {
                            getengine().begin_draw();

                            ts::ivec2 vsz = viewrect.size();
                            ts::ivec2 pos = (vsz - sz) / 2;

                            getengine().draw(pos + viewrect.lt, b->extbody(), false);

                            if (shadow)
                            {
                                evt_data_s d;
                                d.draw_thr.rect.get().lt = pos + viewrect.lt - shadow_size/2;
                                d.draw_thr.rect.get().rb = d.draw_thr.rect.get().lt + sz + shadow_size;
                                getengine().draw(*shadow, DTHRO_BORDER_RECT, &d);
                            }

                            getengine().end_draw();
                            camdraw = true;
                            allowdndinfo = false;

                        }

                        camera->unlock(b);
                    }
                }
                if (!camdraw)
                {
                    ts::TSCOLOR info_c = ts::ARGB(0, 0, 0);
                    if (const theme_rect_s *tr = themerect())
                        info_c = tr->color(2);
                    draw_process(info_c, true, camera->is_busy());
                    allowdndinfo = false;
                }
            }
            
            if (camera == nullptr && (image.info().sz > ts::ivec2(0)))
            {
                allowdndinfo = false;

                ts::TSCOLOR info_c = ts::ARGB(0,0,0);
                if (const theme_rect_s *tr = themerect())
                {
                    if (alpha)
                        draw_chessboard(getengine(), viewrect, tr->color(0), tr->color(1));
                    info_c = tr->color(2);
                }

                selrectvisible = !animated;
                if (imgrect)
                {
                    draw_data_s &dd = getengine().begin_draw();
                    dd.cliprect = viewrect;
                    getengine().draw(offset, image.extbody(imgrect), alpha);

                    if (selrectvisible)
                    {
                        ts::ivec2 o = offset - imgrect.lt;
                        fd.draw(getengine(), croprect + o, tickvalue);

                        if (croprect.lt.x > 0)
                        {
                            // draw dark at left
                            getengine().draw(ts::irect(o.x, o.y + croprect.lt.y, o.x + croprect.lt.x, o.y + croprect.rb.y), ts::ARGB(0, 0, 50, 50));
                        }
                        int rw = image.info().sz.x - croprect.rb.x - 1;
                        if (rw > 0)
                        {
                            // draw dark at rite
                            getengine().draw(ts::irect(o.x + croprect.rb.x, o.y + croprect.lt.y, o.x + image.info().sz.x, o.y + croprect.rb.y), ts::ARGB(0, 0, 50, 50));
                        }
                        if (croprect.lt.y > 0)
                        {
                            // draw dark at top
                            getengine().draw(ts::irect(o.x, o.y, o.x + image.info().sz.x, o.y + croprect.lt.y), ts::ARGB(0, 0, 50, 50));
                        }
                        int rh = image.info().sz.y - croprect.rb.y - 1;
                        if (rh > 0)
                        {
                            // draw dark at bottom
                            getengine().draw(ts::irect(o.x, o.y + croprect.rb.y, o.x + image.info().sz.x, o.y + image.info().sz.y), ts::ARGB(0, 0, 50, 50));
                        }
                    }

                    getengine().end_draw();
                }
                
                if (saver)
                {
                    draw_process( info_c, false, false );

                } else
                {
                    ts::wstr_c infostr;
                    infostr.append(text_sizebytes( saved_image.size() ));
                    infostr.append(CONSTWSTR(", "));
                    infostr.append_as_uint(bitmapcroprect.width());
                    infostr.append(CONSTWSTR(" x "));
                    infostr.append_as_uint(bitmapcroprect.height());
                    switch (saved_img_format) // -V719
                    {
                    case ts::if_none:
                        break;
                    case ts::if_png:
                        infostr.append(CONSTWSTR(", png"));
                        break;
                    case ts::if_jpg:
                        infostr.append(CONSTWSTR(", jpg"));
                        break;
                    case ts::if_gif:
                        infostr.append(CONSTWSTR(", gif"));
                        break;
                    case ts::if_dds:
                        break;
                    }

                    draw_data_s &dd = getengine().begin_draw();
                    dd.offset = inforect.lt;
                    dd.size = inforect.size();
                    text_draw_params_s tdp;
                    tdp.rectupdate = getrectupdate();
                    tdp.forecolor = &info_c;
                    ts::flags32_s f(ts::TO_END_ELLIPSIS | ts::TO_VCENTER);
                    tdp.textoptions = &f;
                    getengine().draw(infostr, tdp);
                    getengine().end_draw();

                }
            }

            if (allowdndinfo)
            {
                // empty

                draw_data_s&dd = getengine().begin_draw();

                ts::wstr_c t(CONSTWSTR("<l>"),  loc_text(loc_dropimagehere)); t.append(CONSTWSTR("</l>"));
                ts::ivec2 tsz = gui->textsize(ts::g_default_text_font, t);
                ts::ivec2 tpos = (viewrect.size() - tsz) / 2;


                if (const theme_image_s *img = gui->theme().get_image(CONSTASTR("avdroptarget")))
                {
                    ts::ivec2 imgpos(viewrect.lt + (viewrect.size() - img->info().sz) / 2);
                    imgpos.y -= tsz.y / 2 + 2;
                    img->draw(getengine(), imgpos);
                    tpos.y += img->info().sz.y/2 + 1;
                }


                ts::TSCOLOR info_c = ts::ARGB(0, 0, 0);
                if (const theme_rect_s *tr = themerect())
                    info_c = tr->color(2);


                text_draw_params_s tdp;
                tdp.forecolor = &info_c;

                dd.offset += tpos + viewrect.lt;
                dd.size = tsz;

                getengine().draw(t, tdp);

                getengine().end_draw();

            }

            return true;
        case SQ_DETECT_AREA:
            if (nullptr == gui->mtrack(getrid(), MTT_APPDEFINED1))
            {
                area = 0;
                if (data.detectarea.area == 0 && selrectvisible)
                {
                    ts::ivec2 mp = data.detectarea.pos() - offset + imgrect.lt;
                    if (ts::tabs(mp.x - croprect.lt.x) < 4) data.detectarea.area |= AREA_LEFT;
                    else if (ts::tabs(mp.x - croprect.rb.x) < 4) data.detectarea.area |= AREA_RITE;
                    
                    if (ts::tabs(mp.y - croprect.lt.y) < 4) data.detectarea.area |= AREA_TOP;
                    else if (ts::tabs(mp.y - croprect.rb.y) < 4) data.detectarea.area |= AREA_BOTTOM;
                    area = data.detectarea.area;
                    if (area & AREA_RESIZE) data.detectarea.area |= AREA_NORESIZE | AREA_FORCECURSOR;
                }

            }
            break;
        case SQ_MOUSE_RDOWN:
            if (camera == nullptr && viewrect.inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
            {
                mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_MOVECONTENT);
                opd.mpos = user_offset - data.mouse.screenpos();
                return true;
            }
            break;
        case SQ_MOUSE_LDOWN:
            if (camera == nullptr)
            {
                if (area)
                {
                    mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_APPDEFINED1);
                    opd.area = area;
                    opd.mpos = data.mouse.screenpos();
                    opd.rect = croprect;
                    return true;
                }
                else if ((croprect + offset - imgrect.lt).inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
                {
                    mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_APPDEFINED2);
                    opd.mpos = data.mouse.screenpos();
                    storecroprect = croprect;
                    return true;
                }

            }
            break;
        case SQ_MOUSE_RUP:
            if (gui->end_mousetrack(getrid(), MTT_MOVECONTENT))
                clamp_user_offset();
            break;
        case SQ_MOUSE_LUP:
            if (gui->end_mousetrack(getrid(), MTT_APPDEFINED1) || gui->end_mousetrack(getrid(), MTT_APPDEFINED2))
                resave();
            break;
        case SQ_MOUSE_MOVE_OP:
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_MOVECONTENT))
            {
                ts::ivec2 o = user_offset;
                user_offset = data.mouse.screenpos() + opd->mpos;
                if (o != user_offset)
                {
                    dirty = true;
                    getengine().redraw();
                }
            }
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_APPDEFINED1))
            {
                if ( 0 != (opd->area & AREA_LEFT) )
                {
                    croprect.lt.x = opd->rect.lt.x + data.mouse.screenpos().x - opd->mpos.x;
                    if (croprect.lt.x < 0) croprect.lt.x = 0;
                    if (croprect.width() < 16) croprect.lt.x = croprect.rb.x - 16;
                }
                else if(0 != (opd->area & AREA_RITE))
                {
                    croprect.rb.x = opd->rect.rb.x + data.mouse.screenpos().x - opd->mpos.x;
                    if (croprect.rb.x > image.info().sz.x) croprect.rb.x = image.info().sz.x;
                    if (croprect.width() < 16) croprect.rb.x = croprect.lt.x + 16;
                }

                if (0 != (opd->area & AREA_TOP))
                {
                    croprect.lt.y = opd->rect.lt.y + data.mouse.screenpos().y - opd->mpos.y;
                    if (croprect.lt.y < 0) croprect.lt.y = 0;
                    if (croprect.height() < 16) croprect.lt.y = croprect.rb.y - 16;
                }
                else if (0 != (opd->area & AREA_BOTTOM))
                {
                    croprect.rb.y = opd->rect.rb.y + data.mouse.screenpos().y - opd->mpos.y;
                    if (croprect.rb.y > image.info().sz.y) croprect.rb.y = image.info().sz.y;
                    if (croprect.height() < 16) croprect.rb.y = croprect.lt.y + 16;
                }
                backscale_croprect();
            }
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_APPDEFINED2))
            {
                croprect = storecroprect + data.mouse.screenpos() - opd->mpos;

                if (croprect.lt.x < 0)
                {
                    croprect.lt.x = 0;
                    if (croprect.width() < 16) croprect.rb.x = 16;
                }
                if (croprect.rb.x > image.info().sz.x)
                {
                    croprect.rb.x = image.info().sz.x;
                    if (croprect.width() < 16) croprect.lt.x = croprect.rb.x - 16;
                }
                if (croprect.lt.y < 0)
                {
                    croprect.lt.y = 0;
                    if (croprect.height() < 16) croprect.rb.y = 16;
                }
                if (croprect.rb.y > image.info().sz.y)
                {
                    croprect.rb.y = image.info().sz.y;
                    if (croprect.height() < 16) croprect.lt.y = croprect.rb.y - 16;
                }
                backscale_croprect();
            }
            break;
        case SQ_RECT_CHANGED:

            dirty = true;
            if (nullptr == gui->mtrack(getrid(), MTT_RESIZE))
            {
                DEFERRED_UNIQUE_CALL(0.0, DELEGATE(this, prepare_working_image), nullptr);
            } else
            {
                DEFERRED_UNIQUE_CALL(0.5, DELEGATE(this, prepare_working_image), nullptr);
            }
            break;
        }

    }

    return __super::sq_evt(qp,rid,data);
}

/*virtual*/ ts::ivec2 dialog_prepareimage_c::get_min_size() const
{
    return wsz;
}


/*virtual*/ void dialog_prepareimage_c::on_confirm()
{
    if (!prf().is_loaded() || image.info().sz == ts::ivec2(0)) return;

    if (contact_root_c *c = contacts().rfind(ck))
    {
        ts::wstr_c tmpsave(cfg().temp_folder_sendimg());
        path_expand_env(tmpsave, ts::wstr_c());
        ts::make_path(tmpsave, 0);
        ts::md5_c md5;
        md5.update(saved_image.data(), saved_image.size());
        tmpsave.append_as_hex(md5.result(), 16);
        switch (saved_img_format)
        {
        case ts::if_png:
            tmpsave.append(CONSTWSTR(".png"));
            break;
        case ts::if_jpg:
            tmpsave.append(CONSTWSTR(".jpg"));
            break;
        case ts::if_gif:
            tmpsave.append(CONSTWSTR(".gif"));
            break;
        }

        saved_image.save_to_file(tmpsave);
        c->send_file(tmpsave);
    }

    __super::on_confirm();
}



