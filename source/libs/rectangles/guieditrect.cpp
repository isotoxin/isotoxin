#include "rectangles.h"

gui_textedit_c::gui_textedit_c(initial_rect_data_s &data) : gui_control_c(data), start_sel(-1), caret_line(0), caret_offset(0), scroll_left(0), under_mouse_active_element(nullptr), meta_text_length_limit(0)
{
    flags.set(F_CARET_SHOW);
	lines.add(ts::ivec2(0,0));
    font.assign( gui->default_font() );
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
    if (redraw_texture) flags.set(F_TEXTUREDIRTY);
    getengine().redraw();
}

bool gui_textedit_c::invert_caret(RID, GUIPARAM)
{
    gui->delete_event(DELEGATE(this, invert_caret));
    flags.invert(F_CARET_SHOW);
    redraw(false);
    flags.clear(F_HEARTBEAT);
    if (gui->get_focus() != getrid() && focus())
        gui->set_focus(getrid()); // get focus to papa
    return true;
}


void gui_textedit_c::run_heartbeat()
{
    if ( focus() && !flags.is(F_HEARTBEAT) )
    {
        DELAY_CALL(0.3, DELEGATE(this, invert_caret), nullptr);
        flags.set(F_HEARTBEAT);
    } else {
        flags.clear(F_HEARTBEAT);
    }
}

#if 0
void gui_textedit_c::changeUIScaleDefaultProcessing(int oldScale)
{
	Control::changeUIScaleDefaultProcessing(oldScale);
	font.update();
	for (int i=0; i<text.size(); i++) text[i].updateAdvance(font);
	scrollBar->setStepSize(ts::ui_scale(stepSize));
}

#endif

void gui_textedit_c::active_element_s::update_advance(ts::font_c *font)
{
	advance = 0;
	for (const ts::wchar *s = str; *s; s++)
		advance += (*font)[*s].advance;
}

gui_textedit_c::active_element_s *gui_textedit_c::create_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data, int user_data_size)
{
	active_element_s *el = active_element_s::create(user_data_size);
	el->str = str;
	el->color = color;
	el->user_data_size = user_data_size;
	if (user_data) memcpy(el + 1, user_data, user_data_size);
	el->update_advance(font);
	return el;
}

