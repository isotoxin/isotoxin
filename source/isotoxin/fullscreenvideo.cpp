#include "isotoxin.h"

MAKE_ROOT<fullscreenvideo_c>::~MAKE_ROOT()
{
    ts::irect rect = ts::wnd_get_max_size_fs(frompt);
    MODIFY( *me ).pos( rect.lt ).size( rect.size() ).visible(true);
}

/*virtual*/ void fullscreenvideo_c::created()
{
    set_theme_rect(CONSTASTR("fullscreen"), false);
    __super::created();

    gui->register_kbd_callback(DELEGATE(this, esc_handler), SSK_ESC, 0);

    common.create_buttons( owner, getrid(), true );
}

bool fullscreenvideo_c::esc_handler(RID, GUIPARAM)
{
    TSDEL(this);
    return true;
}

fullscreenvideo_c::fullscreenvideo_c(MAKE_ROOT<fullscreenvideo_c> &data) :gui_control_c(data), watch(data.owner->getrid(), DELEGATE(this, esc_handler))
{
    owner = data.owner;
    common.display = owner->common.display;
    common.flags.set(common.F_FS);
}

fullscreenvideo_c::~fullscreenvideo_c()
{
    if (owner) owner->on_fsvideo_die();
    if (common.flags.is(common.F_HIDDEN_CURSOR))
        ts::show_hardware_cursor();

    if (gui)
    {
        gui->unregister_kbd_callback(DELEGATE(this, esc_handler));
    }
}

/*virtual*/ ts::wstr_c fullscreenvideo_c::get_name() const
{
    return ts::wstr_c();
}

/*virtual*/ bool fullscreenvideo_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid()) return false;

    switch (qp)
    {
    case SQ_DRAW:
        __super::sq_evt(qp, rid, data);
        
        if (common.display)
        {
            ts::irect r = get_client_area();
            int dw = r.width() - common.display_size.x;
            if (dw > 0)
            {
                // fill left and rite areas
                getengine().begin_draw();
                ts::irect rl = r; rl.rb.x = rl.lt.x + dw / 2 + 1;
                getengine().draw(rl, get_default_text_color(0));
                r.lt.x = r.rb.x - dw / 2 - 1;
                getengine().draw(r, get_default_text_color(0));
                getengine().end_draw();
            }

            common.vsb_draw(getengine(), common.display, common.display_position, common.display_size, false, false);

        } else
        {
            getengine().begin_draw();
            getengine().draw( get_client_area(), get_default_text_color(0) );
            getengine().end_draw();
        }

        if (owner->getcamera() && (common.cam_previewsize >> ts::ivec2(0)))
        {
            common.vsb_draw(getengine(), owner->getcamera(), common.cam_position, common.cam_previewsize, true, true);
        }
        else if (common.flags.is(common.F_CAMINITANIM))
        {
            common.draw_blank_cam(getengine(), true);
        }

        ++common.calc_rect_tag_frame;
        return true;
    case SQ_MOUSE_LDOWN:
        common.mouse_down(getrid(), data.mouse.screenpos());
        break;
    case SQ_MOUSE_OUT:
        if (common.mouse_out())
            getengine().redraw();
        break;
    case SQ_DETECT_AREA:
        common.detect_area(getrid(), data, true);
        break;

    case SQ_MOUSE_MOVE:
        break;
    case SQ_MOUSE_LUP:
        gui->end_mousetrack(getrid(), MTT_MOVECONTENT);
        gui->end_mousetrack(getrid(), MTT_APPDEFINED1);
        break;
    case SQ_MOUSE_MOVE_OP:
        if (common.mouse_op(owner, getrid(), data.mouse.screenpos()))
            getengine().redraw();
        break;
    case SQ_RECT_CHANGED:
        common.update_btns_positions(10);
        break;
    case SQ_MOUSE_L2CLICK:
        TSDEL(this);
        return true;
    }

    return __super::sq_evt(qp, rid, data);
}


void fullscreenvideo_c::tick()
{
    common.tick();
}

