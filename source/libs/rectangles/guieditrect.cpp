#include "rectangles.h"

gui_textedit_c::gui_textedit_c(initial_rect_data_s &data) : gui_control_c(data), start_sel(-1), under_mouse_active_element(nullptr)
{
    selection_color = gui->selection_color;
    selection_bg_color = gui->selection_bg_color;

    flags.set(F_CARET_SHOW);
	lines.add(ts::ivec2(0,0));
}

gui_textedit_c::~gui_textedit_c()
{
    if (gui) gui->delete_event( DELEGATE(this, invert_caret) );
}

bool gui_textedit_c::focus() const
{
    return gui->get_focus() == getrid();
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

bool gui_textedit_c::text_replace( int pos, int num, const ts::wsptr &str, text_element_c *el, int len, bool update_caret_pos)
{
    if (check_text_func && !flags.is(F_CHANGE_HANDLER))
    {
        ts::wstr_c placeholder_text_backup = placeholder_text; // hack to use less memory for text edit control

        int pos0 = pos, pos1 = pos + num;
        placeholder_text = get_text_and_fix_pos(&pos0, &pos1);
        ts::wstr_c old = placeholder_text;
        placeholder_text.replace(pos0, pos1 - pos0, str);

        flags.clear(F_CHANGED_DURING_CHANGE_HANDLER);
        AUTOCLEAR(flags, F_CHANGE_HANDLER);
        if (!check_text_func(placeholder_text, !old.equals( placeholder_text )))
        {
            placeholder_text = placeholder_text_backup;
            return false;
        }
        placeholder_text = placeholder_text_backup;
        if (flags.is(F_CHANGED_DURING_CHANGE_HANDLER))
        {
            flags.clear(F_CHANGED_DURING_CHANGE_HANDLER);
            return true;
        }
    }

    if (num > 0 || len > 0)
    {
        bool someselected = start_sel != -1;
        _undo.push( text.array(), get_caret_char_index(), start_sel, num > 1 || someselected ? CH_REPLACE : (len == 0 ? CH_DELCHAR : CH_ADDCHAR) );
        _redo.clear();
    }

	if (!is_multiline()) // singleline editbox
	{
        int editwidth = size().x;

        int pos0 = pos, pos1 = pos + num;
        ts::wstr_c ttext = get_text_and_fix_pos(&pos0, &pos1);
        ttext.replace(pos0, pos1 - pos0, str);


		if (ttext.find_pos(L'\n') != -1) return false; // no caret return allowed

		if (chars_limit > 0) if (ttext.get_length() <= chars_limit) goto ok; else return false;

		int w = editwidth-ts::ui_scale(margins_lt.x)-ts::ui_scale(margins_rb.x)-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0);
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
	    if (update_caret_pos) set_caret_pos(pos + len);
    }
    new_text( get_caret_char_index() );
	start_sel = -1;
	return true;
}

bool gui_textedit_c::text_replace( int pos, int num, const ts::wsptr &str, bool update_caret_pos)
{
	ts::tmp_array_inplace_t<text_element_c, 0> elements( str.l );

    if (is_multiline())
    {
        for(int i=0;i<str.l;++i)
            elements.add( text_element_c(str.s[i]));
	    return text_replace(pos, num, str, elements.begin(), (int)elements.size(), update_caret_pos);
    } else
    {
        bool nldetected = false;
        for (int i = 0; i < str.l; ++i)
            if (str.s[i] != '\n')
                elements.add( text_element_c(str.s[i]));
            else
                nldetected = true;
        if (nldetected)
        {
            ts::wstr_c x;
            ts::pwstr_c(str).extract_non_chars( x, CONSTWSTR("\n") );
            return text_replace(pos, num, x.as_sptr(), elements.begin(), (int)elements.size(), update_caret_pos);
        } else
            return text_replace(pos, num, str, elements.begin(), (int)elements.size(), update_caret_pos);
    }
}

ts::wstr_c gui_textedit_c::get_text_and_fix_pos( int *pos0, int *pos1) const
{
    int count = (int)text.size();
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
        if (te.is_char()) r.append_char(te.as_char()); else r.append(te.to_wstr());
    }

    if (!pos0fixed && *pos0 == count)
        *pos0 = r.get_length();

    if (!pos1fixed && *pos1 == count)
        *pos1 = r.get_length();

    return r;
}

