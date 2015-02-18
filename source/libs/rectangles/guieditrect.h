/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

class application_c;
extern application_c *g_app;

#define SEPARATORS L" \n\t:;\"',./?`~!@#$%^&*()-+=\\|{}"
#define IS_WORDB(c) (c==0||ts::CHARz_find(SEPARATORS,c)>=0)

class gui_textedit_c : public gui_control_c
{
    DUMMY( gui_textedit_c );

    GM_RECEIVER( gui_textedit_c, GM_HEARTBEAT );

    void ctx_menu_cut(const ts::str_c &);
    void ctx_menu_paste(const ts::str_c &);
    void ctx_menu_copy(const ts::str_c &);
    void ctx_menu_del(const ts::str_c &);
    void ctx_menu_selall(const ts::str_c &);
    bool ctxclosehandler(RID, GUIPARAM);

	ts::drawable_bitmap_c texture;
    sbhelper_s sbhelper;
	ts::TSCOLOR caret_color         = ts::ARGB(0,0,0),
                color               = ts::ARGB(0,0,0),
                selection_color     = ts::ARGB(255,255,0),
                selection_bg_color  = ts::ARGB(100,100,255),
                outline_color       = 0; //ts::ARGB(0,0,0);
	const ts::font_desc_c *font = &ts::g_default_text_font;
	ts::wchar password_char = 0;
	struct active_element_s
	{
		int advance, user_data_size;
		ts::wstr_c str;
		ts::TSCOLOR color;
		void update_advance(ts::font_c *font);

        static active_element_s * fromchar( ts::wchar ch ) // хинт. создается указатель с установленным младшим битом. таких указателей не бывает (по крайней мере в винде), поэтому в старших битах сам символ
        {
            return (active_element_s*)(((UINT_PTR)ch << 16)|1);
        }
        static active_element_s * create( ts::aint addition_space )
        {
            active_element_s *el = (active_element_s*)MM_ALLOC(sizeof(active_element_s) + addition_space);
            TSPLACENEW(el);
            return el;
        }
        bool is_char() const
        {
            return ((UINT_PTR)this & 1); // хинт! младший бит валидного указателя всегда 0
        }
        ts::wchar as_char() const
        {
            return ((UINT_PTR)this >> 16);
        }

        void die()
        {
            if (ASSERT(!is_char() && this))
            {
                this->~active_element_s();
                MM_FREE(this);
            }
        }
    private:
        ~active_element_s() {}
	};
	class text_element_c
	{
		text_element_c(const text_element_c &) UNUSED;
		void operator=(const text_element_c &) UNUSED;

	public:
		active_element_s *p;

		text_element_c(active_element_s *p) : p(p) {}//absorb constructor
		text_element_c(ts::wchar ch) {p = active_element_s::fromchar(ch);}
		~text_element_c() {if (!p->is_char()) p->die();}

        void operator=(active_element_s *_p)
        {
            if (!p->is_char()) p->die();
            p = _p;
        }

		bool operator==(ts::wchar ch) const {return p->is_char() && p->as_char() == ch;}

		ts::wchar get_char_fast() const {return p->as_char();}
		bool     is_char() const {return p->is_char();}
		ts::wchar get_char() const {return p->is_char() ? p->as_char() : 0;}
		int advance(ts::font_c *font) const {return p->is_char() ? (*font)[p->as_char()].advance : p->advance;}
		void update_advance(ts::font_c *font) {if (!p->is_char()) p->update_advance(font);}
		int meta_text_size() const {return p->is_char() ? 1 : (3 + 8) + p->str.get_length() + p->user_data_size*2;}
	};
	ts::array_inplace_t<text_element_c,512> text; // text
	ts::tbuf_t<ts::ivec2> lines; // lines of text
	int start_sel, caret_line, caret_offset, scroll_left;
	float wheel_scroll_step_size = 50.0f;
	int caret_width = 2, baseline_offset = 0, chars_limit = 0;
	int margin_left = 0, margin_right = 0, margin_top = 0, margin_bottom = 0;