void fullscreenvideo_c::video_tick(vsb_display_c *display_, const ts::ivec2 &vsz)
{
    common.flags.clear(common.F_RECTSOK);
    common.display = display_;
    if (!common.display) return;

    ts::ivec2 prevp = common.display_position;
    ts::ivec2 prevs = common.display_size;

    ts::irect clr = get_client_area();

    common.display->update_video_size(vsz, clr.size());

    if (clr.size() != common.cur_vres)
    {
        common.cur_vres = clr.size();
        if (av_contact_s *avc = owner->get_avc())
            avc->set_video_res(common.cur_vres * 2 /* improove quality: vp8 too poor quality */);
    }

    common.display_size = common.display->get_desired_size();
    if (common.display_size <= 0)
        return;

    common.display_position = clr.center();
    common.flags.set(common.F_RECTSOK);
    common.calc_cam_display_rects(owner, true);

    ASSERT(common.display->notice == owner);
    if (common.flags.is(common.F_FULL_DISPLAY_REDRAW) || prevp != common.display_position || prevs != common.display_size)
    {
        flags.set(common.F_FULL_DISPLAY_REDRAW);
        getengine().redraw();
    }
    else
    {
        ts::irect clrx = ts::irect::from_center_and_size(common.display_position, common.display_size);
        getengine().redraw(&clrx);
    }

    getengine().redraw();
}

void fullscreenvideo_c::camera_tick()
{

    ts::ivec2 prevp = common.cam_position;
    ts::ivec2 prevs = common.cam_previewsize;

    common.calc_cam_display_rects(owner, true);

    if (common.flags.is(common.F_FULL_CAM_REDRAW) || prevp != common.cam_position || prevs != common.cam_previewsize)
    {
        common.flags.set(common.F_FULL_CAM_REDRAW);
        getengine().redraw();

    } else {
        ts::irect clr = ts::irect::from_center_and_size(common.cam_position, common.cam_previewsize);
        getengine().redraw(&clr);
    }

}

void fullscreenvideo_c::cam_switch(bool on)
{
    common.nommovetime = ts::Time::current();
    if (on)
    {
        set_cam_init_anim();
        common.calc_cam_display_rects(owner, true);
    }
    else
    {
        common.flags.clear(common.F_RECTSOK);
        clear_cam_init_anim();
        common.cam_previewsize = ts::ivec2(0);
        common.flags.set(common.F_FULL_CAM_REDRAW | common.F_FULL_DISPLAY_REDRAW);
        getengine().redraw();
    }
}

void fullscreenvideo_c::wait_anim()
{
    common.pa.render();
    getengine().redraw();
    if (!common.flags.is(common.F_RECTSOK))
        common.calc_cam_display_rects(owner, true);
}

common_videocall_stuff_s::common_videocall_stuff_s()
{
    shadow = gui->theme().get_rect(CONSTASTR("shadow"));
    if (shadow)
    {
        shadowsize.x = shadow->clborder_x() / 2;
        shadowsize.y = shadow->clborder_y() / 2;
    }
}

void common_videocall_stuff_s::update_btns_positions(int up)
{
    ts::irect clar = HOLD(b_hangup->getparent())().get_client_area();
    if (!clar) return;

    int w = 0;
    int mh = 0;
    int bn = 0;
    ts::ivec2 szs[maxnumbuttons];
    for_each_button([&](gui_button_c *b)
    {
        szs[bn] = b->get_min_size();
        w += szs[bn].x;
        if (szs[bn].y > mh) mh = szs[bn].y;
        ++bn;
    });

    int space = 5;
    w += (bn - 1) * space; // spaces between buttons

    up += mh / 2;
    int x = clar.lt.x + (clar.width() - w) / 2;
    int i = 0;
    for_each_button([&](gui_button_c *b)
    {
        MODIFY(*b).pos(x, clar.rb.y - up - szs[i].y / 2).size(szs[i]).visible(true);
        x += szs[i].x + space;
        ++i;
    });
}

void common_videocall_stuff_s::update_fs_pos(const ts::irect &vrect)
{
    if (b_fs)
    {
        ts::ivec2 s = b_fs->get_min_size();
        MODIFY(*b_fs).pos(ts::ivec2(vrect.rb.x - s.x, vrect.lt.y)).size(s).visible(flags.is(F_BUTTONS_VISIBLE));
    }
}


void common_videocall_stuff_s::show_buttons(bool show)
{
    for_each_button([&](gui_button_c *b)
    {
        MODIFY(*b).visible(show);
    });
    if (b_fs) MODIFY(*b_fs).visible(show);
    bool oldvb = flags.is(F_BUTTONS_VISIBLE);
    flags.init(F_BUTTONS_VISIBLE, show);

    if (oldvb != flags.is(F_BUTTONS_VISIBLE))
        flags.set(F_FULL_DISPLAY_REDRAW);
}