ts::wstr_c gui_textedit_c::get_text_11( ts::wchar rc ) const
{
    int count = (int)text.size();
    ts::wstr_c r(count, true);
    for (int i = 0; i < count; ++i)
    {
        const text_element_c &te = text[i];
        r.append_char(te.is_char() ? te.as_char() : rc);
    }
    return r;
}

ts::wstr_c gui_textedit_c::text_substr( int start, int count) const
{
    ts::wstr_c r(count, true);
    for (int i = 0; i < count; ++i)
    {
        const text_element_c &te = text[start + i];
        if (te.is_char()) r.append_char(te.as_char()); else r.append(te.to_wstr());
    }
    return r;
}

ts::str_c gui_textedit_c::text_substr_utf8( int start, int count) const
{
	ts::str_c r(count, true);
	for ( ts::aint i=0; i<count; ++i)
	{
		const text_element_c &te = text[start+i];
		if (te.is_char()) r.append_unicode_as_utf8( te.as_char() ); else r.append(te.to_utf8());
	}
	return r;
}

bool gui_textedit_c::cut_( int cp, bool copy2clipboard)
{
	if (start_sel!=-1 && !is_readonly())
	{
        int len = ts::tabs(start_sel-cp);
		cp=ts::tmin(cp,start_sel);
		if (copy2clipboard)
        {
            ts::wstr_c t( text_substr(cp, len) );
            if (password_char) for(int i=0;i<t.get_length();++i) t.set_char(i,password_char);
            ts::set_clipboard_text(t);
        }
		text_erase(cp, len);
        return true;
	}
    return false;
}

bool gui_textedit_c::copy_( int cp )
{
	if (start_sel!=-1)
    {
        ts::wstr_c t(text_substr(ts::tmin(cp,start_sel),abs(start_sel-cp)));
        if (password_char) for (int i = 0; i < t.get_length(); ++i) t.set_char(i, password_char);
        ts::set_clipboard_text(t);
        return true;
    }
	return false;
}

void gui_textedit_c::paste_( int cp )
{
	if (is_readonly()) return;

    ts::wstr_c t(ts::get_clipboard_text());
    t.replace_all(CONSTWSTR("\r\n"), CONSTWSTR("\n"));
	text_replace(cp, t);
}

void gui_textedit_c::insert_active_element(active_element_s *ae, int cp)
{
    text_element_c te(ae);
    text_replace( cp, 0, ae->to_wstr(), &te, 1 );
}

ts::ivec2 gui_textedit_c::get_caret_pos() const
{
	int i=0, x=0, s=lines.get(caret_line).r0;
	for (;i<caret_offset;i++) x+=text_el_advance(s+i);
	return ts::ivec2(x + ts::ui_scale(margins_lt.x) - scroll_left, caret_line * (*font)->height + ts::ui_scale(margins_lt.y) - scroll_top());
}

void gui_textedit_c::scroll_to_caret()
{
    ts::ivec2 sz = size();
    bool dirty_texture = false;
	int newST = ts::tmax((caret_line+1) * (*font)->height - sz.y + ts::ui_scale(margins_lt.y) + ts::ui_scale(margins_rb.y), ts::tmin(scroll_top(), caret_line * (*font)->height));
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
		scroll_left = ts::tmax(caretX /*+ ts::ui_scale(caretWidth)*/ - editwidth + ts::ui_scale(margins_lt.x) + ts::ui_scale(margins_rb.x), ts::tmin(scroll_left, caretX));
		scroll_left = ts::tmax(0, ts::tmin(scroll_left, lineW + ts::ui_scale(margins_lt.x) + ts::ui_scale(margins_rb.x) - editwidth));
	}
	flags.set(F_CARET_SHOW);
    redraw(dirty_texture);
}

int gui_textedit_c::get_caret_char_index(ts::ivec2 p) const
{
    if (!font) return 0;
    p.x += scroll_left - ts::ui_scale(margins_lt.x);

    ts::aint line = ts::tmax(0, ts::tmin((p.y - ts::ui_scale(margins_lt.y) + scroll_top()) / (*font)->height, lines.count() - 1));
    int lineW = 0;
    int i = 0;
    for (; i < lines.get(line).delta(); i++)
    {
        int charW = text_el_advance(lines.get(line).r0 + i);
        if (p.x <= lineW + charW / 2) break;
        lineW += charW;
    }

    return lines.get(line).x + i;
}