bool gui_textedit_c::text_replace(int pos, int num, const ts::wsptr &str, active_element_s **el, int len, bool updateCaretPos)
{
	if (check_text_func && !check_text_func(get_text().replace(pos, num, str))) return false;
	if (meta_text_length_limit)
	{
		int metaTextLength = 0;
		for (int i=0; i<text.size(); i++)	metaTextLength += text[i].meta_text_size();
		ASSERT(metaTextLength == get_meta_text().get_length());
		for (int i=0; i<len; i++) metaTextLength += ((text_element_c&)el[i]).meta_text_size();
		for (int i=0; i<num; i++) metaTextLength -= text[i+pos].meta_text_size();
		if (metaTextLength > meta_text_length_limit) return false;
	}
	if (!is_multiline())//если editbox однострочный
	{
        int editwidth = size().x;

		ts::wstr_c ttext = get_text().replace(pos, num, str);
		if (ttext.find_pos(L'\n') != -1) return false;//переводы строк в однострочном тексте запрещены

		if (chars_limit > 0) if (ttext.get_length() <= chars_limit) goto ok; else return false;

		int w = editwidth-ts::ui_scale(margin_left)-ts::ui_scale(margin_right)-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0);
		if (!password_char)
			for (ts::wchar c : ttext) w -= (*font)[c].advance;
		else
			w -= ttext.get_length() * (*font)[password_char].advance;
		if (w < 0) return false;//значит текст не умещается в строке - запрещаем
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

ts::wstr_c gui_textedit_c::text_substr(int start, int count) const
{
	ts::wstr_c r(count, true);
	for (int i=0; i<count; i++)
	{
		const text_element_c &te = text[start+i];
		if (te.is_char()) r.append_char( te.get_char_fast() ); else r.append(te.p->str);
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

void gui_textedit_c::insert_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data, int user_data_size, int cp)
{
	active_element_s *el = create_active_element(str, color, user_data, user_data_size);
	if (!text_replace(cp, 0, str, &el, 1)) el->die();
}

ts::ivec2 gui_textedit_c::get_caret_pos() const
{
	int i=0, x=0, s=lines.get(caret_line).r0;
	for (;i<caret_offset;i++) x+=text_el_advance(s+i);
	return ts::ivec2(x + ts::ui_scale(margin_left) - scroll_left, caret_line*font->height + ts::ui_scale(margin_top) - scroll_top());
}

void gui_textedit_c::scroll_to_caret()
{
    ts::ivec2 sz = size();
    bool dirty_texture = false;
	int newST = ts::tmax((caret_line+1)*font->height - sz.y + ts::ui_scale(margin_top) + ts::ui_scale(margin_bottom), ts::tmin(scroll_top(), caret_line*font->height));
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
	int line=ts::tmax(0,ts::tmin((p.y-ts::ui_scale(margin_top)+scroll_top())/font->height,lines.count()-1));
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
			if (start_sel==cp) start_sel=-1;//нужно чтобы избежать ошибочной обработки нажатий влево/вправо при выделенном нулевом тексте
		}
		else if (start_sel==-1) start_sel=cp;//на всякий случай ставим выделение

		if (GetKeyState(VK_CONTROL)<0)
		{
            if (flags.is(F_DISABLE_CARET)) return false;
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
			if (!shiftPressed && start_sel!=-1 && (scan==SSK_LEFT || scan==SSK_RIGHT))//Обрабатываем спец. случай - когда Shift не нажата, а выделение есть, и нажата клавиша влево или вправо - нужно переместить курсор в начало или в конец выделения прежде чем сбрасывать его
			{
				if (scan==SSK_LEFT) set_caret_pos(min(cp,start_sel));
				else             set_caret_pos(max(cp,start_sel));
				res = true;
			}
			else
			switch (scan)
			{
			case SSK_ESC:
				if (!(on_escape_press && on_escape_press(getrid(), this))) gui->set_focus(RID());
				res = true;
				break;
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
				set_caret_pos(get_caret_pos()+ts::ivec2(0,font->height*(scan==SSK_UP ? -1 : 1)));
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
				def=true;//означает что в данном обработчике нажатие клавиши не обрабатывается, а потому выделение сбрасывать не нужно (скорее всего нажатие такой клавиши обработается в onChar, где и будет сброшено выделение и произведены доп. действия)
				break;
			}
		}

		if (!def && start_sel!=-1 && !shiftPressed) start_sel=-1;//Сбрасываем выделение
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
			if (on_enter_press && on_enter_press(getrid(), this))
            {
                gui->set_focus(RID());
                return true;
            }
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
    if (sz.x <= 0) return false; // возможно контрол еще не отресайзен
	int linew=0,linestart=startchar;
	if (startchar == 0) lines.clear();
    int i=linestart;
	if (!is_multiline()) i=text.size(); else
	for (;i<text.size();i++)
	{
		if (text[i]==L'\n')//формируем новую строку
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
			else//Также формируем новую строку, но уже не по последнему пробелу, а по части слова, вмещающегося в строку
			{
				lines.add(ts::ivec2(linestart,i));
				linew=text_el_advance(i);
				linestart=i;
			}
		}
	}
	lines.add(ts::ivec2(linestart,i));

	under_mouse_active_element = nullptr;//при изменении текста указатель может стать битым, поэтому просто необходимо обнулять его
    flags.clear(F_LINESDIRTY);

    sbhelper.set_size( lines.count()*font->height+ ts::ui_scale(margin_top) + ts::ui_scale(margin_bottom), sz.y );

    if (caret_line >= lines.count()) caret_line = lines.count() - 1;

    return true;
}

