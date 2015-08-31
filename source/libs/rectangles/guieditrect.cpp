#include "rectangles.h"

gui_textedit_c::gui_textedit_c(initial_rect_data_s &data) : gui_control_c(data), start_sel(-1), caret_line(0), caret_offset(0), scroll_left(0), under_mouse_active_element(nullptr)
{
    flags.set(F_CARET_SHOW);
	lines.add(ts::ivec2(0,0));
}

gui_textedit_c::~gui_textedit_c()
{
    if (gui) gui->delete_event( DELEGATE(this, invert_caret) );
}

bool gui_textedit_c::focus() const
{
    return gui->get_active_focus() == getrid() || gui->get_focus() == getrid();
}

void gui_textedit_c::redraw(bool redraw_texture)
{
    if (redraw_texture)
        flags.set(F_TEXTUREDIRTY);
    getengine().redraw();
}

bool gui_textedit_c::invert_caret(RID, GUIPARAM)
{
    gui->delete_event(DELEGATE(this, invert_caret));
    flags.invert(F_CARET_SHOW);
    if (caretrect) getengine().redraw(&caretrect);
    flags.clear(F_HEARTBEAT);
    if (gui->get_focus() != getrid() && focus())
        gui->set_focus(getrid()); // get focus to papa
    return true;
}


void gui_textedit_c::run_heartbeat()
{
    if ( focus() && !flags.is(F_HEARTBEAT) )
    {
        DEFERRED_UNIQUE_CALL(0.3, DELEGATE(this, invert_caret), nullptr);
        flags.set(F_HEARTBEAT);
    } else {
        flags.clear(F_HEARTBEAT);
    }
}

bool gui_textedit_c::text_replace(int pos, int num, const ts::wsptr &str, active_element_s **el, int len, bool updateCaretPos)
{
	if (check_text_func)
    {
        int pos0 = pos, pos1 = pos + num;
        ts::wstr_c t = get_text_and_fix_pos(&pos0, &pos1);
        t.replace(pos0, pos1 - pos0, str);
        if (!check_text_func(t)) return false;
    }
	if (!is_multiline()) // singleline editbox
	{
        int editwidth = size().x;

        int pos0 = pos, pos1 = pos + num;
        ts::wstr_c ttext = get_text_and_fix_pos(&pos0, &pos1);
        ttext.replace(pos0, pos1 - pos0, str);


		if (ttext.find_pos(L'\n') != -1) return false; // no caret return allowed

		if (chars_limit > 0) if (ttext.get_length() <= chars_limit) goto ok; else return false;

		int w = editwidth-ts::ui_scale(margin_left)-ts::ui_scale(margin_right)-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0);
		if (!password_char)
			for (ts::wchar c : ttext) w -= (*(*font))[c].advance;
		else
			w -= ttext.get_length() * (*(*font))[password_char].advance;
		if (w < 0) return false; // text width too large
	}
	else
		if (chars_limit > 0 && (text.size() - num + str.l) > chars_limit) return false;

ok:
	text.replace(pos, num, el, len);

	if (prepare_lines())
    {
	    redraw();
	    if (updateCaretPos) set_caret_pos(pos + len);
    }
	start_sel = -1;
	return true;
}

bool gui_textedit_c::text_replace(int pos, int num, const ts::wsptr &str, bool updateCaretPos)
{
	ts::tmp_pointers_t<active_element_s, 0> elements( str.l );
    if (is_multiline())
    {
        for(int i=0;i<str.l;++i)
            elements.add(active_element_s::fromchar(str.s[i]));
	    return text_replace(pos, num, str, elements.begin(), elements.size(), updateCaretPos);
    } else
    {
        bool nldetected = false;
        for (int i = 0; i < str.l; ++i)
            if (str.s[i] != '\n')
                elements.add(active_element_s::fromchar(str.s[i]));
            else
                nldetected = true;
        if (nldetected)
        {
            ts::wstr_c x;
            ts::pwstr_c(str).extract_non_chars( x, CONSTWSTR("\n") );
            return text_replace(pos, num, x.as_sptr(), elements.begin(), elements.size(), updateCaretPos);  
        } else
            return text_replace(pos, num, str, elements.begin(), elements.size(), updateCaretPos);  
    }
}

ts::wstr_c gui_textedit_c::get_text_and_fix_pos(int *pos0, int *pos1) const
{
    int count = text.size();
    ts::wstr_c r(count, true);

    bool pos0fixed = pos0 == nullptr;
    bool pos1fixed = pos1 == nullptr;

    for (int i = 0; i < count; ++i)
    {
        if (!pos0fixed && *pos0 == i)
            *pos0 = r.get_length(), pos0fixed = true;

        if (!pos1fixed && *pos1 == i)
            *pos1 = r.get_length(), pos1fixed = true;

        const text_element_c &te = text[i];
        if (te.is_char()) r.append_char(te.get_char_fast()); else r.append(te.p->to_wstr());
    }

    if (!pos0fixed && *pos0 == count)
        *pos0 = r.get_length();

    if (!pos1fixed && *pos1 == count)
        *pos1 = r.get_length();

    return r;
}