void gui_textedit_c::set_caret_pos(ts::ivec2 p)
{
    if (!font) return;
	p.x += scroll_left - ts::ui_scale(margins_lt.x);
	ts::aint line=ts::tmax(0,ts::tmin((p.y-ts::ui_scale(margins_lt.y)+scroll_top())/(*font)->height,lines.count()-1));
	int lineW=0;
	int i=0;
	for (;i<lines.get(line).delta(); ++i)
	{
		int charW=text_el_advance(lines.get(line).r0+i);

		if (p.x<=lineW+charW/2) break;

		lineW+=charW;
	}
	caret_offset=i;
	caret_line=line;
	scroll_to_caret();
}

ts::ivec2 gui_textedit_c::get_char_pos( int pos ) const
{
    if (flags.is(F_LINESDIRTY))
        const_cast<gui_textedit_c *>(this)->prepare_lines();

	int i;
	for (i=0;i<lines.count()-1;i++)
		if (pos<=lines.get(i).r1) break;

	return ts::ivec2(pos-lines.get(i).r0, i);
}

void gui_textedit_c::set_caret_pos( int cp )
{
	ts::ivec2 p = get_char_pos(cp);
	caret_offset=p.x;
	caret_line=p.y;
	scroll_to_caret();
}

bool gui_textedit_c::kbd_processing_(system_query_e qp, ts::wchar charcode, int scan, int casw )
{
	bool res = false;
    int cp = 0;
	if (qp == SQ_KEYDOWN)
	{
		cp=get_caret_char_index();
		bool def=false, key_shift = (casw & ts::casw_shift) != 0;

		if (!key_shift)
		{
			if (start_sel==cp) start_sel=-1; // if zero-len selection - avoid handle of left/rite key pressed
		}
		else if (start_sel==-1) start_sel = cp;

		if ( ( casw & ts::casw_ctrl ) != 0 )
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
            case ts::SSK_RSHIFT:
			case ts::SSK_LSHIFT:
				if (start_sel==-1) start_sel=cp;
				res = true;
				break;
			case ts::SSK_A:
				selectall();
				res = true;
				break;
            case ts::SSK_W:
                selectword();
                res = true;
                break;
            case ts::SSK_Z:
                undo();
                res = true;
                break;
            case ts::SSK_Y:
                redo();
                res = true;
                break;
			case ts::SSK_HOME:
                if (text.size() == 0) return false;
				caret_offset = 0;
				caret_line = 0;
				scroll_to_caret();
				res = true;
				def = false;
				break;
			case ts::SSK_END:
                if (text.size() == 0) return false;
                end();
				res = true;
				def = false;
				break;
			case ts::SSK_C:
			case ts::SSK_INSERT:
				res = copy_(cp);
				break;
			case ts::SSK_V:
				paste_(cp);
				res = true;
				break;
			case ts::SSK_X:
				res = cut_(cp);
				break;
			case ts::SSK_LEFT:
				for (;cp>0 &&  IS_WORDB(text.get(cp-1).as_char());cp--);
				for (;cp>0 && !IS_WORDB(text.get(cp-1).as_char());cp--);
				set_caret_pos(cp);
				res = true;
				break;
			case ts::SSK_RIGHT:
				for (;cp<text.size() && !IS_WORDB(text.get(cp).as_char());cp++);
				for (;cp<text.size() &&  IS_WORDB(text.get(cp).as_char());cp++);
				set_caret_pos(cp);
				res = true;
				break;
            case ts::SSK_ENTER:
                cp=get_caret_char_index();
                goto do_enter_key;
			}
		}
		else
		{
            if (flags.is(F_DISABLE_CARET)) return false;
			if (!key_shift && start_sel!=-1 && (scan== ts::SSK_LEFT || scan== ts::SSK_RIGHT)) // special case: Shift not pressed but selection present and left or right key pressed - caret must be moved to start or end of selection before selection be reset
			{
				if (scan== ts::SSK_LEFT) set_caret_pos(ts::tmin(cp,start_sel));
				else set_caret_pos( ts::tmax(cp,start_sel));
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
			    case ts::SSK_LEFT:
				    if (caret_offset>0) caret_offset--;
				    else if (caret_line>0) caret_offset=lines.get(--caret_line).delta();
				    scroll_to_caret();
				    res = true;
				    break;
			    case ts::SSK_RIGHT:
				    if (caret_offset<lines.get(caret_line).delta()) ++caret_offset;
				    else if (caret_line<lines.count()-1) caret_offset=0, ++caret_line;
				    scroll_to_caret();
				    res = true;
				    break;
			    case ts::SSK_UP:
			    case ts::SSK_DOWN:
                    if (text.size() == 0) return false;
				    set_caret_pos(get_caret_pos()+ts::ivec2(0,(*font)->height*(scan== ts::SSK_UP ? -1 : 1)));
				    res = true;
				    break;
			    case ts::SSK_HOME:
                    if (text.size() == 0) return false;
				    caret_offset=0;
				    scroll_to_caret();
				    res = true;
				    break;
			    case ts::SSK_END:
                    if (text.size() == 0) return false;
				    caret_offset=lines.get(caret_line).delta();
				    scroll_to_caret();
				    res = true;
				    break;
			    case ts::SSK_INSERT:
				    if (is_readonly()) def=true; else
				    if (key_shift) paste_(cp); else def=true;
				    res = true;
				    break;
			    case ts::SSK_DELETE:
				    if (is_readonly()) def=true; else
				    if (key_shift)
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
                case ts::SSK_BACKSPACE:
                    if ( is_readonly() ) def = true; else
                    {
                        if ( casw == ts::casw_alt )
                        {
                            undo();
                        } else if ( 0 == (casw & ts::casw_shift) )
                        {
                            if ( start_sel != -1 )
                            {
                                text_erase( cp );
                            }
                            else if ( cp > 0 )
                            {
                                text_erase( cp - 1, 1 );
                            }
                        }
                    }
                    res = true;
                    break;
                case ts::SSK_TAB:
                    if ( casw & ts::casw_shift )
                    {
                        getroot()->tab_focus( getrid(), false );
                    }
                    else
                    {
                        getroot()->tab_focus( getrid() );
                    }
                    return false;
                case ts::SSK_ESC:

                    if ( start_sel < 0 )
                    {
                        evt_data_s d;
                        d.kbd.scan = scan;
                        d.kbd.charcode = 0;
                        d.kbd.casw = casw;

                        return HOLD( getparent() )( ).sq_evt( SQ_KEYDOWN, getparent(), d );
                    }

                    res = true;
                    break;
                default:
				    def=true; // do not clear selection
				    break;
			    }
            }
		}

		if (!def && start_sel!=-1 && !key_shift) start_sel=-1; // clear selection
	}
	else if (qp == SQ_CHAR)
	{
		if (is_readonly()) return false;

		cp=get_caret_char_index();

		switch (charcode)
		{
        case '\r':
            do_enter_key:
			charcode='\n';
            _undo.push(text.array(), get_caret_char_index(), start_sel, CH_ADDSPECIAL );

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
	if (!is_multiline()) i = (int)text.size(); else
	for (;i<text.size();i++)
	{
        auto rmargin = [&]() ->ts::aint
        {
            ts::aint curh = (lines.count() + 1) * (*font)->height;
            return curh >= rb_margin_from ? margins_rb.x : 0;
        };

		if (text[i]==L'\n') // build new line
		{
			lines.add(ts::ivec2(linestart,i));
			linestart=i+1;
			linew=0;
			continue;
		}

		linew+=text_el_advance(i);
		if (linew>sz.x-ts::ui_scale(margins_lt.x)-ts::ui_scale(rmargin())-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0) && /*обязательно проверяем - вдруг это первый символ, т.к. ситуация когда символ не помещается во всей строке не может быть корректно обработана*/i>linestart)
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

    sbhelper.set_size( lines.count() * (*font)->height + ts::ui_scale(margins_lt.y) + ts::ui_scale(margins_rb.y), sz.y );

    if (caret_line >= lines.count()) caret_line = lines.count() - 1;

    return true;
}

void gui_textedit_c::set_placeholder(GET_TOOLTIP plht)
{
    bool need_redraw = false;
    if (plht != placeholder)
        placeholder = plht, need_redraw = text.size() == 0;

    if (!need_redraw && placeholder && placeholder() != placeholder_text)
        need_redraw = true;

    if (need_redraw)
    {
        flags.set(F_TEXTUREDIRTY);
        redraw();
    }
}

void gui_textedit_c::set_text(const ts::wstr_c &text_, bool move_caret2end, bool setup_undo)
{
    if (text_.is_empty())
        flags.clear(F_LOCKED);

    if (flags.is(F_CHANGE_HANDLER))
    {
        if (placeholder_text.equals(text_)) return; // why placeholder_text? it's a hack! see text_replace
        flags.set(F_CHANGED_DURING_CHANGE_HANDLER);

    } else
    {
        flags.init(F_PREV_SB_VIS, is_vsb());

        if (text.size() == text_.get_length())
        {
            const ts::wchar *t = text_.cstr();
            for ( ts::aint i = 0, n = text.size(); i < n; ++i)
                if (!(text[i] == t[i])) goto notEq;
            return;
        }
    notEq: ;
    }


	ts::ivec2 prev_caret_pos = get_caret_pos();
	if (text_replace(0, (int)text.size(), text_, false))
		if (move_caret2end) set_caret_pos( (int)text.size()); else set_caret_pos(prev_caret_pos);

    if (setup_undo && !flags.is(F_CHANGE_HANDLER))
    {
        _redo.clear();
        _undo.clear();
        _undo.push( text.array(), get_caret_char_index(), start_sel, CH_REPLACE );
    }
}

ts::aint gui_textedit_c::lines_count() const
{
    ts::aint lc = 1;
	for ( ts::aint i=0, n=lines.count()-1; i<n; ++ i)
		if (text.get(lines.get(i).r1) == L'\n') ++lc;
	return lc;
}

void gui_textedit_c::remove_lines( ts::aint r)
{
	ASSERT(r > 0);
	int lc = 1;
	for ( ts::aint i=0, n=lines.count()-1; i<n; i++)
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
    if ( flags.is( F_LINESDIRTY ) )
    {
        int pos = get_caret_char_index();
        prepare_lines();
        set_caret_pos(pos);
    }
	flags.clear(F_TEXTUREDIRTY);

    ts::ivec2 asize = size();
    if (asize >> 0)
    {
    } else
        return;

    ts::tmp_tbuf_t<ts::glyph_image_s> glyphs, outlinedglyphs;

    if (!placeholder_text.is_empty() && text.size() == 0)
    {
        // placeholder only

        int w = asize.x;
        texture.ajust_ARGB(ts::ivec2(w, asize.y), false);
        ts::ivec2 pen(ts::ui_scale(margins_lt.x) + 4, (*font)->ascender + ts::ui_scale(margins_lt.y));

        int advoffset = 0;
        ts::aint cnt = placeholder_text.get_length();
        for(int i=0;i<cnt;++i)
        {
            ts::wchar phc = placeholder_text.get_char(i);
            ts::glyph_image_s &gi = glyphs.add();
            ts::glyph_s &glyph = (*(*font))[phc];
            gi.width = (ts::uint16)glyph.width;
            gi.height = (ts::uint16)glyph.height;
            gi.pitch = (ts::uint16)glyph.width;
            gi.pixels = (ts::uint8*)(&glyph + 1);
            ts::ivec2 pos = ts::ivec2(advoffset, ts::ui_scale(baseline_offset)) + pen;
            gi.pos().x = (ts::int16)(glyph.left + pos.x);
            gi.pos().y = (ts::int16)(-glyph.top + pos.y);
            gi.color = placeholder_color;
            gi.thickness = 0;
            advoffset += glyph.advance;
        }

        ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, glyphs.array(), ts::ivec2(0), true);
        return;
    }

	ts::ivec2 visible_lines = (ts::ivec2(0, asize.y) + (scroll_top() - ts::ui_scale(margins_lt.y)))/(*font)->height & ts::ivec2(0, lines.count()-1);
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
        ts::aint cp = get_caret_char_index();
		ts::ivec2 selRange = (ts::ivec2(ts::tmin(start_sel,cp), ts::tmax(start_sel,cp)-1) - lines.get(visible_lines.r0).r0) & ts::ivec2(0, colors.count()-1);
		for (int i=selRange.r0; i<=selRange.r1; i++)
		{
			colors.get(i).first=selection_color;
			colors.get(i).second=selection_bg_color;
		}
	}

	int w = asize.x-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0);
	if (!CHECK(w > 0 && asize.y > 0)) return;
    texture.ajust_ARGB(ts::ivec2(w, asize.y), false);

	
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

    const ts::buf0_c *badwords = bad_words ? bad_words() : nullptr;
    ts::tmp_tbuf_t<ts::ivec3> bads;
    bool prevbad = false;

	// glyphs and selection
	for (int l = visible_lines.r0; l <= visible_lines.r1; ++l) // lines
	{
		if (l > 0 && text.get(lines.get(l).r0-1) == L'\n') current_color = 0; //-V807 // new line always reset current color
		ts::ivec2 pen(ts::ui_scale(margins_lt.x) - scroll_left, (*font)->ascender + l*(*font)->height + ts::ui_scale(margins_lt.y) - scroll_top());

		for (int i = lines.get(l).r0; i < lines.get(l).r1; ++i) // chars at current line
		{
			text_element_c &el = text.get(i);

            bool is_bad_char = false;
            if (badwords) // if spellchecker is active
            {
                is_bad_char = badwords->get_bit(i);
                if ((!prevbad && is_bad_char) || (prevbad && bads.last(ts::ivec3(0)).z != pen.y))
                {
                    ts::ivec3 &bx = bads.add();
                    bx.r0 = pen.x;
                    bx.r1 = pen.x;
                    bx.z = pen.y;
                }
                prevbad = is_bad_char;
            }

			const ts::wchar *str = nullptr;
            active_element_s *ae = nullptr;
			int str_len = 0, advoffset = 0;
            ts::wchar dummychar;
			ts::TSCOLOR cc = (current_color == 0 ? colors.get(i-firstvischar).first : current_color);
			if (!password_char)
				if (el.is_char())
					str = &dummychar, dummychar = el.as_char(), str_len = 1;
				else
				{
                    ae = el.get_ae();
#if 0
                    // TODO : color of active element
					if (ae->str.is_empty() && ae->user_data_size == 0) // special element to change current color
					{
						current_color = ae->color;
						continue;
					}
					str = ae->str;
					str_len = ae->str.get_length();
					if (ae->color/*if == 0, then color of text_element_c not set*/) cc = ae->color;
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
				gi.pos().x = (ts::int16)(glyph.left + pos.x);
                gi.pos().y = (ts::int16)(-glyph.top + pos.y);
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
                ae->setup( ( *font ), pos, gi );
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

            if (is_bad_char)
            {
                ASSERT(bads.count() > 0);
                bads.get(bads.count() - 1).r1 = pen.x;
            }

		}
	}

	// now render glyphs to texture
	if (outlinedglyphs.count() != 0)
		ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, outlinedglyphs.array(), ts::ivec2(0), false);
	ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, glyphs.array(), ts::ivec2(0), false);

    if (bads.count() > 0)
    {
        static const ts::uint8 addy[] = { 0,0,1,2,2,1 };

        // draw spellchecker underline
        for( ts::ivec3 &l : bads )
        {
            const ts::ivec2 &tsz = texture.info().sz;
            if (l.r0 >= tsz.x) continue;
            if (l.z >= tsz.y) continue;
            if ( l.r1 <= 0 ) continue;
            if ( l.r0 < 0 ) l.r0 = 0;
            if ( l.r1 >= tsz.x) l.r1 = tsz.x - 1;
            if (l.r0 >= l.r1) continue;
            for (int i = 0; l.r0 < l.r1; ++l.r0)
            {
                ASSERT(l.r0 >= 0 && l.r0 < tsz.x);
                int y = l.z + addy[i];
                if (y >= 0 && y < tsz.y)
                {
                    ts::uint8 *dst = texture.body(ts::ivec2(l.r0, y));
                    *(ts::TSCOLOR *)dst = badword_undeline_color;
                }
                ++i;
                if (i >= ARRAY_SIZE(addy)) i = 0;
            }
        }
    }
}