void common_videocall_stuff_s::create_buttons( gui_notice_callinprogress_c *owner, RID parent, bool video_supported )
{
    contact_c * collocutor_ptr = owner->collocutor();
    av_contact_s &avc = *owner->get_avc();

    gui_button_c &b_reject = MAKE_CHILD<gui_button_c>(parent);
    b_hangup = &b_reject;
    b_reject.set_face_getter(BUTTON_FACE(reject_call));
    b_reject.set_handler(DELEGATE(collocutor_ptr, b_hangup), owner);

    gui_button_c &b_mute_mic = MAKE_CHILD<gui_button_c>(parent);
    b_mic_mute = &b_mute_mic;

    avc.is_mic_off() ?
        b_mute_mic.set_face_getter(BUTTON_FACE(unmute_mic)) :
        b_mute_mic.set_face_getter(BUTTON_FACE(mute_mic));

    b_mute_mic.set_handler(DELEGATE(&avc, b_mic_switch), &b_mute_mic);

    gui_button_c &b_mute_speaker = MAKE_CHILD<gui_button_c>(parent);
    b_spkr_mute = &b_mute_speaker;

    avc.is_speaker_off() ?
        b_mute_speaker.set_face_getter(BUTTON_FACE(unmute_speaker)) :
        b_mute_speaker.set_face_getter(BUTTON_FACE(mute_speaker));

    b_mute_speaker.set_handler(DELEGATE(&avc, b_speaker_switch), &b_mute_speaker);

    if (video_supported)
    {
        gui_button_c &b_cam = MAKE_CHILD<gui_button_c>(parent);
        b_cam_swtch = &b_cam;
        avc.update_btn_face_camera(b_cam);
        b_cam.set_handler(DELEGATE(owner, b_camera_switch), nullptr);

        gui_button_c &b_extra = MAKE_CHILD<gui_button_c>(parent);
        b_options = &b_extra;
        b_extra.set_face_getter(BUTTON_FACE(extra));
        b_extra.set_handler(DELEGATE(owner, b_extra), nullptr);
        
        if (owner->getrid() == parent)
        {
            gui_button_c &_b_fs = MAKE_CHILD<gui_button_c>(parent);
            b_fs = &_b_fs;
            _b_fs.set_face_getter(BUTTON_FACE(vfs));
            _b_fs.set_handler(DELEGATE(owner, b_fs), nullptr);
        }
    }
}

void common_videocall_stuff_s::tick()
{
    ts::ivec2 mp = gui->get_cursor_pos();
    bool inside = false;

    if (HOLD(b_hangup->getparent())().getprops().screenrect().inside(mp))
    {
        if (mp != mousepos)
        {
            nommovetime = ts::Time::current();
            show_buttons(true);
        }
        inside = true;
    }

    if ((ts::Time::current() - nommovetime) > 3000)
    {
        show_buttons(false);
        if (inside)
            ts::hide_hardware_cursor(), flags.set(F_HIDDEN_CURSOR);
    }

    if (mp != mousepos && F_HIDDEN_CURSOR)
    {
        ts::show_hardware_cursor();
    }

    mousepos = mp;

}


void common_videocall_stuff_s::draw_blank_cam(rectengine_c &eng, bool draw_shadow)
{
    draw_data_s &dd = eng.begin_draw();
    ts::irect clr = ts::irect::from_center_and_size(cam_position, cam_previewsize);

    eng.draw(clr, ts::ARGB(30, 30, 30));
    eng.draw(clr.center() - pa.bmp.info().sz / 2, pa.bmp.extbody(), true);

    if (draw_shadow && shadow)
    {
        clr.lt -= shadowsize;
        clr.rb += shadowsize;
        clr += dd.offset;
        evt_data_s d;
        d.draw_thr.rect.get() = clr;
        eng.draw(*shadow, DTHRO_BORDER_RECT, &d);
    }
    eng.end_draw();

};