ts::wstr_c gui_textedit_c::text_substr(int start, int count) const
{
    ts::wstr_c r(count, true);
    for (int i = 0; i < count; ++i)
    {
        const text_element_c &te = text[start + i];
        if (te.is_char()) r.append_char(te.get_char_fast()); else r.append(te.p->to_wstr());
    }
    return r;
}

ts::str_c gui_textedit_c::text_substr_utf8(int start, int count) const
{
	ts::str_c r(count, true);
	for (int i=0; i<count; ++i)
	{
		const text_element_c &te = text[start+i];
		if (te.is_char()) r.append_unicode_as_utf8( te.get_char_fast() ); else r.append(te.p->to_utf8());
	}
	return r;
}

bool gui_textedit_c::cut_(int cp, bool copy2clipboard)
{
	if (start_sel!=-1 && !is_readonly())
	{
		int len=ts::tabs(start_sel-cp);
		cp=ts::tmin(cp,start_sel);
		if (copy2clipboard) ts::set_clipboard_text(text_substr(cp, len));
		text_erase(cp, len);
        return true;
	}
    return false;
}

bool gui_textedit_c::copy_(int cp)
{
	if (start_sel!=-1)
    {
        ts::set_clipboard_text(text_substr(ts::tmin(cp,start_sel),abs(start_sel-cp)));
        return true;
    }
	return false;
}

void gui_textedit_c::paste_(int cp)
{
	if (is_readonly()) return;

	text_replace(cp, ts::get_clipboard_text());
}

void gui_textedit_c::insert_active_element(active_element_s *ae, int cp)
{
	if (!text_replace(cp, 0, ae->to_wstr(), &ae, 1)) ae->release();
}

ts::ivec2 gui_textedit_c::get_caret_pos() const
{
	int i=0, x=0, s=lines.get(caret_line).r0;
	for (;i<caret_offset;i++) x+=text_el_advance(s+i);
	return ts::ivec2(x + ts::ui_scale(margin_left) - scroll_left, caret_line * (*font)->height + ts::ui_scale(margin_top) - scroll_top());
}

void gui_textedit_c::scroll_to_caret()
{
    ts::ivec2 sz = size();
    bool dirty_texture = false;
	int newST = ts::tmax((caret_line+1) * (*font)->height - sz.y + ts::ui_scale(margin_top) + ts::ui_scale(margin_bottom), ts::tmin(scroll_top(), caret_line * (*font)->height));
	if (newST != scroll_top())
    {
        sbhelper.shift = -newST;
        dirty_texture = true;
    }
	if (chars_limit && !is_multiline())
	{
        int editwidth = sz.x - caret_width;
		ASSERT(caret_line == 0 && lines.count() == 1 && lines.get(0).r0 == 0);
		int i = 0, caretX = 0, lineW = 0;
		for (; i<caret_offset; i++) caretX += text_el_advance(i);
		for (lineW = caretX; i<lines.get(0).r1; i++) lineW += text_el_advance(i);
		scroll_left = ts::tmax(caretX /*+ ts::ui_scale(caretWidth)*/ - editwidth + ts::ui_scale(margin_left) + ts::ui_scale(margin_right), ts::tmin(scroll_left, caretX));
		scroll_left = ts::tmax(0, ts::tmin(scroll_left, lineW + ts::ui_scale(margin_left) + ts::ui_scale(margin_right) - editwidth));
	}
	flags.set(F_CARET_SHOW);
    redraw(dirty_texture);
}

void gui_textedit_c::set_caret_pos(ts::ivec2 p)
{
    if (!font) return;
	p.x += scroll_left - ts::ui_scale(margin_left);
	int line=ts::tmax(0,ts::tmin((p.y-ts::ui_scale(margin_top)+scroll_top())/(*font)->height,lines.count()-1));
	int lineW=0;
	int i=0;
	for (;i<lines.get(line).delta();i++)
	{
		int charW=text_el_advance(lines.get(line).r0+i);

		if (p.x<=lineW+charW/2) break;

		lineW+=charW;
	}
	caret_offset=i;
	caret_line=line;
	scroll_to_caret();
}

ts::ivec2 gui_textedit_c::get_char_pos(int pos) const
{
	int i;
	for (i=0;i<lines.count()-1;i++)
		if (pos<=lines.get(i).r1) break;

	return ts::ivec2(pos-lines.get(i).r0, i);
}

void gui_textedit_c::set_caret_pos(int cp)
{
	ts::ivec2 p = get_char_pos(cp);
	caret_offset=p.x;
	caret_line=p.y;
	scroll_to_caret();
}

