#pragma once

#define TABSELMI(mask) gettabsel(), ts::amake((ts::uint32)mask)

bool TSCALL dialog_already_present( int );

struct gui_listitem_params_s
{
    ts::wstr_c text;
    ts::str_c  param;
    ts::str_c  themerect;
    GETMENU_FUNC gm;
    const ts::bitmap_c *icon = nullptr;
    const ts::bmpcore_exbody_s *eb = nullptr;
    int addmarginleft_icon = 0;
    int addmarginleft = 3;
};
class gui_listitem_c;
template<> struct MAKE_CHILD<gui_listitem_c> : public _PCHILD(gui_listitem_c), public gui_listitem_params_s
{
    MAKE_CHILD(RID parent_, const ts::wstr_c &text_, const ts::str_c &param_)
    {
        parent = parent_;
        text = text_;
        param = param_;
    }
    ~MAKE_CHILD();

    MAKE_CHILD &operator<<(const ts::bmpcore_exbody_s &_icon) { eb = &_icon; return *this; }
    MAKE_CHILD &operator<<( const ts::bitmap_c *_icon ) { icon = _icon; return *this; }
    MAKE_CHILD &operator<<( GETMENU_FUNC _gm ) { gm = _gm; return *this; }
    MAKE_CHILD &threct( const ts::asptr&thr ) { themerect = thr; return *this; }
};

class gui_listitem_c : public gui_label_ex_c
{
    friend struct MAKE_CHILD<gui_listitem_c>;
    DUMMY(gui_listitem_c);
    ts::str_c  param;
    GETMENU_FUNC gm; // get menu on rite click
    ts::bitmap_c icon;
    int height = 0;
    int marginleft_icon;

    static const ts::flags32_s::BITS F_MENU_ON_LCLICK = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS F_MENU_ON_RCLICK = FLAGS_FREEBITSTART_LABEL << 1;

    GM_RECEIVER( gui_listitem_c, GM_POPUPMENU_DIED );
public:

    gui_listitem_c(initial_rect_data_s &data, gui_listitem_params_s &prms);
    gui_listitem_c(MAKE_CHILD<gui_listitem_c> &data) :gui_listitem_c(data, data) {}

    /*virtual*/ ~gui_listitem_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ void set_text(const ts::wstr_c&text, bool full_height_last_line = false) override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    /*virtual*/ int get_height_by_width(int width) const override;

    const ts::str_c  &getparam() const {return param;}
    void setparam(const ts::str_c &p) { param = p; };

    void set_gm(GETMENU_FUNC gm_, bool on_left_click = false, bool on_rite_click = true)
    {
        gm = gm_;
        flags.init( F_MENU_ON_LCLICK, on_left_click );
        flags.init( F_MENU_ON_RCLICK, on_rite_click );
    }
    void set_icon( const ts::bmpcore_exbody_s &eb );
    void set_icon(const ts::bitmap_c &b) { icon = b; if (b.info().sz.y > height) height = b.info().sz.y; }
};

class gui_dialog_c : public gui_vscrollgroup_c
{
    DUMMY(gui_dialog_c);

    GM_RECEIVER(gui_dialog_c, GM_DIALOG_PRESENT);
    GM_RECEIVER(gui_dialog_c, GM_UI_EVENT);

    ts::wstr_c captiontext;
    ts::smart_int numtopbuttons;
    int skipctls = 0;
    ts::uint32 tabselmask = 0;
    ts::irect border = ts::irect(0);
    ts::hashmap_t<ts::str_c, guirect_c::sptr_t> ctl_by_name;

    friend class gui_textfield_c;

    void update_header();

protected:

    virtual int unique_tag() { return 0; }
    virtual void on_close() { TSDEL(this); }
    virtual void on_confirm() { TSDEL(this); }

    ts::str_c find( RID crid ) const;
    RID find( const ts::asptr &name ) const;
    void setctlname( const ts::asptr &name, guirect_c &r );