bool common_videocall_stuff_s::apply_preview_cam_pos(const ts::ivec2&p)
{
    camview_snap_e s = (camview_snap_e)prf().camview_snap();
    int shift = prf().camview_pos();
    if (shift < 0) shift = 0;

#define IS_LEFT_AXIS (CVS_LEFT_TOP == s || CVS_LEFT_BOTTOM == s)
#define IS_RITE_AXIS (CVS_RITE_TOP == s || CVS_RITE_BOTTOM == s)

#define IS_TOP_AXIS (CVS_TOP_LEFT == s || CVS_TOP_RITE == s)
#define IS_BOTTOM_AXIS (CVS_BOTTOM_LEFT == s || CVS_BOTTOM_RITE == s)

#define AT_TOP (CVS_LEFT_TOP == s || CVS_RITE_TOP == s)
#define AT_LEFT (CVS_TOP_LEFT == s || CVS_BOTTOM_LEFT == s)

    switch (s)
    {
    case CVS_LEFT_TOP:
    case CVS_RITE_TOP:
    case CVS_LEFT_BOTTOM:
    case CVS_RITE_BOTTOM:
    {
        // vertical move allowed
        cam_position.y = p.y;
        if (cam_position.y < cam_previewposrect.lt.y)
            cam_position.y = cam_previewposrect.lt.y;
        if (cam_position.y > cam_previewposrect.rb.y)
            cam_position.y = cam_previewposrect.rb.y;

        if ((cam_position.y - cam_previewposrect.lt.y) < (cam_previewposrect.rb.y - cam_position.y))
        {
            // top
            shift = cam_position.y - cam_previewposrect.lt.y;
            s = IS_LEFT_AXIS ? CVS_LEFT_TOP : CVS_RITE_TOP;
        }
        else
        {
            // bottom
            shift = cam_previewposrect.rb.y - cam_position.y;
            s = IS_LEFT_AXIS ? CVS_LEFT_BOTTOM : CVS_RITE_BOTTOM;
        }

        if (shift == 0)
        {
            // horizontal move allowed
            cam_position.x = p.x;

            if (cam_position.x < cam_previewposrect.lt.x)
                cam_position.x = cam_previewposrect.lt.x;
            if (cam_position.x > cam_previewposrect.rb.x)
                cam_position.x = cam_previewposrect.rb.x;

            if (cam_position.x == cam_previewposrect.lt.x && IS_LEFT_AXIS)
                break; // still at left axis, do nothing

            if (cam_position.x == cam_previewposrect.rb.x && IS_RITE_AXIS)
                break; // still at rite axis, do nothing

            if ((cam_position.x - cam_previewposrect.lt.x) < (cam_previewposrect.rb.x - cam_position.x))
            {
                // left
                shift = cam_position.x - cam_previewposrect.lt.x;
                s = AT_TOP ? CVS_TOP_LEFT : CVS_BOTTOM_LEFT;
            }
            else
            {
                // rite
                shift = cam_previewposrect.rb.x - cam_position.x;
                s = AT_TOP ? CVS_TOP_RITE : CVS_BOTTOM_RITE;
            }

        }
    }
    break;
    case CVS_TOP_LEFT:
    case CVS_TOP_RITE:
    case CVS_BOTTOM_LEFT:
    case CVS_BOTTOM_RITE:
    {
        // horizontal move allowed
        cam_position.x = p.x;
        if (cam_position.x < cam_previewposrect.lt.x)
            cam_position.x = cam_previewposrect.lt.x;
        if (cam_position.x > cam_previewposrect.rb.x)
            cam_position.x = cam_previewposrect.rb.x;

        if ((cam_position.x - cam_previewposrect.lt.x) < (cam_previewposrect.rb.x - cam_position.x))
        {
            // left
            shift = cam_position.x - cam_previewposrect.lt.x;
            s = IS_TOP_AXIS ? CVS_TOP_LEFT : CVS_BOTTOM_LEFT;
        }
        else
        {
            // rite
            shift = cam_previewposrect.rb.x - cam_position.x;
            s = IS_TOP_AXIS ? CVS_TOP_RITE : CVS_BOTTOM_RITE;
        }

        if (shift == 0)
        {
            // vertical move allowed
            cam_position.y = p.y;

            if (cam_position.y < cam_previewposrect.lt.y)
                cam_position.y = cam_previewposrect.lt.y;
            if (cam_position.y > cam_previewposrect.rb.y)
                cam_position.y = cam_previewposrect.rb.y;

            if (cam_position.y == cam_previewposrect.lt.y && IS_TOP_AXIS)
                break; // still at top axis, do nothing

            if (cam_position.y == cam_previewposrect.rb.y && IS_BOTTOM_AXIS)
                break; // still at bottom axis, do nothing

            if ((cam_position.y - cam_previewposrect.lt.y) < (cam_previewposrect.rb.y - cam_position.y))
            {
                // top
                shift = cam_position.y - cam_previewposrect.lt.y;
                s = AT_LEFT ? CVS_LEFT_TOP : CVS_RITE_TOP;

            }
            else
            {
                // bottom
                shift = cam_previewposrect.rb.y - cam_position.y;
                s = AT_LEFT ? CVS_LEFT_BOTTOM : CVS_RITE_BOTTOM;
            }

        }
    }
    break;
    }

    bool ch1 = prf().camview_snap(s);
    bool ch2 = prf().camview_pos(shift);
    return ch1 || ch2;
}

