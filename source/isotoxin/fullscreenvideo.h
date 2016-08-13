#pragma once

class fullscreenvideo_c;
template<> struct MAKE_ROOT<fullscreenvideo_c> : public _PROOT(fullscreenvideo_c)
{
    ts::ivec2 frompt;
    gui_notice_callinprogress_c *owner;
    MAKE_ROOT(const ts::ivec2 &frompt, gui_notice_callinprogress_c *owner) :_PROOT(fullscreenvideo_c)(), frompt(frompt), owner(owner) { init(RS_NORMAL); }
    ~MAKE_ROOT();
};

struct av_contact_s;

struct common_videocall_stuff_s
{
    ts::safe_ptr<gui_button_c> b_mic_mute;
    ts::safe_ptr<gui_button_c> b_spkr_mute;
    ts::safe_ptr<gui_button_c> b_cam_swtch;
    ts::safe_ptr<gui_button_c> b_options;
    ts::safe_ptr<gui_button_c> b_hangup;
    ts::safe_ptr<gui_button_c> b_fs;

    ts::ivec2 shadowsize = ts::ivec2(0);
    ts::shared_ptr<theme_rect_s> shadow;
    process_animation_s pa;

    vsb_display_c *display = nullptr;

    ts::ivec2 cam_previewsize = ts::ivec2(0);
    ts::ivec2 cam_position = ts::ivec2(0);
    ts::irect cam_previewposrect = ts::irect(0);

    ts::ivec2 display_size = ts::ivec2(0);
    ts::ivec2 display_position = ts::ivec2(0);
    ts::ivec2 cur_vres = ts::ivec2(0);

    ts::Time nommovetime = ts::Time::current();
    ts::ivec2 mousepos = ts::ivec2(0);

    int calc_rect_tag_frame = 0;
    int calc_rect_tag = -1;

    static const ts::flags32_s::BITS F_BUTTONS_VISIBLE = SETBIT(0);
    static const ts::flags32_s::BITS F_HIDDEN_CURSOR = SETBIT(1);
    static const ts::flags32_s::BITS F_RECTSOK = SETBIT(2);
    static const ts::flags32_s::BITS F_OVERPREVIEWCAM = SETBIT(3);
    static const ts::flags32_s::BITS F_MOVEPREVIEWCAM = SETBIT(4);
    static const ts::flags32_s::BITS F_CAMINITANIM = SETBIT(5);
    static const ts::flags32_s::BITS F_FULL_CAM_REDRAW = SETBIT(6);
    static const ts::flags32_s::BITS F_FULL_DISPLAY_REDRAW = SETBIT(7);
    static const ts::flags32_s::BITS F_FS = SETBIT(8);
    ts::flags32_s flags;

    static const int maxnumbuttons = 6;

    template<typename F> void for_each_button( const F &f )
    {
        f(b_mic_mute);
        if ( b_cam_swtch )
        {
            f(b_spkr_mute);
            f(b_hangup);
            f(b_cam_swtch);
            f(b_options);

        } else
        {
            f(b_hangup);
            f(b_spkr_mute);
        }
    }

    void vsb_draw(rectengine_c &eng, vsb_c *cam, const ts::ivec2& campos, const ts::ivec2& camsz, bool c, bool sh);
    void update_btns_positions(int up);
    void update_fs_pos(const ts::irect &vrect);
    void show_buttons(bool show);
    void create_buttons( gui_notice_callinprogress_c *owner, RID parent, bool video_supported );
    void tick();

    void draw_blank_cam(rectengine_c &eng, bool draw_shadow);
    bool apply_preview_cam_pos(const ts::ivec2&p);
    int preview_cam_cursor_resize(const ts::ivec2 &p) const;
    void calc_cam_display_rects(gui_notice_callinprogress_c *owner, bool show_video);

    void mouse_down(RID pnl, const ts::ivec2 &scrmpos);
    bool mouse_out();
    bool detect_area(RID pnl, evt_data_s &data, bool show_video);
    bool mouse_op(gui_notice_callinprogress_c *owner, RID pnl, const ts::ivec2 &scrmpos);

    common_videocall_stuff_s();
};

class fullscreenvideo_c : public gui_control_c
{
    friend class gui_notice_callinprogress_c;
    guirect_watch_c watch;
    ts::safe_ptr<gui_notice_callinprogress_c> owner;
    common_videocall_stuff_s common;

    bool esc_handler(RID, GUIPARAM);

protected:
    /*virtual*/ void created() override;

public:
    fullscreenvideo_c(MAKE_ROOT<fullscreenvideo_c> &data);
    ~fullscreenvideo_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void tick();
    void video_tick(vsb_display_c *display, const ts::ivec2 &vsz);
    void camera_tick();

    void set_cam_init_anim()
    {
        common.flags.set(common.F_CAMINITANIM);
    }
    void clear_cam_init_anim()
    {
        common.flags.clear(common.F_CAMINITANIM);
    }

    void cam_switch(bool on);
    void wait_anim();
};