void gui_textedit_c::append_meta_text(const ts::wstr_c &metatext)
{
	int startchar = text.size();
	if (startchar)
        text.addnew<ts::wchar>( L'\n' ), startchar++;

	for (int i=0; i<metatext.get_length(); i++)
	{
		if (metatext[i] != L'\1')
			text.addnew<ts::wchar>(metatext.get_char(i));
		else
		{
			int s = i + 1;
			i = metatext.find_pos(s, L'\1');
			if (i == -1) break;
			ts::wstr_c str(metatext.cstr() + s, i - s);
			i = metatext.find_pos(s = i + 1, L'\1');
			if (i == -1 || i - s < 8) break;
			ts::TSCOLOR color;
			ts::hex_to_data<ts::wchar>(&color, metatext.substr(s, s+8));
			s += 8;
			active_element_s *el = create_active_element(str, color, nullptr, (i - s)/2);
			ts::hex_to_data(el + 1, ts::wsptr(metatext.cstr() + s, i - s));
			text.addnew(el);
		}
	}

	bool wasAtEnd = is_vsb() && sbhelper.at_end( size().y );
	if (prepare_lines(startchar))
    {
	    redraw();
	    if (wasAtEnd) set_caret_pos(text.size());
    }
	start_sel = -1;
}

void gui_textedit_c::set_text(const ts::wstr_c &text_, bool setCaretToEnd)
{
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

ts::wstr_c gui_textedit_c::get_meta_text() const
{
	ts::wstr_c mt(text.size(), true);
	for (const text_element_c &te : text)
	{
		if (te.is_char())
		{
			ts::wchar ch = te.get_char_fast();
			if (ch != L'\1') mt.append_char(ch);
		}
		else
		{
			mt.append_char(L'\1');
			mt.append( te.p->str );
			mt.append_char(L'\1');
			mt.append_as_hex(&te.p->color, 4);
			mt.append_as_hex(te.p + 1, te.p->user_data_size);
			mt.append_char(L'\1');
		}
	}
	return mt;
}

ts::wstr_c gui_textedit_c::make_meta_text_from_active_element(const ts::wstr_c &str, ts::TSCOLOR color, const void *user_data, int user_data_size)
{
	ts::wstr_c mt(3 + str.get_length() + (4 + user_data_size)*2, true);
	mt.append_char(L'\1');
	mt.append(str);
	mt.append_char(L'\1');
	mt.append_as_hex(&color, 4);
	mt.append_as_hex(user_data, user_data_size);
	mt.append_char(L'\1');
	return mt;
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

			//scrollBar->setScrollRange(0, multiline() ? (float)lines.size()*font->height + ts::ui_scale(marginTop) + ts::ui_scale(marginBottom) : size.y);
			//scrollBar->correctScrollPos();
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
	ts::ivec2 visLines = (ts::ivec2(0, asize.y) + (scroll_top() - ts::ui_scale(margin_top)))/font->height & ts::ivec2(0, lines.count()-1);//getVisibleLinesRange();
	int firstvischar = lines.get(visLines.r0).r0;
    int numcolors = lines.get(visLines.r1).r1 - firstvischar;
    ts::tmp_tbuf_t< ts::pair_s<ts::TSCOLOR, ts::TSCOLOR> > colors( numcolors );

	//Раскрашиваем буковки
	for (int i=0; i<numcolors; i++)
	{
        auto &c = colors.add();
		c.first=color;//сначала основной цвет текста
		c.second=0;
	}

	if (start_sel!=-1 && focus())//выделение всегда имеет цветовой приоритет, поэтому делаем это в конце
	{
		int cp=get_caret_char_index();
		ts::ivec2 selRange = (ts::ivec2(ts::tmin(start_sel,cp), ts::tmax(start_sel,cp)-1) - lines.get(visLines.r0).r0) & ts::ivec2(0, colors.count()-1);
		for (int i=selRange.r0; i<=selRange.r1; i++)
		{
			colors.get(i).first=selection_color;
			colors.get(i).second=selection_bg_color;
		}
	}

	int w = asize.x-ts::ui_scale(is_vsb() ? sbhelper.sbrect.width() : 0);
	if (!CHECK(w > 0 && asize.y > 0)) return;
    texture.ajust(ts::ivec2(w, asize.y), false);

	
	{
        texture.fill(ts::ivec2(0), ts::ivec2(w, asize.y), 0);

		ts::TSCOLOR currentColor = 0;
		if (text.size())
			for (int i=lines.get(visLines.r0).r0; i>=0; i--)//цвет может быть изменён в вышестоящей строке, которую сейчас не видно, поэтому ищем спец. элемент изменения тек. цвета или начало строки
			{
				text_element_c &el = text.get(i);
				if (el == L'\n')
					break;//переход к новой строке равнозначен сбросу цвета
				else
					if (!el.is_char() && el.p->str.is_empty() && el.p->user_data_size == 0)//это спец. элемент изменения текущего цвета
					{
						currentColor = el.p->color;
						break;
					}
			}

		//Проставляем положение глифов и рисуем выделение
		for (int l = visLines.r0; l <= visLines.r1; l++)
		{
			if (l > 0 && text.get(lines.get(l).r0-1) == L'\n') currentColor = 0;//при переходе к новой строке, установленный цвет всегда сбрасывается
			ts::ivec2 pen(ts::ui_scale(margin_left) - scroll_left, font->ascender + l*font->height + ts::ui_scale(margin_top) - scroll_top());

			for (int i = lines.get(l).r0; i < lines.get(l).r1; i++)
			{
				text_element_c &el = text.get(i);
				const ts::wchar *str;
				int strLen, advoffset = 0;
				ts::TSCOLOR color = (currentColor == 0 ? colors.get(i-firstvischar).first : currentColor);
				if (!password_char)
					if (el.is_char())
						str = (ts::wchar*)&el.p + 1, strLen = 1;
					else
					{
						if (el.p->str.is_empty() && el.p->user_data_size == 0)//это спец. элемент изменения текущего цвета
						{
							currentColor = el.p->color;
							continue;
						}
						str = el.p->str;
						strLen = el.p->str.get_length();
						if (el.p->color/*если = 0, то цвет ActiveElement'а не задан*/) color = el.p->color;
					}
				else
					str = (ts::wchar*)&password_char, strLen = 1;

				ASSERT(strLen > 0);
				do
				{
					ts::glyph_image_s &gi = glyphs.add();
					ts::glyph_s &glyph = (*font)[*str];
					gi.width  = (ts::uint16)glyph.width;
					gi.height = (ts::uint16)glyph.height;
                    gi.pitch = (ts::uint16)glyph.width;
					gi.pixels = (ts::uint8*)(&glyph+1);
					ts::ivec2 pos = ts::ivec2(advoffset, ts::ui_scale(baseline_offset)) + pen;
					gi.pos.x = (ts::int16)(glyph.left + pos.x);
                    gi.pos.y = (ts::int16)(-glyph.top + pos.y);
					gi.color = color;
					gi.thickness = 0;
					advoffset += glyph.advance;
					if (outline_color)
						glyph.get_outlined_glyph(outlinedglyphs.add(), font, pos, outline_color);
				}
				while (str++, --strLen);

				ASSERT(advoffset == text_el_advance(i));

				if (ts::TSCOLOR c = colors.get(i-firstvischar).second)
				{
                    ts::TSCOLOR *dst = (ts::TSCOLOR*)texture.body();
                    int pitch = texture.info().pitch / sizeof(ts::TSCOLOR);
					int x = ts::tmax(0, pen.x), xx = ts::tmin(w, pen.x + advoffset), y = pen.y - font->ascender;
					for (int ty = ts::tmax(0, y), yy = ts::tmin(asize.y, y + font->height); ty < yy; ty++)
						for (int tx = x; tx < xx; tx++)
                            dst[ty * pitch + tx] = c;
				}

				pen.x += advoffset;
			}
		}

		//А теперь рисуем глифы в текстуру
		if (outlinedglyphs.count() != 0)
			ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, outlinedglyphs.array(), ts::ivec2(0), false);
		ts::text_rect_c::draw_glyphs(texture.body(), w, asize.y, texture.info().pitch, glyphs.array(), ts::ivec2(0), false);
	}

#if 0

	//Линия подчеркивания при наведении на элемент
	if (ui.underMouseControl == this && underMouseActiveElement)
	{
		srect r;
		r.min = pp + underMouseActiveElementPos + (svec2)ivec2(-scrollLeft_, /*font->ascender - font->ulinePos + ts::ui_scale(2)*/font->height - scrollTop_);
		r.max = r.min + svec2((short)underMouseActiveElement->advance, (short)max(font->ulineThickness, 1.f));
		r &= srect(p, p+size);
		if (r != srect(0))
		{
			SetDefaultStates();
			SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
			SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
			ApplyStates();
			imode.color(underMouseActiveElement->color);
			drawTexRect(r);
		}
	}

	scrollBar->show(scrollBarWidth > 0);
#endif

}