    bool focus() const;
    bool selection_disallowed() const;

    bool show_caret() const
    {
        return flags.is(F_CARET_SHOW) && is_enabled() && focus();
    }

    ts::ivec2 size() const
    {
        return get_client_area().size();
    }

    bool summoncontextmenu();
    void redraw(bool redtraw_texture = true);
    void prepare_texture();
    void run_heartbeat();
    bool invert_caret(RID, GUIPARAM);

    bool kbd_processing_(system_query_e qp, ts::wchar charcode, int scan);

	//void onChangeScrollPos(float f) {scrollTo((int)f);}

	int text_el_advance(int index) const { if (index >= text.size()) return 0; return !password_char ? text[index].advance((*font)) : (*(*font))[password_char].advance;}
	active_element_s *create_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data, int user_data_size);
	bool text_replace(int pos, int num, const ts::wsptr &str, active_element_s **el, int len, bool updateCaretPos = true);
	bool text_replace(int pos, int num, const ts::wsptr &str, bool updateCaretPos = true);
	bool text_replace(int cp, const ts::wsptr &str) {return start_sel == -1 ? text_replace(cp, 0, str) : text_replace(ts::tmin(cp, start_sel), ts::tabs(start_sel-cp), str);}
	bool text_erase(int pos, int num) {return text_replace(pos, num, ts::wstr_c(), nullptr, 0);}
	bool text_erase(int cp) {return text_erase(ts::tmin(cp, start_sel), ts::tabs(start_sel-cp));}
	ts::wstr_c text_substr(int start, int count) const;
	void scroll_to_caret();
	bool cut_(int cp, bool copy2clipboard = true);//used internally
	bool copy_(int cp);//used internally
	void paste_(int cp);//used internally
	bool prepare_lines(int startChar = 0);//формирует из text набор строк в lines

    NUMGEN_START(fff, 0);
#define DECLAREBIT( fn ) static const ts::flags32_s::BITS fn = FLAGS_FREEBITSTART << NUMGEN_NEXT(fff)
    DECLAREBIT( F_READONLY );
    DECLAREBIT( F_TEXTUREDIRTY );
    DECLAREBIT( F_LINESDIRTY );
    DECLAREBIT( F_NOFOCUS );
    DECLAREBIT( F_TRANSPARENT_ME ); // прозрачность для mouse events везде внутри окна, кроме active elements
    DECLAREBIT( F_MULTILINE );
    DECLAREBIT( F_HEARTBEAT );
    DECLAREBIT( F_CARET_SHOW ); // blinking flag
    DECLAREBIT( F_HANDCURSOR );
    DECLAREBIT( F_ARROW_CURSOR );
    DECLAREBIT( F_DISABLE_CARET ); // no caret - no selections
    DECLAREBIT( F_SBALWAYS );
    DECLAREBIT( F_IGNOREFOCUSCHANGE );

protected:
    DECLAREBIT( F_TEXTEDIT_FREBITSTART ); // free bit start for child
    FREE_BIT_START_CHECK( FLAGS_FREEBITSTART, NUMGEN_NEXT(fff) - 1 );
#undef DECLAREBIT
    gui_textedit_c() {}