bool gui_textedit_c::kbd_processing_(system_query_e qp, ts::wchar charcode, int scan)
{
	bool res = false;
    int cp = 0;
	if (qp == SQ_KEYDOWN)
	{
		cp=get_caret_char_index();
		bool def=false,shiftPressed=GetKeyState(VK_SHIFT)<0;

		if (!shiftPressed)
		{
			if (start_sel==cp) start_sel=-1; // if zero-len selection - avoid handle of left/rite key pressed
		}
		else if (start_sel==-1) start_sel=cp;

		if (GetKeyState(VK_CONTROL)<0)
		{
            bool do_default = true;
            for (const kbd_press_callback_s &cb : kbdhandlers)
            {
                if (-cb.scancode == scan)
                {
                    ts::safe_ptr<rectengine_c> e(&getengine());
                    if (cb.handler(getrid(), this))
                        do_default = false;
                    if (!e) return true;
                    res = true;
                    break;
                }
            }


            if (!do_default || flags.is(F_DISABLE_CARET)) return false;
			def=true;
			switch (scan)
			{
            case SSK_RSHIFT:
			case SSK_LSHIFT:
				if (start_sel==-1) start_sel=cp;
				res = true;
				break;
			case SSK_A:
				selectall();
				res = true;
				break;
            case SSK_W:
                selectword();
                res = true;
                break;
			case SSK_HOME:
				caret_offset = 0;
				caret_line = 0;
				scroll_to_caret();
				res = true;
				def = false;
				break;
			case SSK_END:
                end();
				res = true;
				def = false;
				break;
			case SSK_C:
			case SSK_INSERT:
				res = copy_(cp);;
				break;
			case SSK_V:
				paste_(cp);
				res = true;
				break;
			case SSK_X:
				res = cut_(cp);
				break;
			case SSK_LEFT:
				for (;cp>0 &&  IS_WORDB(text.get(cp-1).get_char());cp--);
				for (;cp>0 && !IS_WORDB(text.get(cp-1).get_char());cp--);
				set_caret_pos(cp);
				res = true;
				break;
			case SSK_RIGHT:
				for (;cp<text.size() && !IS_WORDB(text.get(cp).get_char());cp++);
				for (;cp<text.size() &&  IS_WORDB(text.get(cp).get_char());cp++);
				set_caret_pos(cp);
				res = true;
				break;
            case SSK_ENTER:
                cp=get_caret_char_index();
                goto do_enter_key;
			}
		}
		else
		{
            if (flags.is(F_DISABLE_CARET)) return false;
			if (!shiftPressed && start_sel!=-1 && (scan==SSK_LEFT || scan==SSK_RIGHT)) // special case: Shift not pressed but selection present and left or right key pressed - caret must be moved to start or end of selection before selection be reset
			{
				if (scan==SSK_LEFT) set_caret_pos(min(cp,start_sel));
				else set_caret_pos(max(cp,start_sel));
				res = true;
			}
			else
            {
                bool do_default = true;
                for(const kbd_press_callback_s &cb : kbdhandlers)
                {
                    if (cb.scancode == scan)
                    {
                        ts::safe_ptr<rectengine_c> e( &getengine() );
                        if (cb.handler( getrid(), this ))
                            do_default = false;
                        if (!e) return true; // engine was killed by handler. just exit
                        res = true;
                        break;
                    }
                }

                if (do_default)
			    switch (scan)
			    {
			    case SSK_SHIFT:
				    if (start_sel==-1) start_sel=cp;
				    res = true;
				    break;
			    case SSK_LEFT:
				    if (caret_offset>0) caret_offset--;
				    else if (caret_line>0) caret_offset=lines.get(--caret_line).delta();
				    scroll_to_caret();
				    res = true;
				    break;
			    case SSK_RIGHT:
				    if (caret_offset<lines.get(caret_line).delta()) caret_offset++;
				    else if (caret_line<lines.count()-1) caret_offset=0,caret_line++;
				    scroll_to_caret();
				    res = true;
				    break;
			    case SSK_UP:
			    case SSK_DOWN:
				    set_caret_pos(get_caret_pos()+ts::ivec2(0,(*font)->height*(scan==SSK_UP ? -1 : 1)));
				    res = true;
				    break;
			    case SSK_HOME:
				    caret_offset=0;
				    scroll_to_caret();
				    res = true;
				    break;
			    case SSK_END:
				    caret_offset=lines.get(caret_line).delta();
				    scroll_to_caret();
				    res = true;
				    break;
			    case SSK_INSERT:
				    if (is_readonly()) def=true; else
				    if (shiftPressed) paste_(cp); else def=true;
				    res = true;
				    break;
			    case SSK_DELETE:
				    if (is_readonly()) def=true; else
				    if (shiftPressed)
				    {
					    cut_(cp);
				    }
				    else
				    {
					    if (start_sel!=-1) text_erase(cp);
					    else if (text.size() > cp) text_erase(cp, 1);
				    }
				    res = true;
				    break;
			    default:
				    def=true; // do not clear selection
				    break;
			    }
            }
		}

		if (!def && start_sel!=-1 && !shiftPressed) start_sel=-1; // clear selection
	}
	else if (qp == SQ_CHAR)
	{
		if (is_readonly()) return false;

		cp=get_caret_char_index();

		switch (charcode)
		{
		case VK_TAB:
			if (GetKeyState(VK_SHIFT) < 0)
			{
                // TODO : jump to previous control
			} else
			{
                 // TODO : jump to next control
			}
			return false;
		case VK_ESCAPE:
			res = true;
			break;
		case VK_BACK:
			if (GetKeyState(VK_SHIFT)>=0)
			{
				if (start_sel!=-1)
				{
					text_erase(cp);
				}
				else if (cp > 0)
				{
					text_erase(cp-1, 1);
				}
			}
			res = true;
			break;
		case VK_RETURN:
            do_enter_key:
			//if (on_enter_press && on_enter_press(getrid(), this))
   //         {
   //             gui->set_focus(RID());
   //             return true;
   //         }
			charcode='\n';
		default:
            {
                ts::wchar c = charcode;
                text_replace(cp, ts::wsptr(&c,1));
            }
			
			res = true;
			break;
		}
	}

	if (res) redraw();
	return res;
}