void gui_textedit_c::selectword()
{
    if (flags.is(F_DISABLE_CARET)) return;
    int left, right = left = get_caret_char_index();
    for (; left > 0 && !IS_WORDB(text.get(left - 1).as_char()); --left);
    for (; right < text.size() && !IS_WORDB(text.get(right).as_char()); ++right);
    start_sel = left;
    set_caret_pos(right);
    redraw();
}

void gui_textedit_c::undo()
{
    if (_undo.size() == 0) return;
    _redo.push(text.array(), get_caret_char_index(), start_sel, CH_REPLACE);
    snapshot_s &ls = _undo.last();
    ts::wstr_c oldt = get_text();
    text.replace(0, text.size(), ls.text.begin(), ls.text.size() );
    ts::wstr_c newt = get_text();
    if (check_text_func) check_text_func(newt, !oldt.equals(newt));
    start_sel = ls.selindex;
    flags.set(F_LINESDIRTY | F_TEXTUREDIRTY);
    set_caret_pos(ls.caret_index);
    _undo.truncate(_undo.size() - 1);
    new_text( get_caret_char_index() );
    redraw();
}
void gui_textedit_c::redo()
{
    if (_redo.size() == 0) return;
    _undo.push(text.array(), get_caret_char_index(), start_sel, CH_REPLACE);
    snapshot_s &ls = _redo.last();
    ts::wstr_c oldt = get_text();
    text.replace(0, text.size(), ls.text.begin(), ls.text.size());
    ts::wstr_c newt = get_text();
    if (check_text_func) check_text_func( newt, !oldt.equals( newt ) );
    start_sel = ls.selindex;
    flags.set(F_LINESDIRTY | F_TEXTUREDIRTY);
    set_caret_pos(ls.caret_index);
    _redo.truncate(_redo.size() - 1);
    new_text( get_caret_char_index() );
    redraw();
}