    struct behav_s
    {
        enum evt_e
        {
            EVT_ON_CREATE,
            EVT_ON_OPEN_MENU,
            EVT_ON_CLICK,
        } e;
        ts::str_c param;
        menu_c *menu;
    };

    struct description_s
    {
        MOVABLE( true );

        struct ctlptr_s
        {
            MOVABLE( true );
            ts::safe_ptr<guirect_c> ptr;
            int what = -1; // -1 - desc; 0 - base ctrl
            ctlptr_s() {}
            ctlptr_s( guirect_c *r, int what ):ptr(r), what(what) {}
        };

        ts::array_inplace_t<ctlptr_s,0> ctlptrs;
        ts::wstr_c build_desc_text() const;
        void changedesc( const ts::wstr_c& desc_ );
        void setctlptr( guirect_c *r, int what );
        bool present( RID r ) const
        {
            for( const ctlptr_s &rr : ctlptrs )
                if (!rr.ptr.expired() && rr.ptr->getrid() == r) return true;
            return false;
        }


        int subctltag = -1;
        ts::str_c name;
        ts::uint32 mask = 0;
        ts::uint32 c_mask = 0;
        ts::wstr_c desc;
        ts::wstr_c text;
        ts::wstr_c hint;
        menu_c items;
        gui_textedit_c::TEXTCHECKFUNC textchecker;
        GUIPARAMHANDLER handler = GUIPARAMHANDLER();
        enum ctl_e
        {
            _CAPTION,
            _HEADER,
            _HGROUP,
            _VSPACE,
            _PANEL,
            _STATIC,
            _STATIC_HIDDEN,
            _PATH,
            _FILE,
            _TEXT,
            _ROTEXT,
            _PASSWD,
            _SELECTOR,
            _COMBIK,
            _LIST,
            _CHECKBUTTONS,
            _RADIO,
            _BUTTON,
            _HSLIDER,
        };
        ctl_e ctl;
        int width_ = 0;
        int height_ = 0;

        static const ts::flags32_s::BITS o_visible = SETBIT(0);
        static const ts::flags32_s::BITS o_focus = SETBIT(1);
        static const ts::flags32_s::BITS o_readonly = SETBIT(2);
        static const ts::flags32_s::BITS o_changed = SETBIT(3);
        static const ts::flags32_s::BITS o_err = SETBIT(4);
        ts::flags32_s options = o_visible;

        DEBUGCODE(bool initialization = true;) // protection flag - [this] can be changed while [initialization] == true due resize of array realloc.

        bool updvalue( const ts::wstr_c &t, bool changed )
        {
            ASSERT( _TEXT == ctl || _PASSWD == ctl || _PATH == ctl || _FILE == ctl );
            options.clear( o_changed );
            if (textchecker && textchecker(t, changed ))
            {
                if (!options.is( o_changed ))
                    text = t;
                options.clear( o_changed );
                return true;
            }
            return false;
        }
        bool updvalue2( RID r, GUIPARAM p );

        menu_c getcontextmenu( const ts::str_c& param, bool activation ) { return items; }

        button_desc_s *getbface() const
        {
            ASSERT(text.begins(CONSTWSTR("face=")));
            return gui->theme().get_button( ts::to_str(text.substr(5)) );
        }
        GET_BUTTON_FACE getgetface() const
        {
            ASSERT(!initialization);
            return DELEGATE(this, getbface);
        }

        GET_TOOLTIP gethintproc() const
        {
            ASSERT(!initialization);
            return DELEGATE(this, gethint);
        }
        ts::wstr_c gethint() const {return hint;}