bool gui_textedit_c::prepare_lines(int startchar)
{
    flags.set(F_LINESDIRTY|F_TEXTUREDIRTY);
    ts::ivec2 sz = size();
    if (sz.x <= 0) return false; // not yet resized
	int linew=0,linestart=startchar;
	if (startchar == 0) lines.clear();
    int i=linestart;
	if (!is_multiline()) i=text.size(); else
	for (;i<text.size();i++)
	{
		if (text[i]==L'\n') // build new line
		{
			lines.add(ts::ivec2(linestart,i));
			linestart=i+1;
			linew=0;
			continue;
		}

		linew+=text_el_advance(i);
		if (linew>sz.x-ts::ui_scale(margin_left)-ts::ui_scale(margin_right)-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0) && /*обязательно проверяем - вдруг это первый символ, т.к. ситуация когда символ не помещается во всей строке не может быть корректно обработана*/i>linestart)
		{
            //Сразу формировать новую строку нельзя - если окажется, что это единственное слово в строке, то его надо не переносить, а разбивать
            //Поиск последнего разделителя (только пробел, табуляция обрабатывается некорректно!)
            //Если пробел стоит в начале строки, а сама строка состоит из одного слова, то слово всё равно переносится на другую строку - это нормально
            int j=i;
			for (;j>=linestart && !(text[j]==L' ');j--);

			if (j>=linestart)//Пробел был найден до начала строки - слово не единственное => формируем новую строку
			{
				lines.add(ts::ivec2(linestart,j));
				linestart=j+1;
				linew=0;
				for (++j;j<=i;j++) linew+=text_el_advance(j);
			}
			else //Также формируем новую строку, но уже не по последнему пробелу, а по части слова, вмещающегося в строку
			{
				lines.add(ts::ivec2(linestart,i));
				linew=text_el_advance(i);
				linestart=i;
			}
		}
	}
	lines.add(ts::ivec2(linestart,i));

	under_mouse_active_element = nullptr;
    flags.clear(F_LINESDIRTY);

    sbhelper.set_size( lines.count() * (*font)->height+ ts::ui_scale(margin_top) + ts::ui_scale(margin_bottom), sz.y );

    if (caret_line >= lines.count()) caret_line = lines.count() - 1;

    return true;
}

void gui_textedit_c::set_placeholder(const ts::wstr_c &text)
{
    if (text != placeholder)
    {
        flags.set(F_TEXTUREDIRTY);
        placeholder = text;
        redraw();
    }
}

void gui_textedit_c::set_text(const ts::wstr_c &text_, bool setCaretToEnd)
{
    flags.init(F_PREV_SB_VIS, is_vsb());

    if (text.size() == text_.get_length())
    {
        const ts::wchar *t = text_;
        for (int i = 0, n = text.size(); i < n; i++)
            if (!(text[i] == t[i])) goto notEq;
        return;
    }
notEq:
	ts::ivec2 prevCaretPos = get_caret_pos();
	if (text_replace(0, text.size(), text_, false))
		if (setCaretToEnd) set_caret_pos(text.size()); else set_caret_pos(prevCaretPos);
}

int gui_textedit_c::lines_count() const
{
	int lc = 1;
	for (int i=0, n=lines.count()-1; i<n; i++)
		if (text.get(lines.get(i).r1) == L'\n') lc++;
	return lc;
}

