#pragma once

#define TABSELMI(mask) gettabsel(), ts::amake((ts::uint32)mask)

bool TSCALL dialog_already_present( int );

class gui_listitem_c;
template<> struct MAKE_CHILD<gui_listitem_c> : public _PCHILD(gui_listitem_c)
{
    ts::wstr_c text;
    ts::str_c  param;
    ts::str_c  themerect;
    GETMENU_FUNC gm;
    const ts::drawable_bitmap_c *icon = nullptr;
    MAKE_CHILD(RID parent_, const ts::wstr_c &text, const ts::str_c &param) :text(text), param(param) { parent = parent_; }
    ~MAKE_CHILD();

    MAKE_CHILD &operator<<( const ts::drawable_bitmap_c *_icon ) { icon = _icon; return *this; }
    MAKE_CHILD &operator<<( GETMENU_FUNC _gm ) { gm = _gm; return *this; }
    MAKE_CHILD &threct( const ts::asptr&thr ) { themerect = thr; return *this; }
};

class gui_listitem_c : public gui_label_c
{
    friend struct MAKE_CHILD<gui_listitem_c>;
    DUMMY(gui_listitem_c);
    ts::str_c  param;
    GETMENU_FUNC gm; // get menu on rite click
    const ts::drawable_bitmap_c *icon = nullptr;
    int height = 0;

    GM_RECEIVER( gui_listitem_c, GM_POPUPMENU_DIED );
public:

    gui_listitem_c(MAKE_CHILD<gui_listitem_c> &data);
    /*virtual*/ ~gui_listitem_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ void set_text(const ts::wstr_c&text) override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    /*virtual*/ int get_height_by_width(int width) const override;

    const ts::str_c  &getparam() const {return param;}
};


class gui_dialog_c : public gui_vscrollgroup_c
{
    DUMMY(gui_dialog_c);

    GM_RECEIVER(gui_dialog_c, GM_DIALOG_PRESENT);
    GM_RECEIVER(gui_dialog_c, GM_CLOSE_DIALOG);

    int numtopbuttons = 0;
    int skipctls = 0;
    ts::irect border = ts::irect(0);
    ts::hashmap_t<ts::str_c, guirect_c::sptr_t> ctl_by_name;

    friend class gui_textfield_c;

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
        guirect_c::sptr_t ctlptr; // can be nullptr (if other tab selected, or parent control not yet created)

        int subctltag = -1;
        ts::str_c name;
        ts::uint32 mask = 0;
        ts::wstr_c desc;
        ts::wstr_c text;
        ts::wstr_c hint;
        menu_c items;
        gui_textedit_c::TEXTCHECKFUNC textchecker;
        GUIPARAMHANDLER handler = GUIPARAMHANDLER();
        ts::TSCOLOR color = 0;
        enum ctl_e
        {
            _HGROUP,
            _VSPACE,
            _STATIC,
            _STATIC_HIDDEN,
            _PATH,
            _FILE,
            _TEXT,
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
        bool focus_ = false;
        bool readonly_ = false;
        DEBUGCODE(bool initialization = true;) // protection flag - [this] can be changed while [initialization] == true due resize of array realloc.