int common_videocall_stuff_s::preview_cam_cursor_resize(const ts::ivec2 &p) const
{
    int area = 0;

    int allow_area = 0;
    camview_snap_e s = (camview_snap_e)prf().camview_snap();
    int shift = prf().camview_pos();
    if (shift < 0) shift = 0;

    switch (s)
    {
    case CVS_LEFT_TOP:
        allow_area |= AREA_RITE | AREA_BOTTOM;
        if (shift) allow_area |= AREA_TOP;
        break;
    case CVS_LEFT_BOTTOM:
        allow_area |= AREA_RITE | AREA_TOP;
        if (shift) allow_area |= AREA_BOTTOM;
        break;
    case CVS_TOP_LEFT:
        allow_area |= AREA_BOTTOM | AREA_RITE;
        if (shift) allow_area |= AREA_LEFT;
        break;
    case CVS_TOP_RITE:
        allow_area |= AREA_BOTTOM | AREA_LEFT;
        if (shift) allow_area |= AREA_RITE;
        break;
    case CVS_RITE_TOP:
        allow_area |= AREA_LEFT | AREA_BOTTOM;
        if (shift) allow_area |= AREA_TOP;
        break;
    case CVS_RITE_BOTTOM:
        allow_area |= AREA_LEFT | AREA_TOP;
        if (shift) allow_area |= AREA_BOTTOM;
        break;
    case CVS_BOTTOM_LEFT:
        allow_area |= AREA_TOP | AREA_RITE;
        if (shift) allow_area |= AREA_LEFT;
        break;
    case CVS_BOTTOM_RITE:
        allow_area |= AREA_TOP | AREA_LEFT;
        if (shift) allow_area |= AREA_RITE;
        break;
    }

    if (flags.is(F_RECTSOK))
    {
        ts::irect r = ts::irect::from_center_and_size(cam_position, cam_previewsize);
        r.lt -= 2;
        r.rb += 2;
        if (r.inside(p))
        {
            if (allow_area & AREA_TOP)
                if ((p.y - r.lt.y) <= 4)
                    area |= AREA_TOP;

            if (allow_area & AREA_BOTTOM)
                if ((r.rb.y - p.y) <= 4)
                    area |= AREA_BOTTOM;

            if (allow_area & AREA_LEFT)
                if ((p.x - r.lt.x) <= 4)
                    area |= AREA_LEFT;

            if (allow_area & AREA_RITE)
                if ((r.rb.x - p.x) <= 4)
                    area |= AREA_RITE;
        }
    }


    return area;
}


void common_videocall_stuff_s::mouse_down(RID pnl, const ts::ivec2 &scrmpos)
{
    if (int area = preview_cam_cursor_resize(HOLD(pnl)().to_local(scrmpos)))
    {
        mousetrack_data_s &opd = gui->begin_mousetrack(pnl, MTT_APPDEFINED1);
        opd.area = area;
        opd.mpos = scrmpos;
        opd.rect.rb = cam_previewsize;

    }
    else if (flags.is(F_OVERPREVIEWCAM))
    {
        flags.set(F_MOVEPREVIEWCAM);
        mousetrack_data_s &opd = gui->begin_mousetrack(pnl, MTT_MOVECONTENT);
        opd.mpos = cam_position - scrmpos;
    }

}

bool common_videocall_stuff_s::mouse_out()
{
    bool prev = flags.is(F_OVERPREVIEWCAM);
    flags.clear(F_OVERPREVIEWCAM | F_MOVEPREVIEWCAM);
    return prev;
}