void gui_textedit_c::ctx_menu_cut(const ts::str_c &)
{
    cut();
    gui->set_focus( getrid() );
}
void gui_textedit_c::ctx_menu_paste(const ts::str_c &)
{
    paste();
    gui->set_focus( getrid() );
}
void gui_textedit_c::ctx_menu_copy(const ts::str_c &)
{
    copy();
    gui->set_focus( getrid() );
}
void gui_textedit_c::ctx_menu_del(const ts::str_c &)
{
    del();
    gui->set_focus( getrid() );
}
void gui_textedit_c::ctx_menu_selall(const ts::str_c &)
{
    gui->set_focus( getrid() );
    selectall();
}

bool gui_textedit_c::ctxclosehandler(RID, GUIPARAM)
{
    gui->set_focus( getrid() );
    return true;
}

bool gui_textedit_c::summoncontextmenu( ts::aint cp)
{
    if (flags.is(F_DISABLE_CARET)) return false;

    AUTOCLEAR( flags, F_IGNOREFOCUSCHANGE );

    if (popupmenu)
    {
        TSDEL(popupmenu);
        return true;
    }
    menu_c mnu;

    ts::aint ss = sel_size();

    if (ctx_menu_func)
    {
        ctx_menu_func(mnu, cp);
        if (!mnu.is_empty()) mnu.add_separator();
    }

    mnu.add(gui->app_loclabel(LL_CTXMENU_CUT), (ss == 0 || is_readonly()) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_cut) );
    mnu.add(gui->app_loclabel(LL_CTXMENU_COPY), (ss == 0) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_copy) );
    mnu.add(gui->app_loclabel(LL_CTXMENU_PASTE), is_readonly() ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_paste) );
    mnu.add(gui->app_loclabel(LL_CTXMENU_DELETE), (ss == 0 || is_readonly()) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_del) );
    mnu.add_separator();
    mnu.add(gui->app_loclabel(LL_CTXMENU_SELALL), (text.size() == 0) ? MIF_DISABLED : 0, DELEGATE(this,ctx_menu_selall) );

    gui_popup_menu_c &pm = gui_popup_menu_c::show(menu_anchor_s(true), mnu);
    pm.set_close_handler( DELEGATE(this, ctxclosehandler) );
    popupmenu = &pm;
    popupmenu->leech(this);
    return true;

}