void gui_textedit_c::remove_lines(int r)
{
	ASSERT(r > 0);
	int lc = 1;
	for (int i=0, n=lines.count()-1; i<n; i++)
		if (text.get(lines.get(i).r1) == L'\n' && lc++ == r)
		{
			int m = lines.get(i).r1+1;
			ASSERT(lines.get(i+1).r0 == m);
			text.remove_some(0, m);
			lines.remove_slow(0, i+1);
			for (auto &l : lines) l -= m;

			caret_offset = caret_line = 0;
			return;
		}
	WARNING("Can't remove lines");
}

void gui_textedit_c::prepare_texture()
{
    if (flags.is(F_LINESDIRTY)) prepare_lines();
	flags.clear(F_TEXTUREDIRTY);

    ts::ivec2 asize = size();
    if (asize >> 0)
    {
    } else
        return;

    ts::tmp_tbuf_t<ts::glyph_image_s> glyphs, outlinedglyphs;

    if (!placeholder.is_empty() && text.size() == 0)
    {
        // placeholder only

        int w = asize.x;
        texture.ajust(ts::ivec2(w, asize.y), false);
        ts::ivec2 pen(ts::ui_scale(margin_left) + 4, (*font)->ascender + ts::ui_scale(margin_top));

        int advoffset = 0;
        int cnt = placeholder.get_length();
        for(int i=0;i<cnt;++i)
        {
            ts::wchar phc = placeholder.get_char(i);
            ts::glyph_image_s &gi = glyphs.add();
            ts::glyph_s &glyph = (*(*font))[phc];
            gi.width = (ts::uint16)glyph.width;
            gi.height = (ts::uint16)glyph.height;
            gi.pitch = (ts::uint16)glyph.width;
            gi.pixels = (ts::uint8*)(&glyph + 1);
            ts::ivec2 pos = ts::ivec2(advoffset, ts::ui_scale(baseline_offset)) + pen;
            gi.pos.x = (ts::int16)(glyph.left + pos.x);
            gi.pos.y = (ts::int16)(-glyph.top + pos.y);
            gi.color = placeholder_color;
            gi.thickness = 0;
            advoffset += glyph.advance;
        }

        ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, glyphs.array(), ts::ivec2(0), true);
        return;
    }

	ts::ivec2 visible_lines = (ts::ivec2(0, asize.y) + (scroll_top() - ts::ui_scale(margin_top)))/(*font)->height & ts::ivec2(0, lines.count()-1);
	int firstvischar = lines.get(visible_lines.r0).r0;
    int numcolors = lines.get(visible_lines.r1).r1 - firstvischar;
    ts::tmp_tbuf_t< ts::pair_s<ts::TSCOLOR, ts::TSCOLOR> > colors( numcolors );

	// colorize
	for (int i=0; i<numcolors; i++)
	{
        auto &c = colors.add();
		c.first=color; // base text color
		c.second=0;
	}

	if (start_sel!=-1 && focus()) // selection has always highest color priority
	{
		int cp=get_caret_char_index();
		ts::ivec2 selRange = (ts::ivec2(ts::tmin(start_sel,cp), ts::tmax(start_sel,cp)-1) - lines.get(visible_lines.r0).r0) & ts::ivec2(0, colors.count()-1);
		for (int i=selRange.r0; i<=selRange.r1; i++)
		{
			colors.get(i).first=selection_color;
			colors.get(i).second=selection_bg_color;
		}
	}

	int w = asize.x-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0);
	if (!CHECK(w > 0 && asize.y > 0)) return;
    texture.ajust(ts::ivec2(w, asize.y), false);

	
    texture.fill(ts::ivec2(0), ts::ivec2(w, asize.y), 0);

	ts::TSCOLOR current_color = 0;

#if 0
    // TODO : color of active element
	if (text.size())
		for (int i=lines.get(visible_lines.r0).r0; i>=0; --i) // color can be defined at upper line that is currently invisible, so search special change color symbol or line start
		{
			text_element_c &el = text.get(i);
			if (el == L'\n')
				break; // new line - reset current color
			else
				if (!el.is_char() && el.p->str.is_empty() && el.p->user_data_size == 0) // special element to change current color
				{
					current_color = el.p->color;
					break;
				}
		}