bool common_videocall_stuff_s::detect_area(RID pnl, evt_data_s &data, bool show_video)
{
    bool ret = false;
    if (nullptr == gui->mtrack(pnl, MTT_MOVECONTENT))
    {
        bool prev = flags.is(F_OVERPREVIEWCAM);
        if (prev != flags.init(F_OVERPREVIEWCAM, flags.is(F_RECTSOK) && show_video && ts::irect::from_center_and_size(cam_position, cam_previewsize).inside(data.detectarea.pos)))
            ret = true;

        data.detectarea.area = preview_cam_cursor_resize(data.detectarea.pos);

        if (flags.is(F_OVERPREVIEWCAM) && data.detectarea.area == 0)
            data.detectarea.area = AREA_MOVECURSOR;
    }
    else
        data.detectarea.area = AREA_MOVECURSOR;

    if (mousetrack_data_s *opd = gui->mtrack(pnl, MTT_APPDEFINED1))
        data.detectarea.area = opd->area;

    if (data.detectarea.area & AREA_RESIZE) data.detectarea.area |= AREA_NORESIZE;

    return ret;
}

static ts::ivec2 calc_preview_campos(const ts::irect& camrect, const ts::ivec2& camsz)
{
    int shift = prf().camview_pos();
    if (shift < 0) shift = 0;
    ts::ivec2 p(camrect.lt);

    auto clamp_left = [&]()
    {
        if (p.x < camrect.lt.x)
            p.x = camrect.lt.x;
    };

    auto clamp_top = [&]()
    {
        if (p.y < camrect.lt.y)
            p.y = camrect.lt.y;
    };

    auto clamp_rite = [&]()
    {
        if (p.x > camrect.rb.x)
            p.x = camrect.rb.x;
    };

    auto clamp_bottom = [&]()
    {
        if (p.y > camrect.rb.y)
            p.y = camrect.rb.y;
    };

    switch (prf().camview_snap())
    {
    case CVS_LEFT_TOP:
        p = camrect.lt;
        p.y += shift;
        clamp_bottom();
        break;

    case CVS_LEFT_BOTTOM:
        p = camrect.lb();
        p.y -= shift;
        clamp_top();
        break;

    case CVS_RITE_TOP:
        p = camrect.rt();
        p.y += shift;
        clamp_bottom();
        break;

    case CVS_RITE_BOTTOM:
        p = camrect.rb;
        p.y -= shift;
        clamp_top();
        break;

    case CVS_TOP_LEFT:
        p = camrect.lt;
        p.x += shift;
        clamp_rite();
        break;

    case CVS_TOP_RITE:
        p = camrect.rt();
        p.x -= shift;
        clamp_left();
        break;

    case CVS_BOTTOM_LEFT:
        p = camrect.lb();
        p.x += shift;
        clamp_rite();
        break;

    case CVS_BOTTOM_RITE:
        p = camrect.rb;
        p.x -= shift;
        clamp_left();
        break;

    }
    return p;
}


void common_videocall_stuff_s::calc_cam_display_rects(gui_notice_callinprogress_c *owner, bool show_video)
{
    if (calc_rect_tag == calc_rect_tag_frame) return;
    calc_rect_tag = calc_rect_tag_frame;

    flags.clear(F_RECTSOK);

    ts::irect clr = HOLD(b_hangup->getparent())().get_client_area();

    auto prepare_cam_draw = [&]()
    {
        cam_previewposrect = clr;
        cam_previewposrect.lt += cam_previewsize / 2;
        cam_previewposrect.rb -= cam_previewsize;
        cam_previewposrect.rb += cam_previewsize / 2;

        if (shadow)
        {
            if (flags.is(F_FS))
                cam_previewposrect.lt.y += shadowsize.y;
            else
                cam_previewposrect.lt.y -= shadowsize.y;
            cam_previewposrect.lt.x += shadowsize.x;
            cam_previewposrect.rb -= shadowsize;
        }

        cam_position = calc_preview_campos(cam_previewposrect, cam_previewsize);
    };

    if (show_video && display)
    {
        display_size = display->get_desired_size();
        if (display_size <= 0)
            return;

        clr.lt.y = clr.rb.y - display_size.y;
        display_position = clr.center();
        if (!flags.is(F_FS))
            display_position.y -= shadowsize.y;

        cam_previewsize = display_size * prf().camview_size() / 100;
        if (owner->getcamera())
        {
            ts::ivec2 csz = owner->getcamera()->get_video_size();
            if (csz.x)
            {
                float xy = (float)csz.y / (float)csz.x;
                cam_previewsize.y = ts::lround(xy * cam_previewsize.x);
            }
        }
        if (pa.bmp.info().sz > cam_previewsize)
        {
            float k1 = (float)pa.bmp.info().sz.x / display_size.x;
            float k2 = (float)pa.bmp.info().sz.y / display_size.y;
            float k = ts::tmax(k1, k2);
            cam_previewsize.x = ts::lround(k * display_size.x);
            cam_previewsize.y = ts::lround(k * display_size.y);
        }

        if (owner->getcamera())
            owner->getcamera()->set_desired_size(cam_previewsize);

        prepare_cam_draw();
    }

    if (cam_previewsize >> 0)
        flags.set(F_RECTSOK);
}