/*virtual*/ void gui_textedit_c::created()
{
    flags.init(F_PREV_SB_VIS, is_vsb());

    set_font( nullptr );
    __super::created();

    color = get_default_text_color();
    caret_color = color;

    getroot()->register_afocus( this, !flags.is( F_DISABLE_CARET ) );
}

ts::uint32 gui_textedit_c::gm_handler(gmsg<GM_UI_EVENT>&ue)
{
    if (ue.evt == UE_THEMECHANGED)
    {
        color = get_default_text_color();
        caret_color = color;
        redraw();
    }

    return 0;
}



/*virtual*/ bool gui_textedit_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid())
    {
        // from submenu
        if (popupmenu && popupmenu->getrid() == rid)
        {
            if (SQ_POPUP_MENU_DIE == qp)
            {
                gui->set_focus(getrid());
            }
        }
        return false;
    }

    switch (qp) // wheel must be handled here: default wheel processing will hide wheel event for this rectangle
    {
    case SQ_MOUSE_WHEELUP:
        if (is_vsb())
        {
            sbhelper.shift += (*font)->height;
            redraw();
            break;
        }
        goto default_stuff;
    case SQ_MOUSE_WHEELDOWN:
        if (is_vsb())
        {
            sbhelper.shift -= (*font)->height;
            redraw();
            break;
        }
        goto default_stuff;
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
                ts::aint osel = start_sel;
                int ocofs = caret_offset;
                int ocl = caret_line;
                set_caret_pos(mplocal - clar.lt);
                if (ts::master().is_key_pressed(ts::SSK_SHIFT)) ; // if Shift pressed, do not reset selection
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
                if ( ts::SSK_LB == cb.scancode)
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
            if (summoncontextmenu(get_caret_char_index(to_local(data.mouse.screenpos) - get_client_area().lt)))
                goto default_stuff;
        }
        break;
    case SQ_KEYDOWN:
    case SQ_KEYUP:
    case SQ_CHAR:
        return kbd_processing_(qp, data.kbd.charcode, data.kbd.scan, data.kbd.casw);

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

            p.x += scroll_left - ts::ui_scale(margins_lt.x);
            ts::aint l = (p.y - ts::ui_scale(margins_lt.y) + scroll_top()) / (*font)->height;
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
                            under_mouse_active_element = text[i].get_ae();
                            under_mouse_active_element_pos = ts::ivec2(x + ts::ui_scale(margins_lt.x), l * (*font)->height + ts::ui_scale(margins_lt.y));
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
        if (rid != getrid()) return false;
        {
            if (text.size() == 0 && placeholder)
            {
                ts::wstr_c plht = placeholder();
                if (placeholder_text != plht)
                {
                    placeholder_text = plht;
                    flags.set(F_TEXTUREDIRTY);
                }
            }


            rectengine_root_c &root = SAFE_REF( getroot() );

            ts::irect drawarea = get_client_area();
            if (is_vsb())
            {
                sbhelper.set_size(lines.count()*(*font)->height + ts::ui_scale(margins_lt.y) + ts::ui_scale(margins_rb.y), drawarea.height()); // clamp scrollTop() value

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

            root.draw( drawarea.lt, texture.extbody(ts::irect(ts::ivec2(0), drawarea.size() - ts::ivec2( rb_margin_from > 0 ? 0 : margins_rb.x, 0))), true );
            
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
            flags.set( F_CARET_SHOW );
            if (!flags.is(F_HEARTBEAT))
                run_heartbeat();
        }
        if (selection_disallowed())
            start_sel = -1;
        redraw();
        return true;
    }

    return false;
}