#endif

	// glyphs and selection
	for (int l = visible_lines.r0; l <= visible_lines.r1; ++l) // lines
	{
		if (l > 0 && text.get(lines.get(l).r0-1) == L'\n') current_color = 0; //-V807 // new line always reset current color
		ts::ivec2 pen(ts::ui_scale(margin_left) - scroll_left, (*font)->ascender + l*(*font)->height + ts::ui_scale(margin_top) - scroll_top());

		for (int i = lines.get(l).r0; i < lines.get(l).r1; ++i) // chars at current line
		{
			text_element_c &el = text.get(i);
			const ts::wchar *str = nullptr;
            active_element_s *ae = nullptr;
			int str_len = 0, advoffset = 0;
			ts::TSCOLOR cc = (current_color == 0 ? colors.get(i-firstvischar).first : current_color);
			if (!password_char)
				if (el.is_char())
					str = (ts::wchar*)&el.p + 1, str_len = 1;
				else
				{
                    ae = el.p;
#if 0
                    // TODO : color of active element
					if (el.p->str.is_empty() && el.p->user_data_size == 0) // special element to change current color
					{
						current_color = el.p->color;
						continue;
					}
					str = el.p->str;
					str_len = el.p->str.get_length();
					if (el.p->color/*if == 0, then color of text_element_c not set*/) cc = el.p->color;
#endif
				}
			else
				str = (ts::wchar*)&password_char, str_len = 1;

            for(;str_len;--str_len,++str)
			{
				ts::glyph_image_s &gi = glyphs.add();
				ts::glyph_s &glyph = (*(*font))[*str];
				gi.width  = (ts::uint16)glyph.width;
				gi.height = (ts::uint16)glyph.height;
                gi.pitch = (ts::uint16)glyph.width;
				gi.pixels = (ts::uint8*)(&glyph+1);
				ts::ivec2 pos = ts::ivec2(advoffset, ts::ui_scale(baseline_offset)) + pen;
				gi.pos.x = (ts::int16)(glyph.left + pos.x);
                gi.pos.y = (ts::int16)(-glyph.top + pos.y);
				gi.color = cc;
				gi.thickness = 0;
				advoffset += glyph.advance;
				if (outline_color)
					glyph.get_outlined_glyph(outlinedglyphs.add(), (*font), pos, outline_color);
            }

            if (ae && ae->advance)
            {
                ts::ivec2 pos = ts::ivec2(advoffset, ts::ui_scale(baseline_offset)) + pen;
                ts::glyph_image_s &gi = glyphs.add();
                ae->setup( pos, gi );
                advoffset += ae->advance;
            }

            ASSERT(advoffset == text_el_advance(i));


			if (ts::TSCOLOR c = colors.get(i-firstvischar).second)
			{
                ts::TSCOLOR *dst = (ts::TSCOLOR*)texture.body();
                int pitch = texture.info().pitch / sizeof(ts::TSCOLOR);
				int x = ts::tmax(0, pen.x), xx = ts::tmin(w, pen.x + advoffset), y = pen.y - (*font)->ascender;
				for (int ty = ts::tmax(0, y), yy = ts::tmin(asize.y, y + (*font)->height); ty < yy; ty++)
					for (int tx = x; tx < xx; tx++)
                        dst[ty * pitch + tx] = c;
			}

			pen.x += advoffset;
		}
	}

	// now render glyphs to texture
	if (outlinedglyphs.count() != 0)
		ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, outlinedglyphs.array(), ts::ivec2(0), false);
	ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, glyphs.array(), ts::ivec2(0), false);
}

void gui_textedit_c::selectword()
{
    if (flags.is(F_DISABLE_CARET)) return;
    int left, right = left = get_caret_char_index();
    for (; left > 0 && !IS_WORDB(text.get(left - 1).get_char()); left--);
    for (; right < text.size() && !IS_WORDB(text.get(right).get_char()); right++);
    start_sel = left;
    set_caret_pos(right);
    redraw();
}

void gui_textedit_c::ctx_menu_cut(const ts::str_c &)
{
    cut();
    gui->set_focus( getrid(), true );
}
void gui_textedit_c::ctx_menu_paste(const ts::str_c &)
{
    paste();
    gui->set_focus( getrid(), true );
}
void gui_textedit_c::ctx_menu_copy(const ts::str_c &)
{
    copy();
    gui->set_focus( getrid(), true );
}
void gui_textedit_c::ctx_menu_del(const ts::str_c &)
{
    del();
    gui->set_focus( getrid(), true );
}
void gui_textedit_c::ctx_menu_selall(const ts::str_c &)
{
    gui->set_focus( getrid(), true );
    selectall();
}

bool gui_textedit_c::ctxclosehandler(RID, GUIPARAM)
{
    gui->set_focus( getrid(), true );
    return true;
}

bool gui_textedit_c::summoncontextmenu()
{
    if (flags.is(F_DISABLE_CARET)) return false;

    AUTOCLEAR( flags, F_IGNOREFOCUSCHANGE );

    if (popupmenu)
    {
        TSDEL(popupmenu);
        return true;
    }
    menu_c mnu;

    int ss = sel_size();

    mnu.add(gui->app_loclabel(LL_CTXMENU_CUT), (ss == 0 || is_readonly()) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_cut) );
    mnu.add(gui->app_loclabel(LL_CTXMENU_COPY), (ss == 0) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_copy) );
    mnu.add(gui->app_loclabel(LL_CTXMENU_PASTE), is_readonly() ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_paste) );
    mnu.add(gui->app_loclabel(LL_CTXMENU_DELETE), (ss == 0 || is_readonly()) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_del) );
    mnu.add_separator();
    mnu.add(gui->app_loclabel(LL_CTXMENU_SELALL), (text.size() == 0) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_selall) );

    gui_popup_menu_c &pm = gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(),0), mnu);
    pm.set_close_handler( DELEGATE(this, ctxclosehandler) );
    popupmenu = &pm;
    return true;

}