        description_s&custom_mask(ts::uint32 cmask) { c_mask = cmask; return *this; }
        description_s&sethint(const ts::wsptr&h) { hint=h; return *this; }
        description_s&setname(const ts::asptr&n) { name=n; return *this; }
        description_s&setmenu( const menu_c &m ) { items = m; return *this; }
        description_s&readonly(bool f = true) { options.init( o_readonly, f ); return *this; }
        description_s&focus(bool f = true) { options.init( o_focus, f ); return *this; }
        description_s&visible(bool f) { options.init( o_visible, f ); return *this; }
        description_s&width(int w) { width_ = w; return *this; }
        description_s&height(int h) { height_ = h; return *this; }
        description_s&size(int w, int h) { width_ = w; height_ = h; return *this; }
        description_s&subctl(int tag,ts::wstr_c &ctldesc);
        description_s&passwd(bool p) { ASSERT(_TEXT == ctl || _PASSWD == ctl); ctl = p ? _PASSWD : _TEXT; return *this; }

        void hgroup( const ts::wsptr& desc );
        description_s& label( const ts::wsptr& text );
        description_s& hiddenlabel( const ts::wsptr& text, bool is_err );
        void page_caption( const ts::wsptr& text );
        void page_header( const ts::wsptr& text );
        description_s& panel(int h, GUIPARAMHANDLER drawhandler = nullptr);
        description_s& vspace( int h = 5, GUIPARAMHANDLER oncreatehanler = nullptr );
        description_s& selector( const ts::wsptr &desc, const ts::wsptr &t, GUIPARAMHANDLER behaviourhandler = nullptr );
        description_s& path( const ts::wsptr &desc, const ts::wsptr &path, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC() );
        description_s& file( const ts::wsptr &desc, const ts::wsptr &iroot, const ts::wsptr &fn, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC() );
        description_s& rotext( const ts::wsptr &desc, const ts::wsptr &val );
        description_s& textfield( const ts::wsptr &desc, const ts::wsptr &val, gui_textedit_c::TEXTCHECKFUNC checker);
        description_s& textfieldml( const ts::wsptr &desc, const ts::wsptr &val, gui_textedit_c::TEXTCHECKFUNC checker, int lines = 3); // multiline
        description_s& combik( const ts::wsptr &desc);
        description_s& list( const ts::wsptr &desc, const ts::wsptr &emptytext, int lines );
        description_s& checkb( const ts::wsptr &desc, GUIPARAMHANDLER handler, ts::uint32 initial );
        description_s& radio( const ts::wsptr &desc, GUIPARAMHANDLER handler, int initial );
        description_s& button(const ts::wsptr &desc, const ts::wsptr &text, GUIPARAMHANDLER handler);
        description_s& hslider(const ts::wsptr &desc, float val, const ts::wsptr &initstr, GUIPARAMHANDLER handler);
        

        RID make_ctl(gui_dialog_c *dlg, RID parent);
    };

    ts::hashmap_t<int, ts::safe_ptr<guirect_c>> subctls;
    void updrect_def(const void *, int r, const ts::ivec2 &p);
    void removerctl(int r);

    typedef ts::array_inplace_t<description_s, 0> descarray;
    descarray descs;
    void tabsel(const ts::str_c& par);
    MENUHANDLER gettabsel() { return DELEGATE(this, tabsel); };

    struct descmaker
    {
        gui_dialog_c*dlg;
        descarray& arr;
        ts::uint32 mask = 0;
        descmaker(gui_dialog_c*dlg):dlg(dlg), arr(dlg->descs) { ASSERT(arr.size() == 0); }
        ~descmaker()
        {
#ifndef _FINAL
            for(description_s &d : arr)
                d.initialization = false;
#endif
        }
        description_s& operator()() { description_s& d = arr.add(); d.mask = mask; return d; }
        void operator << (ts::uint32 m) {mask = m;}
        void operator=(const descmaker&) UNUSED;
        descmaker(const descmaker&) UNUSED;
    };

    ts::wstr_c header_prepend;
    ts::wstr_c header_append;
    ts::wstr_c label_path_selector_caption;
    ts::wstr_c label_file_selector_caption;