void gui_textedit_c::disable_caret( bool dc )
{
    flags.init( F_DISABLE_CARET, dc );
    getroot()->register_afocus( this, accept_focus() );
}

bool gui_textedit_c::selection_disallowed() const
{
    if (start_sel < 0) return false;
    if (focus()) return false;
    if (popupmenu && gui->get_focus() == popupmenu->getrid()) return false;
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

void gui_textedit_c::snapshots_s::push(const ts::array_wrapper_c<const text_element_c> &txt, ts::aint caret, ts::aint ss, change_e ch)
{
    if (0 == size() || CH_REPLACE == ch || CH_ADDSPECIAL == ch)
    {
        addnew(txt, caret, ss, CH_ADDSPECIAL == ch ? CH_ADDCHAR : ch);
        return;
    }

    snapshot_s &ls = last();
    if (ls.ch == ch)
    {
        if (CH_ADDCHAR == ch && caret == ls.last_caret + 1) { ls.last_caret = caret; return; }
        else if (CH_DELCHAR == ch && (caret == ls.last_caret || caret == ls.last_caret - 1)) { ls.last_caret = caret; return; }
    }
    if (ls.text == txt) return;

    addnew(txt, caret, ss, ch);

    if (size() > 100) // limit with 100 undo items
        remove_slow(0);
}

