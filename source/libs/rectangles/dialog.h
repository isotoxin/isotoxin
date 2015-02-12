#pragma once

#define TABSELMI(mask) gettabsel(), ts::amake((ts::uint32)mask)

bool TSCALL dialog_already_present( int );

typedef fastdelegate::FastDelegate< menu_c (const ts::str_c &) > GETMENU_FUNC;
class gui_listitem_c;
template<> struct MAKE_CHILD<gui_listitem_c> : public _PCHILD(gui_listitem_c)
{
    ts::wstr_c text;
    ts::str_c  param;
    GETMENU_FUNC gm;
    MAKE_CHILD(RID parent_, const ts::wstr_c &text, const ts::str_c &param) :text(text), param(param) { parent = parent_; }
    ~MAKE_CHILD();

    MAKE_CHILD &operator<<( GETMENU_FUNC _gm ) { gm = _gm; return *this; }
};

class gui_listitem_c : public gui_label_c
{
    friend struct MAKE_CHILD<gui_listitem_c>;
    DUMMY(gui_listitem_c);
    ts::str_c  param;
    GETMENU_FUNC gm; // get menu on rite click

    GM_RECEIVER( gui_listitem_c, GM_POPUPMENU_DIED );
public:

    gui_listitem_c(MAKE_CHILD<gui_listitem_c> &data) :gui_label_c(data), param(data.param) { set_theme_rect(CONSTASTR("lstitem"), false); }
    /*virtual*/ ~gui_listitem_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

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

protected:

    virtual int unique_tag() { return 0; }
    virtual void on_close() { TSDEL(this); }
    virtual void on_confirm() { TSDEL(this); }

    RID find( const ts::asptr &name ) const;
    void setctlname( const ts::asptr &name, guirect_c &r );

    struct description_s
    {
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
            _TEXT,
            _COMBIK,
            _LIST,
            _CHECKBUTTONS,
            _RADIO,
            _BUTTON,
        };
        ctl_e ctl;
        int width_ = 0;
        int height_ = 0;
        bool focus_ = false;
        bool readonly_ = false;

        bool updvalue( const ts::wstr_c &t )
        {
            ASSERT( _TEXT == ctl || _PATH == ctl );
            if (textchecker && textchecker(t))
            {
                text = t;
                return true;
            }
            return false;
        }
        bool updvalue2( RID r, GUIPARAM p )
        {
            switch (ctl)
            {
            case gui_dialog_c::description_s::_RADIO:
                if (handler) handler(r, p);
                text.set_as_int((int)p);
                break;
            case gui_dialog_c::description_s::_CHECKBUTTONS:
                if (handler) handler(r, p);
                text.set_as_uint((ts::uint32)p);
                break;
            }

            return true;
        }

        GET_TOOLTIP gethintproc() const
        {
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
        void label( const ts::wsptr& text );
        description_s& hiddenlabel( const ts::wsptr& text, ts::TSCOLOR col );
        void page_header( const ts::wsptr& text );
        description_s& vspace( int h = 5, GUIPARAMHANDLER oncreatehanler = nullptr );
        description_s& path( const ts::wsptr &desc, const ts::wsptr &path, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC() );
        description_s& textfield( const ts::wsptr &desc, const ts::wsptr &val, gui_textedit_c::TEXTCHECKFUNC checker);
        description_s& textfieldml( const ts::wsptr &desc, const ts::wsptr &val, gui_textedit_c::TEXTCHECKFUNC checker, int lines = 3); // multiline
        description_s& combik( const ts::wsptr &desc);
        description_s& list( const ts::wsptr &desc, int lines );
        description_s& checkb( const ts::wsptr &desc, GUIPARAMHANDLER handler, ts::uint32 initial );
        description_s& radio( const ts::wsptr &desc, GUIPARAMHANDLER handler, int initial );
        description_s& button(const ts::wsptr &desc, const ts::wsptr &text, GUIPARAMHANDLER handler);

        RID make_ctl(gui_dialog_c *dlg, RID parent);
    };

    ts::hashmap_t<int, ts::safe_ptr<guirect_c>> subctls;
    void updrect(void *, int r, const ts::ivec2 &p);

    typedef ts::array_inplace_t<description_s, 0> descarray;
    descarray descs;
    void tabsel(const ts::str_c& par);
    MENUHANDLER gettabsel() { return DELEGATE(this, tabsel); };

    struct descmaker
    {
        descarray& arr;
        ts::uint32 mask = 0;
        descmaker(descarray& arr):arr(arr) {}
        description_s& operator()() { description_s& d = arr.add(); d.mask = mask; return d; }
        void operator << (ts::uint32 m) {mask = m;}
        void operator=(const descmaker&) UNUSED;
    };

    ts::wstr_c label_path_selector_caption;

    bool path_selector(RID, GUIPARAM prm);
    bool path_explore(RID, GUIPARAM prm);
    bool combo_drop(RID, GUIPARAM prm);

    //ts::hashmap_t<ts::pair_s<int,int>, guirect_c::sptr_t> ctls;
    //template<typename R> R * getctl( int tag, int subtag = 0 )
    //{
    //    guirect_c::sptr_t * ptr = ctls.get( ts::pair_s<int, int>(tag,subtag) );
    //    if (ptr && !ptr->expired()) return ts::ptr_cast<R *>(ptr->get());
    //    return nullptr;
    //}

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
        TFR_COMBO,
    };

    RID hgroup( const ts::wsptr& desc );
    int radio( const ts::array_wrapper_c<const radio_item_s> & items, GUIPARAMHANDLER handler, GUIPARAM current = nullptr );
    int check( const ts::array_wrapper_c<const check_item_s> & items, GUIPARAMHANDLER handler, ts::uint32 initial = 0 );
    RID label(const ts::wstr_c &text, ts::TSCOLOR col = 0, bool visible = true);
    RID vspace(int height);
    RID textfield( const ts::wsptr &deftext, int chars_limit, tfrole_e role, gui_textedit_c::TEXTCHECKFUNC checker = gui_textedit_c::TEXTCHECKFUNC(), const evt_data_s *addition = nullptr, int multiline = 0, RID parent = RID() );
    RID list(int height);
    RID combik( const menu_c &m, RID parent = RID() );

    /*virtual*/ void children_repos_info(cri_s &info) const override;
    virtual void getbutton(bcreate_s &bcr) {};
    virtual int additions( ts::irect & border ) { border = ts::irect(0); return 0; };
    void reset(bool keep_skip = false); // kill all children and recreate default buttons (until keep_skip)

    virtual void tabselected(ts::uint32 mask) {}

    void ctlenable( const ts::asptr&name, bool enblflg );

    void set_combik_menu( const ts::asptr& ctl_name, const menu_c& m );

public:
    gui_dialog_c(initial_rect_data_s &data) :gui_vscrollgroup_c(data) {}
    /*virtual*/ ~gui_dialog_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

};