public:

	gui_textedit_c(initial_rect_data_s &data);
	~gui_textedit_c();

    void set_font(const ts::font_desc_c *f)
    {
        font = &safe_font(f);
    }

	int meta_text_length_limit;
    typedef fastdelegate::FastDelegate<bool (const ts::wstr_c &)> TEXTCHECKFUNC;
	TEXTCHECKFUNC check_text_func;//пользовательская функция проверка текста
	GUIPARAMHANDLER on_enter_press; // пользовательский обработчик нажатия Enter
	GUIPARAMHANDLER on_escape_press; // пользовательский обработчик нажатия Escape
    GUIPARAMHANDLER on_lbclick; // only if F_DISABLE_CARET

	bool is_multiline() const {return flags.is(F_MULTILINE);}
    bool is_vsb() const
    {
        if (is_multiline())
        {
            if (flags.is(F_SBALWAYS)) return true;
            return sbhelper.datasize > size().y;
        }

        return false;
    }
    void set_multiline(bool ml = true) { flags.init(F_MULTILINE, ml); }
	bool is_readonly() const {return flags.is(F_READONLY|F_DISABLED);}
    bool is_disabled_caret() const { return flags.is(F_DISABLE_CARET);}
	void set_readonly(bool ro = true) {flags.init(F_READONLY, ro);}
    void disable_caret(bool dc = true) {flags.init(F_DISABLE_CARET, dc);}
    void arrow_cursor(bool f = true) {flags.init(F_ARROW_CURSOR, f);}
	bool is_empty() const {return text.size() == 0;}
    void set_chars_limit( int cl ) { chars_limit = cl; }
    void set_margins( int left = 0, int rite = 0, int top = 0 ) { margin_left = left; margin_right = rite; margin_top = top; }

	void set_text(const ts::wstr_c &text, bool setCaretToEnd = false);
	ts::wstr_c get_text() const {return text_substr(0, text.size());}
	void insert_text(const ts::wstr_c &text) {text_replace(get_caret_char_index(), text);}//вставляет текст в текущей позиции курсора
	void set_color(ts::TSCOLOR c) { caret_color = color = c; redraw(); }
	void set_password_char(ts::wchar pc) { password_char = pc; }

	void append_meta_text(const ts::wstr_c &meta_text);
	ts::wstr_c get_meta_text() const;
	static ts::wstr_c make_meta_text_from_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data = nullptr, int user_data_size = 0);
	static ts::wstr_c make_meta_text_color_tag(ts::TSCOLOR color) {return make_meta_text_from_active_element(ts::wstr_c(), color);}

	int lines_count() const;
	void remove_lines(int n);
	void remove_lines_over(int linesLimit) {int r = lines_count() - linesLimit; if (r > 0) remove_lines(r);}

	void reset_sel() {start_sel = -1; redraw();}
    int sel_size() const { return start_sel < 0 ? 0 : ts::tabs(start_sel-get_caret_char_index()); }

	int  scroll_top() const {return -sbhelper.shift;}
	void scroll_to(int y) {sbhelper.shift = -y; redraw();}

	//Функции работы с курсором
	ts::ivec2 get_caret_pos() const;
	void set_caret_pos(ts::ivec2 p);//задаёт положение курсора по координатам клика мыши
	void set_caret_pos(int cp);//задаёт положение курсора по смещению символа в тексте
	ts::ivec2 get_char_pos(int pos) const;//возвращает местонахождение (смещение и строку) символа
	int get_caret_char_index() const { return lines.get(caret_line).x+caret_offset; }

	//Функции работы с буфером обмена
    void end()
    {
        caret_offset = lines.last().delta();
        caret_line = lines.count() - 1;
        scroll_to_caret();
    }

    void backspace() { text_erase(get_caret_char_index()-1, 1); }
    void del() {cut_(get_caret_char_index(),false);}
	void cut() {cut_(get_caret_char_index());}
	void copy() {copy_(get_caret_char_index());}
	void paste() {paste_(get_caret_char_index());}
    void selectword();
    void selectall()
    {
        if (flags.is(F_DISABLE_CARET)) return;
        start_sel = 0;
        set_caret_pos(text.size());
        redraw();
    }

	fastdelegate::FastDelegate<bool (system_query_e, const ts::wstr_c &, const void *, int)> active_element_mouse_event_func;
	active_element_s *under_mouse_active_element;
	ts::ivec2 under_mouse_active_element_pos;
	void insert_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data, int user_data_size, int cp);
	void insert_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data, int user_data_size) {insert_active_element(str, color, user_data, user_data_size, get_caret_char_index());}

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    const ts::font_desc_c &get_font() const { ASSERT(font); return *font; }
};