void gui_textedit_c::set_font(const ts::asptr&fname)
{
    font.assign(fname, false);
    bool scale_font = false;
    font.update(scale_font ? 100 : 0);
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
    auto gf = [this]()-> const ts::str_c&
    {
        const theme_rect_s *thr = themerect();
        if (thr && !thr->deffont.is_empty()) return thr->deffont;
        return gui->default_font();
    };
    set_font( gf() );

    return __super::created();
}

/*virtual*/ bool gui_textedit_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp) // колесо необходимо перехватить до вызова родительского обработчика, т.к. оттуда о колесе узнает хозяин и до этого ректа колесо вообще не дойдет
    {
    case SQ_MOUSE_WHEELUP:
        if (is_vsb())
        {
            sbhelper.shift += font->height;
            redraw();
        }
        break;
    case SQ_MOUSE_WHEELDOWN:
        if (is_vsb())
        {
            sbhelper.shift -= font->height;
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
                mousetrack_data_s &opd = getengine().begin_mousetrack(getrid(), MTT_SBMOVE);
                opd.mpos = data.mouse.screenpos;
                opd.rect.lt.y = sbhelper.sbrect.lt.y;
                opd.rect.lt.y -= clar.lt.y;
            } else if (clar.inside(mplocal) && !flags.is(F_DISABLE_CARET))
            {
                int osel = start_sel;
                int ocofs = caret_offset;
                int ocl = caret_line;
                set_caret_pos(mplocal - clar.lt);
                if ((GetAsyncKeyState(VK_SHIFT)  & 0x8000)==0x8000) ; //если Shift нажат, то выделение не сбрасываем
                    else start_sel = get_caret_char_index();
                /*engine_operation_data_s &opd =*/ getengine().begin_mousetrack(getrid(), MTT_TEXTSELECT);
                gui->set_focus(getrid());
                if (osel != start_sel || ocofs != caret_offset || ocl != caret_line) redraw();
                break;
            } else
                goto default_stuff;
        }
        break;
    case SQ_MOUSE_LUP:
        if (getengine().mtrack(getrid(), MTT_SBMOVE))
            getengine().end_mousetrack(MTT_SBMOVE);
        else if (getengine().mtrack(getrid(), MTT_TEXTSELECT))
            getengine().end_mousetrack(MTT_TEXTSELECT);
        else if (on_lbclick) {
            if (!on_lbclick(getrid(),this))
                goto default_stuff;
        } else
            goto default_stuff;
        break;
    case SQ_MOUSE_MOVE_OP:
        if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_SBMOVE))
        {
            int dmouse = data.mouse.screenpos().y - opd->mpos.y;
            if (sbhelper.scroll(opd->rect.lt.y + dmouse, get_client_area().height()))
                redraw();
        } else if (getengine().mtrack(getrid(), MTT_TEXTSELECT))
        {
            if(start_sel == -1)
            {
                getengine().end_mousetrack(MTT_TEXTSELECT);
            } else
            {
                set_caret_pos(to_local(data.mouse.screenpos) - get_client_area().lt);
                redraw();
            }
            return true;
        }
        break;
    case SQ_MOUSE_RUP:
        if (!getengine().mtrack(getrid(), MTT_SBMOVE) && !getengine().mtrack(getrid(), MTT_TEXTSELECT))
        {
            if (summoncontextmenu())
                goto default_stuff;
        }
        break;
    case SQ_KEYDOWN:
    case SQ_KEYUP:
    case SQ_CHAR:
        return kbd_processing_(qp, data.kbd.charcode, data.kbd.scan);
        
    case SQ_MOUSE_MOVE:
        if (!getengine().mtrack(getrid(), MTT_ANY))
        {
            flags.clear(F_HANDCURSOR);
            under_mouse_active_element = nullptr;

            ts::ivec2 p = to_local(data.mouse.screenpos) - get_client_area().lt;

            p.x += scroll_left - ts::ui_scale(margin_left);
            ts::aint l = (p.y - ts::ui_scale(margin_top) + scroll_top()) / font->height;
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
                            under_mouse_active_element_pos = ts::ivec2(x + ts::ui_scale(margin_left), l * font->height + ts::ui_scale(margin_top));
                            flags.set(F_HANDCURSOR);
                        }
                        break;
                    }
                    x += adv;
                }
            }
            //if (underMouseActiveElement == nullptr && flags.is(F_TRANSPARENT_ME)) return false;

            if (under_mouse_active_element)
            {
                if (active_element_mouse_event_func) 
                    if (active_element_mouse_event_func(qp, under_mouse_active_element->str, under_mouse_active_element + 1, under_mouse_active_element->user_data_size))
                        return true;
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
            HOLD rh(getroot());

            ts::irect drawarea = get_client_area();
            if (is_vsb())
            {
                sbhelper.set_size(lines.count()*font->height + ts::ui_scale(margin_top) + ts::ui_scale(margin_bottom), drawarea.height()); // clamp scrollTop() value

                evt_data_s ds;
                ds.draw_thr.sbrect() = drawarea;
                const theme_rect_s *thr = themerect();
                int osbw = sbhelper.sbrect.width();
                if (thr) sbhelper.draw(thr, rh.engine(), ds);
                if (osbw != sbhelper.sbrect.width())
                    flags.set(F_LINESDIRTY);
                drawarea.rb.x -= sbhelper.sbrect.width();
            }

            if (flags.is(F_TEXTUREDIRTY))
                prepare_texture();

            bool gray = (is_readonly() && !is_disabled_caret()) || is_disabled();
            if (gray)
            {
                draw_data_s &dd = rh.engine().begin_draw();
                dd.alpha = 128;
            }

            rh.engine().draw( drawarea.lt, texture, ts::irect( ts::ivec2(0), drawarea.size() - ts::ivec2(margin_right,0) ), true );
            
            if (gray)
                rh.engine().end_draw();

            if (!flags.is(F_DISABLE_CARET))
            {
                ts::irect r;
                r.lt = drawarea.lt + get_caret_pos();
                r.rb = r.lt + ts::ivec2(ts::ui_scale(caret_width), font->height);
                r.intersect(drawarea);
                if (!r.zero_square())
                {
                    if(show_caret())
                        rh.engine().draw(r, caret_color);

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
        redraw(true);
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
        redraw(true);
    }
    return 0;
}