/*virtual*/ void gui_textedit_c::created()
{
    flags.init(F_PREV_SB_VIS, is_vsb());

    set_font( nullptr );
    return __super::created();
}

/*virtual*/ bool gui_textedit_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp) // wheel must be handled here: default wheel processing will hide wheel event for this rectangle
    {
    case SQ_MOUSE_WHEELUP:
        if (is_vsb())
        {
            sbhelper.shift += (*font)->height;
            redraw();
        }
        break;
    case SQ_MOUSE_WHEELDOWN:
        if (is_vsb())
        {
            sbhelper.shift -= (*font)->height;
            redraw();
        }
        break;
    case SQ_MOUSE_L2CLICK:
        selectword();
        break;
    case SQ_MOUSE_LDOWN:
        {
            ts::ivec2 mplocal = to_local(data.mouse.screenpos);
            ts::irect clar = get_client_area();
            if (sbhelper.sbrect.inside(mplocal))
            {
                mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_SBMOVE);
                opd.mpos = data.mouse.screenpos;
                opd.rect.lt.y = sbhelper.sbrect.lt.y;
                opd.rect.lt.y -= clar.lt.y;
            } else if (clar.inside(mplocal) && !flags.is(F_DISABLE_CARET))
            {
                int osel = start_sel;
                int ocofs = caret_offset;
                int ocl = caret_line;
                set_caret_pos(mplocal - clar.lt);
                if ((GetAsyncKeyState(VK_SHIFT)  & 0x8000)==0x8000) ; // if Shift pressed, do not reset selection
                    else start_sel = get_caret_char_index();
                /*engine_operation_data_s &opd =*/ gui->begin_mousetrack(getrid(), MTT_TEXTSELECT);
                gui->set_focus(getrid());
                if (osel != start_sel || ocofs != caret_offset || ocl != caret_line) redraw();
                break;
            } else
                goto default_stuff;
        }
        break;
    case SQ_MOUSE_LUP:
        if (gui->end_mousetrack(getrid(), MTT_SBMOVE) || gui->end_mousetrack(getrid(), MTT_TEXTSELECT)) ; else 
        {
            bool d = true;
            for (const kbd_press_callback_s &cb : kbdhandlers)
            {
                if (SSK_LB == cb.scancode)
                {
                    ts::safe_ptr<rectengine_c> e(&getengine());
                    if (cb.handler(getrid(),this))
                        d = false;
                    if (!e) return true;
                    break;
                }
            }
            if (d)
                goto default_stuff;
        }
        break;
    case SQ_MOUSE_MOVE_OP:
        if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_SBMOVE))
        {
            int dmouse = data.mouse.screenpos().y - opd->mpos.y;
            if (sbhelper.scroll(opd->rect.lt.y + dmouse, get_client_area().height()))
                redraw();
        } else if (gui->mtrack(getrid(), MTT_TEXTSELECT))
        {
            if(start_sel == -1)
            {
                gui->end_mousetrack();
            } else
            {
                set_caret_pos(to_local(data.mouse.screenpos) - get_client_area().lt);
                redraw();
            }
            return true;
        }
        break;
    case SQ_MOUSE_RUP:
        if (!gui->mtrack(getrid(), MTT_SBMOVE) && !gui->mtrack(getrid(), MTT_TEXTSELECT))
        {
            if (summoncontextmenu())
                goto default_stuff;
        }
        break;
    case SQ_KEYDOWN:
    case SQ_KEYUP:
    case SQ_CHAR:
        return kbd_processing_(qp, data.kbd.charcode, data.kbd.scan);

    case SQ_MOUSE_OUT:
        if (flags.is(F_SBHL))
        {
            flags.clear(F_SBHL);
            getengine().redraw();
        }
        break;
    case SQ_MOUSE_MOVE:
        if (is_vsb() && !gui->mtrack(getrid(), MTT_SBMOVE))
        {
            bool of = flags.is(F_SBHL);
            flags.init(F_SBHL, sbhelper.sbrect.inside(to_local(data.mouse.screenpos)));
            if (flags.is(F_SBHL) != of)
                getengine().redraw();
        }
        if (!gui->mtrack(getrid(), MTT_ANY))
        {
            flags.clear(F_HANDCURSOR);
            under_mouse_active_element = nullptr;

            ts::ivec2 p = to_local(data.mouse.screenpos) - get_client_area().lt;

            p.x += scroll_left - ts::ui_scale(margin_left);
            ts::aint l = (p.y - ts::ui_scale(margin_top) + scroll_top()) / (*font)->height;
            if (unsigned(l) < (unsigned)lines.count())
            {
                ts::ivec2 line = lines.get(l);
                int x = 0, i = line.r0;
                for (; i < line.r1; i++)
                {
                    int adv = text_el_advance(i);
                    if (x + adv > p.x)
                    {
                        if (!text[i].is_char())
                        {
                            under_mouse_active_element = text[i].p;
                            under_mouse_active_element_pos = ts::ivec2(x + ts::ui_scale(margin_left), l * (*font)->height + ts::ui_scale(margin_top));
                            if (under_mouse_active_element->hand_cursor()) flags.set(F_HANDCURSOR);
                        }
                        break;
                    }
                    x += adv;
                }
            }
            //if (underMouseActiveElement == nullptr && flags.is(F_TRANSPARENT_ME)) return false;

            if (under_mouse_active_element)
            {
                // TODO active element click
            }
        }
        // no break here! do default stuff

    default:
    default_stuff:
        if (__super::sq_evt(qp, rid, data)) return true;
    }

    switch (qp)
    {
    case SQ_DRAW:
        {
            rectengine_root_c &root = SAFE_REF( getroot() );

            ts::irect drawarea = get_client_area();
            if (is_vsb())
            {
                sbhelper.set_size(lines.count()*(*font)->height + ts::ui_scale(margin_top) + ts::ui_scale(margin_bottom), drawarea.height()); // clamp scrollTop() value

                evt_data_s ds;
                ds.draw_thr.sbrect() = drawarea;
                const theme_rect_s *thr = themerect();
                int osbw = sbhelper.sbrect.width();
                if (thr) sbhelper.draw(thr, root, ds, flags.is(F_SBHL));
                if (osbw != sbhelper.sbrect.width())
                    flags.set(F_LINESDIRTY);
                drawarea.rb.x -= sbhelper.sbrect.width();


                if (!flags.is(F_PREV_SB_VIS))
                    cb_scrollbar_width(sbhelper.sbrect.width());
                flags.set(F_PREV_SB_VIS);

            } else if (flags.is(F_PREV_SB_VIS))
                cb_scrollbar_width(0), flags.clear(F_PREV_SB_VIS);


            if (flags.is(F_TEXTUREDIRTY))
                prepare_texture();

            bool gray = (is_readonly() && !is_disabled_caret()) || is_disabled();
            if (gray)
            {
                draw_data_s &dd = root.begin_draw();
                dd.alpha = 128;
            }

            root.draw( drawarea.lt, texture, ts::irect( ts::ivec2(0), drawarea.size() - ts::ivec2(margin_right,0) ), true );
            
            if (gray)
                root.end_draw();

            if (!flags.is(F_DISABLE_CARET))
            {
                caretrect.lt = drawarea.lt + get_caret_pos();
                caretrect.rb = caretrect.lt + ts::ivec2(ts::ui_scale(caret_width), (*font)->height);
                if (caretrect.intersect(drawarea))
                {
                    if(show_caret())
                        root.draw(caretrect, caret_color, true);

                    if (focus() && !flags.is(F_HEARTBEAT))
                        run_heartbeat();
                } else
                    flags.clear(F_HEARTBEAT);
            }

        }
        break;
    case SQ_RECT_CHANGED:
        if (data.changed.size_changed)
        {

            int i = get_caret_char_index();
            if (prepare_lines())
            {
                ASSERT(!flags.is(F_LINESDIRTY));
                if (!flags.is(F_TRANSPARENT_ME)) set_caret_pos(i);
                redraw();
            }

        }
        break;
    case SQ_DETECT_AREA:
        if (data.detectarea.area == 0 && !flags.is(F_TRANSPARENT_ME))
        {
            ts::irect clar = get_client_area();
            if (is_vsb())
                clar.rb.x -= sbhelper.sbrect.width();

            if (!flags.is(F_ARROW_CURSOR))
            if (clar.inside(data.detectarea.pos))
            {
                data.detectarea.area = AREA_EDITTEXT;
                if (flags.is(F_HANDCURSOR))
                    data.detectarea.area = AREA_HAND;
            }
        }
        return true;
    case SQ_FOCUS_CHANGED:
        if (flags.is(F_IGNOREFOCUSCHANGE)) return true;
        if (data.changed.focus)
        {
            if (!flags.is(F_HEARTBEAT))
                run_heartbeat();
            if (!flags.is(F_DISABLE_CARET))
                data.changed.is_active_focus = true;
        }
        if (selection_disallowed())
            start_sel = -1;
        redraw();
        return true;
    }

    return false;
}

bool gui_textedit_c::selection_disallowed() const
{
    if (start_sel < 0) return false;
    if (focus()) return false;
    if (popupmenu && gui->get_active_focus() == popupmenu->getrid()) return false;
    return true;
}

ts::uint32 gui_textedit_c::gm_handler(gmsg<GM_HEARTBEAT> &)
{
    if (selection_disallowed())
    {
        start_sel = -1;
        redraw();
    }
    return 0;
}