        bool updvalue( const ts::wstr_c &t )
        {
            ASSERT( _TEXT == ctl || _PATH == ctl || _FILE == ctl );
            if (textchecker && textchecker(t))
            {
                text = t;
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

        description_s&sethint(const ts::wsptr&h) { hint=h; return *this; }
        description_s&setname(const ts::asptr&n) { name=n; return *this; }
        description_s&setmenu( const menu_c &m ) { items = m; return *this; }
        description_s&readonly(bool f = true) { readonly_ = f; return *this; }
        description_s&focus(bool f = true) { focus_ = f; return *this; }
        description_s&width(int w) { width_ = w; return *this; }
        description_s&height(int h) { height_ = h; return *this; }
        description_s&size(int w, int h) { width_ = w; height_ = h; return *this; }
        description_s&subctl(int tag,ts::wstr_c &ctldesc);


        void hgroup( const ts::wsptr& desc );
        description_s& label( const ts::wsptr& text );
        description_s& hiddenlabel( const ts::wsptr& text, ts::TSCOLOR col );
        void page_header( const ts::wsptr& text );
        description_s& vspace( int h = 5, GUIPARAMHANDLER oncreatehanler = nullptr );
        description_s& selector( const ts::wsptr &desc, const ts::wsptr &t, GUIPARAMHANDLER behaviourhandler = nullptr );
        description_s& path( const ts::wsptr &desc, const ts::wsptr &path, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC() );
        description_s& file( const ts::wsptr &desc, const ts::wsptr &iroot, const ts::wsptr &fn, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC() );
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
    void updrect(const void *, int r, const ts::ivec2 &p);
    void removerctl(int r);

    typedef ts::array_inplace_t<description_s, 0> descarray;
    descarray descs;
    void tabsel(const ts::str_c& par);
    MENUHANDLER gettabsel() { return DELEGATE(this, tabsel); };

    struct descmaker
    {
        descarray& arr;
        ts::uint32 mask = 0;
        descmaker(descarray& arr):arr(arr) { ASSERT(arr.size() == 0); }
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

    ts::wstr_c label_path_selector_caption;
    ts::wstr_c label_file_selector_caption;

    bool file_selector(RID, GUIPARAM prm);
    bool path_selector(RID, GUIPARAM prm);
    bool path_explore(RID, GUIPARAM prm);
    bool combo_drop(RID, GUIPARAM prm);
    bool custom_menu(RID, GUIPARAM prm);

    struct radio_item_s
    {
        ts::wstr_c text;
        GUIPARAM param;
        ts::str_c name;
        radio_item_s(const ts::wstr_c &text, GUIPARAM param):text(text), param(param) {}
        radio_item_s(const ts::wstr_c &text, GUIPARAM param, const ts::asptr& name):text(text), param(param), name(name) {}
    };
    struct check_item_s
    {
        ts::wstr_c text;
        ts::uint32 mask;
        ts::str_c name;
        check_item_s(const ts::wstr_c &text, ts::uint32 mask) :text(text), mask(mask) {}
        check_item_s(const ts::wstr_c &text, ts::uint32 mask, const ts::asptr& name) :text(text), mask(mask), name(name) {}
    };

    enum tfrole_e
    {
        TFR_TEXT_FILED,
        TFR_TEXT_FILED_RO,
        TFR_PATH_SELECTOR,
        TFR_PATH_VIEWER,
        TFR_FILE_SELECTOR,
        TFR_CUSTOM_SELECTOR,
        TFR_COMBO,
    };

    RID hgroup( const ts::wsptr& desc );
    int radio( const ts::array_wrapper_c<const radio_item_s> & items, GUIPARAMHANDLER handler, GUIPARAM current = nullptr );
    int check( const ts::array_wrapper_c<const check_item_s> & items, GUIPARAMHANDLER handler, ts::uint32 initial = 0, int tag = 0 );
    RID label(const ts::wstr_c &text, ts::TSCOLOR col = 0, bool visible = true);
    RID vspace(int height);
    RID textfield( const ts::wsptr &deftext, int chars_limit, tfrole_e role, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC(), const evt_data_s *addition = nullptr, int multiline = 0, RID parent = RID() );
    RID list(int height, const ts::wstr_c & emptymessage);
    RID combik( const menu_c &m, RID parent = RID() );

    /*virtual*/ void children_repos_info(cri_s &info) const override;
    virtual void getbutton(bcreate_s &bcr) {};
    virtual int additions( ts::irect & b ) { b = ts::irect(0); return 0; };
    void reset(bool keep_skip = false); // kill all children and recreate default buttons (until keep_skip)

    virtual void tabselected(ts::uint32 mask) {}

    void ctlenable( const ts::asptr&name, bool enblflg );

    void set_selector_menu( const ts::asptr& ctl_name, const menu_c& m );
    void set_combik_menu( const ts::asptr& ctl_name, const menu_c& m );
    void set_label_text( const ts::asptr& ctl_name, const ts::wstr_c& t );
    void set_list_emptymessage( const ts::asptr& ctl_name, const ts::wstr_c& t );
    void set_slider_value( const ts::asptr& ctl_name, float val );
    void set_pb_pos( const ts::asptr& ctl_name, float val );

    ts::UPDATE_RECTANGLE getrectupdate() { return DELEGATE(this, updrect); }

public:
    gui_dialog_c(initial_rect_data_s &data) :gui_vscrollgroup_c(data) {}
    /*virtual*/ ~gui_dialog_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

};