    bool file_selector(RID, GUIPARAM prm);
    bool path_selector(RID, GUIPARAM prm);
    bool path_explore(RID, GUIPARAM prm);
    bool combo_drop(RID, GUIPARAM prm);
    bool custom_menu(RID, GUIPARAM prm);
    bool passw_hide_show( RID, GUIPARAM prm );
    bool copy_text( RID, GUIPARAM prm );

    struct radio_item_s
    {
        MOVABLE( true );
        ts::wstr_c text;
        GUIPARAM param;
        ts::str_c name;
        radio_item_s(const ts::wstr_c &text, GUIPARAM param):text(text), param(param) {}
        radio_item_s(const ts::wstr_c &text, GUIPARAM param, const ts::asptr& name):text(text), param(param), name(name) {}
    };
    struct check_item_s
    {
        MOVABLE( true );
        ts::wstr_c text;
        ts::uint32 mask;
        ts::str_c name;
        check_item_s(const ts::wstr_c &text, ts::uint32 mask) :text(text), mask(mask) {}
        check_item_s(const ts::wstr_c &text, ts::uint32 mask, const ts::asptr& name) :text(text), mask(mask), name(name) {}
    };

    enum tfrole_e
    {
        TFR_TEXT_FILED,
        TFR_TEXT_FILED_PASSWD,
        TFR_TEXT_FILED_RO,
        TFR_PATH_SELECTOR,
        TFR_PATH_VIEWER,
        TFR_FILE_SELECTOR,
        TFR_CUSTOM_SELECTOR,
        TFR_COMBO,
    };

    RID hgroup( const ts::wsptr& desc );
    int radio( const ts::array_wrapper_c<const radio_item_s> & items, GUIPARAMHANDLER handler, GUIPARAM current = nullptr );
    int check( const ts::array_wrapper_c<const check_item_s> & items, GUIPARAMHANDLER handler, ts::uint32 initial = 0, int tag = 0, bool visible = true, RID parent = RID() );
    RID label(const ts::wstr_c &text, bool visible = true, bool is_err = false);
    RID vspace(int height);
    RID panel(int height, GUIPARAMHANDLER drawhandler);
    RID textfield( const ts::wsptr &deftext, int chars_limit, tfrole_e role, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC(), const evt_data_s *addition = nullptr, int multiline = 0, RID parent = RID(), bool create_visible = true );
    RID list(int height, const ts::wstr_c & emptymessage);
    RID combik( const menu_c &m, RID parent = RID() );

    /*virtual*/ void children_repos_info(cri_s &info) const override;
    virtual void getbutton(bcreate_s &bcr) {};
    virtual int additions( ts::irect & b ) { b = ts::irect(0); return 0; };
    void reset(bool keep_skip = false); // kill all children and recreate default buttons (until keep_skip)

    virtual void tabselected(ts::uint32 mask) {}

    void ctlshow( ts::uint32 cmask, bool vis );
    void ctlenable( const ts::asptr&name, bool enblflg );
    void ctldesc( const ts::asptr&name, ts::wstr_c desc );

    void set_check_value( const ts::asptr&name, bool v );

    void set_selector_menu( const ts::asptr& ctl_name, const menu_c& m );
    void set_combik_menu( const ts::asptr& ctl_name, const menu_c& m );
    void set_label_text( const ts::asptr& ctl_name, const ts::wstr_c& t, bool full_height_last_line = false );
    void set_list_emptymessage( const ts::asptr& ctl_name, const ts::wstr_c& t );
    void set_slider_value( const ts::asptr& ctl_name, float val );
    void set_pb_pos( const ts::asptr& ctl_name, float val );
    void set_edit_value( const ts::asptr& ctl_name, const ts::wstr_c& t );

    virtual ts::UPDATE_RECTANGLE getrectupdate() { return DELEGATE(this, updrect_def); }

public:
    gui_dialog_c(initial_rect_data_s &data) :gui_vscrollgroup_c(data) { hcenter_small_ctls(true); }
    /*virtual*/ ~gui_dialog_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ ts::wstr_c get_name() const override { return captiontext; }
};