bool common_videocall_stuff_s::mouse_op(gui_notice_callinprogress_c *owner, RID pnl, const ts::ivec2 &scrmpos)
{
    vsb_c *camera = owner->getcamera();

    if (!display || !camera)
    {
        gui->end_mousetrack(pnl, MTT_MOVECONTENT);
        gui->end_mousetrack(pnl, MTT_APPDEFINED1);
        return false;
    }
    
    bool rdr = false;
    if (mousetrack_data_s *opd = gui->mtrack(pnl, MTT_MOVECONTENT))
        if (apply_preview_cam_pos(scrmpos + opd->mpos))
            rdr = true;

    if (mousetrack_data_s *opd = gui->mtrack(pnl, MTT_APPDEFINED1))
    {
        ts::ivec2 osz = opd->rect.rb;
        ts::ivec2 csz = camera->get_video_size();
        ts::ivec2 dsz = display->get_desired_size();
        float xy = (float)csz.y / (float)csz.x;

        if (opd->area & AREA_LEFT)
            osz.x = opd->rect.rb.x + opd->mpos.x - scrmpos.x, osz.y = ts::lround(xy * osz.x);
        else if (opd->area & AREA_RITE)
            osz.x = opd->rect.rb.x + scrmpos.x - opd->mpos.x, osz.y = ts::lround(xy * osz.x);

        if (opd->area & AREA_TOP)
            osz.y = opd->rect.rb.y + opd->mpos.y - scrmpos.y, osz.x = ts::lround(osz.y / xy);
        else if (opd->area & AREA_BOTTOM)
            osz.y = opd->rect.rb.y + scrmpos.y - opd->mpos.y, osz.x = ts::lround(osz.y / xy);

        osz.y = ts::lround(xy * osz.x);

        if (pa.bmp.info().sz > osz)
        {
            float k1 = (float)pa.bmp.info().sz.x / dsz.x;
            float k2 = (float)pa.bmp.info().sz.y / dsz.y;
            float k = ts::tmax(k1, k2);
            osz.x = ts::lround(k * dsz.x);
            osz.y = ts::lround(k * dsz.y);
        }

        int k = osz.x * 100 / dsz.x;
        if (k > 50) k = 50;

        prf().camview_size(k);

        calc_cam_display_rects(owner, true);
       
        rdr = true;
    }
    return rdr;
}

void common_videocall_stuff_s::vsb_draw(rectengine_c &eng, vsb_c *cam, const ts::ivec2& campos, const ts::ivec2& camsz, bool c, bool sh)
{
    if (!cam || camsz == ts::ivec2(0)) return;

    if (ts::drawable_bitmap_c *b = cam->lockbuf(nullptr))
    {
        if (camsz != b->info().sz)
        {
            ts::drawable_bitmap_c bstore = std::move(*b);
            b->create(camsz);
            if (bstore.info().sz >> ts::ivec2(0))
                bstore.resize_to(b->extbody(), ts::FILTER_BICUBIC); // bicubic enough
            else
                b->fill(ts::ARGB(0, 0, 0));
        }

        draw_data_s &dd = eng.begin_draw();
        ts::irect clr = ts::irect::from_center_and_size(campos, camsz);

        eng.draw(clr.lt, b->extbody(), false);

        if (sh && shadow)
        {
            clr.lt -= shadowsize;
            clr.rb += shadowsize;
            clr += dd.offset;
            evt_data_s d;
            d.draw_thr.rect.get() = clr;
            eng.draw(*shadow, DTHRO_BORDER_RECT, &d);
        }

        eng.end_draw();
        cam->unlock(b);

        if (c)
            flags.clear(F_FULL_CAM_REDRAW);
        else
            flags.clear(F_FULL_DISPLAY_REDRAW);
    }
}

