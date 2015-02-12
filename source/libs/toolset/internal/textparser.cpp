#include "toolset.h"
#include "textparser.h"

namespace ts
{
font_desc_c g_default_text_font;

template<typename VECCTYPE, int VECLEN, typename TCHARACTER> vec_t<VECCTYPE, VECLEN> as_vec( const sptr<TCHARACTER> &s )
{
    vec_t<VECCTYPE, VECLEN> r(0);
    int index = 0;
    for (token<TCHARACTER> t(s, TCHARACTER(',')); t && index < VECLEN; ++t, ++index)
        r[index] = t->as_num<VECCTYPE>();
    return r;
}

namespace
{
    struct meta_glyph_s //структура, которая представляет глиф на стадии парсинга текста
    {
	    TSCOLOR color;
	    font_c *font;
	    short underlined,//-1 - нет подчеркивания, 1 - есть, 0 - подчеркивание запрещено
		      underline_offset;
	    int advance;//горизонтальный размер метаглифа
	    short shadow;
	    wchar_t ch;
        int charindex;
	    TSCOLOR shadow_color, outline_color;
	    int image_offset_Y;

	    enum mgtype_e {
		    //INDENT,//отступ в начале абзаца
		    CHAR, // обычный символ
		    SPACE, // пробел
		    IMAGE, // картинка
            RECTANGLE, // пустая прямоугольная область
	    } type;

	    union
	    {
		    glyph_s *glyph; //CHAR
		    scaled_image_s *image; //IMAGE
	    };

        int calch() const
        {
            switch (type)
            {
                case meta_glyph_s::CHAR:
                case meta_glyph_s::SPACE:
                    return glyph->top-glyph->height;
                case meta_glyph_s::IMAGE:
                    return image->height;
                case meta_glyph_s::RECTANGLE:
                    return shadow;
            }

            FORBIDDEN();
            return 0;
        }

	    void export_to_glyph_image(GLYPHS *glyphs, const ivec2 &pos, int add_underline_len, TSCOLOR color) const
	    {
		    glyph_image_s &gi = glyphs->add();
		    switch (type)
		    {
		    case CHAR:
		    case SPACE:
			    if (color == 0)//в этом случае можно было бы вообще не добавлять символ для оптимизации, но цвет может потом меняться - blinking (актуально для span напр.)
				     gi.color = ARGB(255,255,255,0);//т.к. 0 трактуется как RGBA изображение, а у нас ALPHA-глиф, надо подставить что-нить эквивалентное, но не = 0
			    else gi.color = color;
			    gi.width  = (uint16)glyph->width;
			    gi.height = (uint16)glyph->height;
                gi.pitch = (uint16)glyph->width; // для глифов 1 байт на пиксель
			    gi.pixels = (uint8*)(glyph+1);
			    gi.pos = svec2((int16)glyph->left, (int16)(-glyph->top));
			    break;
		    case IMAGE:
			    gi.color = 0;//для картинок текущий цвет игнорируется
			    gi.width  = (uint16)image->width;
			    gi.height = (uint16)image->height;
                gi.pitch = (uint16)image->pitch;
			    gi.pixels = image->pixels;
			    gi.pos = svec2(0, (int16)(-image->height+image_offset_Y));
			    break;
            case RECTANGLE:
                gi.pixels = (uint8 *)1; // хинтовый указатель - 1 - значит прямоугольник
                gi.width = (uint16)advance;
                gi.height = shadow;
                gi.pitch = ch;
                gi.pos = svec2(0, (int16)(-shadow+image_offset_Y));
                break;
		    }

            gi.charindex = charindex;
		    //underline
            gi.length = (uint16)(advance + add_underline_len);
            gi.start_pos = svec2(0, (int16)underline_offset) - gi.pos;
            gi.thickness = font->ulineThickness * underlined;

            gi.pos.x += (int16)pos.x;
            gi.pos.y += (int16)pos.y;
	    }

	    void add_glyph_image(GLYPHS &outlined_glyphs, GLYPHS *glyphs, const ivec2 &pos, int add_underline_len = 0) const
	    {
		    for (int i = 1, n = ui_scale(shadow); i <= n; i++)//можно было бы идти в обратную сторону (i--), но это не повлияет на результат, т.к. lerp(lerp(S, D, A1), D, A2) = lerp(lerp(S, D, A2), D, A1)
			    export_to_glyph_image(glyphs, pos + ui_scale(ivec2(i)), add_underline_len, shadow_color);

		    export_to_glyph_image(glyphs, pos, add_underline_len, color);

		    if (type == CHAR && outline_color) glyph->get_outlined_glyph(outlined_glyphs.add(), font, pos, outline_color);//обведенные символы добавляются в отдельный массив, т.к. обводка должна рисоваться раньше всех остальных символов, чтобы не затирать их
	    }
    };
}

void glyph_s::get_outlined_glyph(glyph_image_s &gi, font_c *font, const ivec2 &pos, TSCOLOR outline_color)
{
	float invr = tmin(tmax(font->font_params.size)*font->font_params.outline_radius, 6.f);//ограничиваем размер обводки в 6 пикселей, т.к. иначе для больших букв проявляется круговая природа distance field
	invr = tmax(invr, 1.1f);//радиус меньше пикселя не имеет смысла, к тому же единичный радиус приведет к делению на 0
	int ir = (int)invr;
    gi.charindex = -1;
	gi.color = outline_color;
	gi.width  = (uint16)(width  + 2*ir);
	gi.height = (uint16)(height + 2*ir);
    gi.pitch = (uint16)gi.width;
	gi.pos = svec2((int16)left, (int16)-top);
    gi.pos.x += (int16)(pos.x - ir);
    gi.pos.y += (int16)(pos.y - ir);
	gi.thickness = 0;//для линии подчеркивания обводки пока не будет

	if (outlined == nullptr)
	{
		outlined = (uint8*)MM_ALLOC(gi.width * gi.height);
		uint8 *pixels = (uint8*)(this+1);

		float oshift = 1.f / (1.f - font->font_params.outline_shift);
		invr = oshift/(invr-1);//уменьшаем радиус на 1 для того, чтобы обводка начиналась сразу на границе символа (особенно актуально при маленьких размерах шрифта)
		irect imgRect(ivec2(0), ivec2(width, height)-1);
		ivec2 p;
		for (p.y=0; p.y<gi.height; p.y++)//формируем изображение обводки символа
		for (p.x=0; p.x<gi.width ; p.x++)
		{
			float nearest = 10000;
			irect r = irect(p - 2*ir, p).intersect(imgRect);
			vec2 c(p - ir);
			for (int j=r.lt.y; j<=r.rb.y; j++)
			for (int i=r.lt.x; i<=r.rb.x; i++)
				if (pixels[i+width*j] > 0) nearest = tmin(nearest, (vec2((float)i, (float)j) - c).len() /* + 1.f*/ - pixels[i+width*j]/255.f);//если альфа > 0, значит на расстоянии не более чем в пиксель alpha = 255
			outlined[p.x+gi.width*p.y] = (uint8)lround(CLAMP(oshift - (nearest/* - 1*/) * invr, 0.f, 1.f) * 0xff);
		}
	}
	gi.pixels = outlined;
}

namespace
{
struct text_parser_s
{
	//void operator=(const text_parser_s &) UNUSED;

	const wstr_c *text;
	GLYPHS *glyphs;
    GLYPHS outlined_glyphs;
    int max_line_length;
	int flags, boundy;

	struct paragraph_s
	{
        enum a{ALEFT, ARIGHT, AJUSTIFY, ACENTER};

		DUMMY(paragraph_s);
		paragraph_s():indention(0), line_spacing(0), align(ALEFT), rite(false) {}
		signed indention : 16; //абзацный отступ
		signed line_spacing : 16; //добавочный межстрочный интервал, в пикселях
		unsigned align : 8;
        unsigned rite : 1;
	};
    static_assert( sizeof(paragraph_s) == 8, "!" );

    tbuf_t<ivec2> addhs;
	tbuf_t<TSCOLOR> colors_stack;
	tbuf_t<font_c*> fonts_stack;
	tbuf_t<paragraph_s> paragraphs_stack;
	struct shadow_pars_s
	{
		DUMMY(shadow_pars_s);
		shadow_pars_s() {}
		int len; TSCOLOR color;
	};
	tbuf_t<shadow_pars_s> shadow_stack;
	tbuf_t<TSCOLOR> outline_stack;
	tbuf_t<int> underline_stack;
	int hyphenation_tag_nesting_level;
	int line_len, last_line_descender;
	ivec2 pen;
	tbuf_t<meta_glyph_s> last_line; //метаглифы последней строки
	int cur_max_line_len;
    int prev_line_dim_glyph_index;
	struct side_text_limit //обтекание, поддерживается только по одной картинке с каждой стороны
	{
		int width, bottom;
	} leftSL, rightSL; // границы для обтекания текста слева и справа (используется для imgl и imgr)
	int maxW; // максимальная ширина строки (возвращаемое значение)

    int rite_rite = 0; // индекс в массиве глифов для блока в <r></r> - выровнять этот кусок вправо
    int addhtags = 0; // количество тэгов, увеличивающих высоту строки

    bool first_char_in_paragraph, was_inword_break;

    text_parser_s() {}

	void setup(const wstr_c &text_, int max_line_length_, GLYPHS *glyphs_, TSCOLOR default_color_, font_c *default_font, uint32 flags_, int boundy_)
	{
        text = &text_;
        max_line_length = max_line_length_;
        glyphs = glyphs_;
        flags = flags_;
        boundy = boundy_;

        addhs.clear();
        outlined_glyphs.clear();
        colors_stack.clear();
        fonts_stack.clear();
        paragraphs_stack.clear();
        shadow_stack.clear();
        outline_stack.clear();
        underline_stack.clear();
        last_line.clear();


		leftSL.bottom = rightSL.bottom = 0;

		underline_stack.clear();
		hyphenation_tag_nesting_level = 0;

		colors_stack.add(default_color_);
		fonts_stack .add(default_font);
		paragraphs_stack.add(paragraph_s());
        if (flags & TO_HCENTER) paragraphs_stack.last().align = paragraph_s::ACENTER;
		outline_stack.add(0);

		if (glyphs)
        {
            glyphs->reserve( text->get_length() );
            glyph_image_s &gi = glyphs->add();
            gi.pixels = nullptr;
            gi.outline_index = 0;
        }

		line_len = 0;//текущая длина строки в пикселях (используется для переноса слов)
		//int H = 0;//высота строки (инициализируется нулем, т.к. непонятно чем её инициализировать - в начале строки может идти тег изменения шрифта)
		pen = ivec2(0,0);//положение "пера", реально оно выше на величину H, но H не известно пока не сформировалась строка целиком
        prev_line_dim_glyph_index = -1;
		cur_max_line_len = max_line_length;
		first_char_in_paragraph = true;
		was_inword_break = false;
		last_line_descender = maxW = 0;

        rite_rite = 0;
        addhtags = 0;
	}

	void add_indent(int chari)//добавляет абзацный отступ
	{
		paragraph_s &paragraph = paragraphs_stack.last();
		if (paragraph.indention > 0)
		{
			meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR, chari);//тип именно CHAR, а не SPACE, т.к. иначе при выравнивании по шинине все SPACE-ы расширятся, включая и абзацный отступ
			mg.underlined = 0;//это спец. признак indent-а, за счет которого под ним не будет рисоваться линия подчеркивания
			mg.glyph = &(*fonts_stack.last())[L' '];
			mg.advance = paragraph.indention;
			line_len += mg.advance;
		}
	}

	void next_line(int H = INT_MAX) //перемещает "перо" на следующую строку с учетом обтекания текста
	{
		if (H != INT_MAX) pen.y += H + paragraphs_stack.last().line_spacing + fonts_stack.last()->font_params.additional_line_spacing;
		cur_max_line_len = max_line_length;
		if (pen.y <  leftSL.bottom) cur_max_line_len -= (pen.x = leftSL.width); else pen.x = 0;
		if (pen.y < rightSL.bottom) cur_max_line_len -= rightSL.width;
		line_len = 0;
	}

	void end_line(bool nextLn = true, bool acceptaddhnow = false) //прерывает текущую строку
	{
		//Считаем высоту строки
		int H, spaces, W, WR;
		pen.y += calc_HSW(H, spaces, W, WR, last_line.count());

		//Добавляем символы в массив glyphs с учетом выравнивания (только по правому краю, выравнивание по ширине при переходе на новую строку игнорируется)
		if (glyphs)
		{
			ivec2 offset(0, H);//смещение, добавляемое к каждому символу перед помещением в массив

			if (paragraphs_stack.last().align == paragraph_s::ARIGHT) offset.x = cur_max_line_len - W - WR;
			else if (paragraphs_stack.last().align == paragraph_s::ACENTER) offset.x = (cur_max_line_len - W - WR)/2;

            if (prev_line_dim_glyph_index >= 0) glyphs->get(prev_line_dim_glyph_index).next_dim_glyph = glyphs->count();
            prev_line_dim_glyph_index = glyphs->count();
            glyph_image_s &gi = glyphs->add();
            gi.pixels = nullptr;
            gi.outline_index = -1;
            gi.next_dim_glyph = -1;
            gi.line_lt.x = (int16) (offset.x + pen.x);
            gi.line_lt.y = (int16) (offset.y + pen.y - H);
            gi.line_rb.x = (int16) (gi.line_lt.x + W);
            gi.line_rb.y = (int16) (gi.line_lt.y + H);

            int oldpenx = pen.x;
            for (int i = rite_rite, cnt = last_line.count(); i < cnt; ++i)
            {
                const meta_glyph_s &mg = last_line.get(i);
                mg.add_glyph_image(outlined_glyphs, glyphs, pen + offset);
                if (mg.image_offset_Y > 0)
                    for (ivec2 &x : addhs)
                        if (x.x == i) { x.x = pen.x + offset.x; break; }
                pen.x += mg.advance;
            }
            if (rite_rite)
            {
                pen.x = oldpenx;
                offset.x = cur_max_line_len - WR;
                for (int i = 0; i < rite_rite; ++i)
                {
                    meta_glyph_s &mg = last_line.get(i);
                    mg.charindex = -1;
                    mg.add_glyph_image(outlined_glyphs, glyphs, pen + offset);
                    if (mg.image_offset_Y > 0)
                        for (ivec2 &x : addhs)
                            if (x.x == i) { x.x = pen.x + offset.x; break; }
                    pen.x += mg.advance;
                }
                rite_rite = 0;
            }

		}
        if (acceptaddhnow)
        {
            int addH = 0;
            for (const ivec2 &x : addhs)
                if (x.y > addH) addH = x.y;
            H += addH;
        }
		next_line(nextLn && H != 0 ? H : INT_MAX);
		last_line.clear();
		//addIndent();
		first_char_in_paragraph = true;
	}

	int calc_HSW(int &H, int &spaces, int &W, int &WR, int lineSize)
	{
        ivec2 *curaddhs = addhtags ? (ivec2 *)_alloca(addhtags * sizeof(ivec2)) : nullptr;
        int curaddhs_n = 0;

		H = 0; spaces = 0; W = 0; WR = 0;
		if (lineSize == 0) return 0;//чтобы не портить значение last_line_descender из-за последней пустой строки (актуально для singleLine)
		last_line_descender = 0;
        int addH = 0;
		for (int j = 0; j < lineSize; j++)
		{
			meta_glyph_s &mg = last_line.get(j);
			H = tmax(H, pen.y ? mg.font->height : mg.font->ascender);//1. считаем max, т.к. в строке может меняться шрифт; 2. берется не высота символа, а именно ascender, чтобы высота строки не могла быть меньше этого значения, иначе строка из прописных символов нарисуется выше чем надо
			last_line_descender = tmin(last_line_descender, mg.calch(), mg.font->ulinePos-(int)lceil(mg.font->ulineThickness*.5f));
			if		(mg.type == meta_glyph_s::SPACE) spaces++;
            else if (mg.type == meta_glyph_s::IMAGE) // для картинок дополнительно выполняется проверка на высоту картинки, чтобы высокие картинки расширяли строку
            {
                H = tmax(H, mg.image->height - mg.image_offset_Y);
                if (mg.image_offset_Y > addH)
                {
                    addH = mg.image_offset_Y;
                    ASSERT(curaddhs && curaddhs_n < addhtags);
                    curaddhs[curaddhs_n++] = ivec2(j,addH);
                }

            } else if (mg.type == meta_glyph_s::RECTANGLE) // для прямоугольников дополнительно выполняется проверка на высоту, чтобы высокие прямоугольники расширяли строку
            {
                H = tmax(H, mg.shadow - mg.image_offset_Y);
                if (mg.image_offset_Y > addH)
                {
                    addH = mg.image_offset_Y;
                    ASSERT(curaddhs && curaddhs_n < addhtags);
                    curaddhs[curaddhs_n++] = ivec2(j, addH);
                }
            }
            if (j<rite_rite)
                WR += mg.advance;
            else
			    W += mg.advance; //длина строки (нужно для выравнивания)
		}
		if ((W+WR) > maxW) maxW = (W+WR);

        addH = 0;
        for( const ivec2 & x : addhs )
            if (W >= x.x && x.y > addH)
                addH = x.y;

        addhs.clear();
        if (curaddhs_n)
        {
            addhs.set_count(curaddhs_n, false);
            memcpy(addhs.data(), curaddhs, sizeof(ivec2) * curaddhs_n);
        }

        return addH;
	}

	meta_glyph_s &add_meta_glyph(meta_glyph_s::mgtype_e type, int chari = -1)
	{
		if (first_char_in_paragraph) first_char_in_paragraph = false, add_indent(chari);//если это первый символ строки, добавляем отступ
		meta_glyph_s &mg = last_line.add();//добавляем метаглиф
        mg.image_offset_Y = 0;
        mg.charindex = chari;
		mg.type = type;
		mg.font = fonts_stack.last();
		mg.color = colors_stack.last();
		if (underline_stack.count() == 0)
			mg.underlined = -1, mg.underline_offset = 0;
		else mg.underlined = 1, mg.underline_offset = (short)underline_stack.last();
		if (shadow_stack.count() == 0) mg.shadow = 0;
		else
		{
			mg.shadow = (short)shadow_stack.last().len;
			mg.shadow_color = shadow_stack.last().color;
		}
		mg.outline_color = outline_stack.last();
		mg.ch = 0;

		return mg;
	}

	bool process_tag(const wsptr &tag_, const wsptr &tagbody_, int chari)
	{
        pwstr_c tag(tag_);
        pwstr_c tagbody(tagbody_);

		font_c &font = *fonts_stack.last();

		if (tag == CONSTWSTR("br"))
		{
			if (last_line.count() > 0) end_line();
			else next_line(font.height);//если строка пустая, то добавляем высоту строки для текущего шрифта
			pen.y += ui_scale(tagbody.as_int());
		}
        else if (tag == CONSTWSTR("char"))
        {
            addchar(font, (wchar)tagbody.as_uint('?'), chari);
        }
        else if (tag == CONSTWSTR("color"))
		{
            colors_stack.add( parsecolor(tagbody.as_sptr(), ARGB(0,0,0)) );
		}
		else if (tag == CONSTWSTR("/color"))
		{
			if (CHECK(colors_stack.count() > 1)) colors_stack.pop();
		}
		else if (tag == CONSTWSTR("s"))
		{
			underline_stack.add(ui_scale(tagbody.as_int())-font.height/2-font.descender);
		}
		else if (tag == CONSTWSTR("u"))
		{
			underline_stack.add(ui_scale(tagbody.as_int())-font.ulinePos);
		}
		else if (tag == CONSTWSTR("/u") || tag == CONSTWSTR("/s"))
		{
			if (CHECK(underline_stack.count()>0)) underline_stack.pop();
		}
		else if (tag == CONSTWSTR("shadow"))
		{
			shadow_pars_s &sp = shadow_stack.add();
            token<wchar> t( tagbody, L';' );
			sp.len = t ? t->as_int() : 2;
			++t;
			sp.color = t ? parsecolor(t->as_sptr(), ARGB(0,0,0)) : ARGB(0,0,0);
		}
		else if (tag == CONSTWSTR("/shadow"))
		{
			if (CHECK(shadow_stack.count() > 0)) shadow_stack.pop();
		}
		else if (tag == CONSTWSTR("outline"))
		{
            outline_stack.add(parsecolor(tagbody.as_sptr(), ARGB(0,0,0)));
		}
		else if (tag == CONSTWSTR("/outline"))
		{
			if (CHECK(outline_stack.count() > 1)) outline_stack.pop();
		}
		else if (tag == CONSTWSTR("p") || tag == CONSTWSTR("pn"))
		{
			if (last_line.count() > 0 && tag == CONSTWSTR("p")) end_line();
			else ;//если строка пустая, то перо не смещаем вниз, в этом отличие тега <p> от <br>, ведь часто <p> стоит в начале строки и новая строка в этом случае не нужна

            ASSERT(rite_rite == 0);

            token<wchar> t( tagbody, L',' ); //align,indent,lineSpacing
			paragraph_s pargph = paragraphs_stack.last(); pargph.rite = false;
			if (!t->is_empty())
			{
				if (t->get_char(0) == L'l') pargph.align = paragraph_s::ALEFT;//проверяем только первый символ, чтоб можно было писать сокращенно
				else if (t->get_char(0) == L'r') pargph.align = paragraph_s::ARIGHT;
				else if (t->get_char(0) == L'j') pargph.align = paragraph_s::AJUSTIFY;
				else if (t->get_char(0) == L'c') pargph.align = paragraph_s::ACENTER;
			}
            ++t;
			if (t)
			{
				if (!t->is_empty()) pargph.indention = ui_scale(t->as_int());
                ++t;
				if (t && !t->is_empty()) pargph.line_spacing = ui_scale(t->as_int());
			}
			paragraphs_stack.add(pargph);
			//addIndent();
		}
		else if (tag == CONSTWSTR("pr"))
		{
			if (last_line.count() > 0) end_line(false);
			else ;//если строка пустая, то перо не смещаем вниз, в этом отличие тега <p> от <br>, ведь часто <p> стоит в начале строки и новая строка в этом случае не нужна

            ASSERT(rite_rite == 0);

			paragraph_s pargph = paragraphs_stack.last();
			pargph.align = paragraph_s::ARIGHT;
			paragraphs_stack.add(pargph);
		}
		else if (tag == CONSTWSTR("/p"))
		{
			if (last_line.count() > 0) end_line(false);
			if (CHECK(paragraphs_stack.count() > 1)) paragraphs_stack.pop();
		}
		else if (tag == CONSTWSTR("nbsp") || tag == CONSTWSTR("space"))
		{
			meta_glyph_s &mg = add_meta_glyph(tag.get_char(0) == 's' ? meta_glyph_s::SPACE : meta_glyph_s::CHAR, chari); // метаглиф пробела
			mg.glyph = &font[L' '];
			mg.advance = tagbody.is_empty() ? mg.glyph->advance : (tagbody.get_last_char() == L'%' ? tagbody.as_int() * mg.glyph->advance / 100 : ui_scale(tagbody.as_int()));
			line_len += mg.advance;
		}
		else if (tag == CONSTWSTR("tab"))
		{
			int pos = ui_scale(tagbody.as_int());
			if (pos > line_len)
			{
				meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR, chari);
				mg.glyph = &font[L' '];
				mg.advance = pos - line_len;
				line_len += mg.advance;
			}
		}
		else if (tag == CONSTWSTR("b"))
		{
            font_c &f = *fonts_stack.last();
			fonts_stack.add(font_desc_c(f.makename_bold()));
		}
        else if (tag == CONSTWSTR("l"))
        {
            font_c &f = *fonts_stack.last();
            fonts_stack.add(font_desc_c(f.makename_light()));
        }
        else if (tag == CONSTWSTR("i"))
        {
            font_c &f = *fonts_stack.last();
            fonts_stack.add(font_desc_c(f.makename_italic()));
        }
        else if (tag == CONSTWSTR("font"))
        {
            fonts_stack.add(font_desc_c(tmp_str_c(tagbody)));
        }
		else if (tag == CONSTWSTR("/font") || tag == CONSTWSTR("/b") || tag == CONSTWSTR("/l") || tag == CONSTWSTR("/i"))
		{
			if (CHECK(fonts_stack.count() > 1)) fonts_stack.pop();
		}
		else if (tag == CONSTWSTR("hr"))
		{
			if (last_line.count() > 0) end_line();

            token<wchar> t( tagbody, L',' ); //vertindent[,sideindent,[thickness]]
			
			int vertindent = ui_scale(t && !t->is_empty() ? t->as_int() : 7);
            ++t; int sideindent = ui_scale(t && !t->is_empty() ? t->as_int() : 3);

			if (glyphs)
			{
				glyph_image_s &gi = glyphs->add();
                gi.pixels = (uint8 *)16; // nullptr or value less 16 means special glyph, skipped while draw. 16 value let this glyph drawable
				gi.color = colors_stack.last();
				gi.width = gi.height = 0; // zeros - no one lookup to gi.pixels 
				gi.pos.x = (int16)(pen.x + sideindent);
                gi.pos.y = (int16)(pen.y + vertindent);
				++t; gi.thickness = ui_scale(t && !t->is_empty() ? t->as_float() : 1.75f);
				gi.length    = (uint16)(cur_max_line_len - 2*sideindent);
				gi.start_pos  = svec2(0);
			}

			pen.y += 2 * vertindent;
		}
		else if (tag == CONSTWSTR("img"))
		{
			meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::IMAGE, chari);
            token<wchar> t( tagbody, L',' ); //fileName[,offsetY]
			mg.image = scaled_image_s::load(t->as_sptr(), ivec2(ui_scale(100)));
			++t; if (t)
            {
                mg.image_offset_Y = t->as_int();
                if (mg.image_offset_Y < 0)
                    mg.image_offset_Y += (mg.image->height - mg.font->height)/2 + 1;
                if (mg.image_offset_Y > 0)
                    ++addhtags;
            }
			mg.advance = mg.image->width;
			line_len += mg.advance;
		}
		else if (tag == CONSTWSTR("imgl") || tag == CONSTWSTR("imgr"))
		{
			side_text_limit &sl = tag == CONSTWSTR("imgl") ? leftSL : rightSL;
			if (last_line.count() > 0) end_line();//переход на новую строку, если нужно
			if (pen.y < sl.bottom) pen.y = sl.bottom;//если сейчас текст уже обтекается картинкой, то перемещаем перо на конец картинки, т.к. область обтекания более чем из одной картинки с одной стороны не поддерживается
			// Теперь добавляем картинку с нужной стороны
			scaled_image_s *si = scaled_image_s::load(tagbody, ivec2(ui_scale(100)));
			if (glyphs)
			{
				glyph_image_s &gi = glyphs->add();
				gi.color = 0;
				gi.width  = (uint16)si->width;
				gi.height = (uint16)si->height;
                gi.pitch = (uint16)si->pitch;
				gi.pixels = si->pixels;
				gi.thickness = 0;
				gi.pos.x = (int16)(&sl == &leftSL ? 0 : max_line_length - si->width);
                gi.pos.y = (int16)pen.y;
			}
			sl.width  = si->width;
			sl.bottom = si->height + pen.y;
			next_line();
		}
        else if (tag == CONSTWSTR("r"))
        {
            ASSERT( last_line.count() == 0 );
            paragraphs_stack.last().rite = true;
        }
        else if (tag == CONSTWSTR("/r"))
        {
            ASSERT( paragraphs_stack.last().rite );
            rite_rite = last_line.count();
            if (rite_rite == 0)
                paragraphs_stack.last().rite = false;
        }
        else if (tag == CONSTWSTR("hyphenation"))
        {
            hyphenation_tag_nesting_level++;
        }
        else if (tag == CONSTWSTR("/hyphenation"))
        {
            if (CHECK(hyphenation_tag_nesting_level > 0)) hyphenation_tag_nesting_level--;
        }
        else if (tag == CONSTWSTR("rect"))
        {
            meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::RECTANGLE, chari);
            token<wchar> t(tagbody, L',');
            mg.ch = (wchar)t->as_int();
            ++t; mg.advance = t ? t->as_int() : 0;
            ++t; mg.shadow = t ? (short)t->as_int() : 0;
            ++t; if (t)
            {
                mg.image_offset_Y = t->as_int();
            }
            if (mg.shadow < 0)
            {
                mg.shadow = -mg.shadow;
                mg.image_offset_Y += (mg.shadow - mg.font->height) / 2;
            }
            if (mg.image_offset_Y > 0)
                ++addhtags;
            mg.image = nullptr;
            line_len += mg.advance;
        } else if (tag == CONSTWSTR("."))//это служебный тег разделитель для удобства локализации - скрываем его
			;
		else
			return false;

		return true;
	}

    void addchar(font_c &font, wchar ch, int chari)
    {
        meta_glyph_s &mg = add_meta_glyph(ch == L' ' ? meta_glyph_s::SPACE : meta_glyph_s::CHAR, chari);
        mg.glyph = &font[mg.ch = ch];
        mg.advance = mg.glyph->advance;
        line_len += mg.advance;
        //Kerning processing
        meta_glyph_s *prev;
        int cnt = last_line.count();
        if (cnt > 1 && (prev = &last_line.get(cnt - 2))->type == meta_glyph_s::CHAR && (cnt-1) != rite_rite) //если перед этим символом стоит другой символ
        {
            int k = font.kerning_ci(prev->glyph->char_index, mg.glyph->char_index);
            prev->advance += k;//корректируем ширину пред. символа; по хорошему нужно сдвигать "перо", но так проще и не нужно вводить новые переменные
            line_len += k;
        }
    }

	ivec2 parse(int curTextI = 0)
	{

		for (; curTextI<text->get_length(); curTextI++)
		{
			font_c &font = *fonts_stack.last();
			paragraph_s &paragraph = paragraphs_stack.last();
			wchar ch = text->get_char(curTextI);

			if (ch == L'\n')
			{
				if (last_line.count() > 0) end_line();
				else next_line(font.height);//если строка пустая, то добавляем высоту строки для текущего шрифта
			} else if (ch != L'<')//это простой символ
			{
            add_char:
                addchar(font, ch, curTextI);
			} else if (curTextI < text->get_length()-1 && text->get_char(curTextI+1) == '|')//это экранированный символ '<'
			{
				curTextI++;//skip |
				goto add_char;
			} else//это тэг
			{
				int i = text->find_pos(curTextI + 1, L'>');
				if (i == -1) {WARNING("Tag '%s' not closed", to_str(text->cstr()+curTextI).cstr()); break;}

                token<wchar> t(text->substr(curTextI+1, i), L'=');
                wsptr tag(*t);
                ++t; wsptr tagbody; if (t) tagbody = *t;
				if (!process_tag(tag, tagbody, curTextI))
					goto add_char;//если не смогли раскрыть тег, оставляем его нераскрытым в тексте (чтобы хотя бы было видно баг)
				curTextI = i;
			}

			//Перенос слов
			if (line_len > cur_max_line_len && last_line.count() > 1)//обязательно проверяем - вдруг это первый символ строки, т.к. ситуация когда символ не помещается во всей строке не может быть корректно обработана
			{
				int j = last_line.count() - 1, lineSize;

				if (flags & TO_MULTILINE || (flags & TO_END_ELLIPSIS && pen.y + fonts_stack.last()->height * 3 / 2 >= boundy))
				{
					glyph_s &dot_glyph = (*fonts_stack.last())[L'.'];
					line_len += dot_glyph.advance * 3;//advance of ...

					for (; j > 0; j--) if ((line_len -= last_line.get(j).advance) <= cur_max_line_len) break;
					last_line.set_count(j);

					//Add ...
					for (int i=0; i<3; i++)
					{
						meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR, curTextI);//добавляем метаглиф
						mg.glyph = &dot_glyph;
						mg.advance = mg.glyph->advance;
					}

					break;//early exit
				}
				else
				{
					if (flags & TO_WRAP_BREAK_WORD) goto inlineBreak;

					//Сразу формировать новую строку нельзя - если окажется, что это единственное слово в строке, то его надо не переносить, а разбивать
					//Поиск последнего разделителя (только пробел)
					//Если пробел стоит в начале строки, а сама строка состоит из одного слова, то слово всё равно переносится на другую строку - это нормально
					while (j >= rite_rite && last_line.get(j).type != meta_glyph_s::SPACE) j--;

					char charclass[30];
					int start = j+1, n = last_line.count() - start;
					if (hyphenation_tag_nesting_level && n < LENGTH(charclass)-3)//нужно смотреть на 3 буквы вперёд, чтобы работали правила gss-ssg и gs-ssg
					{
						//Перенос слов по слогам реализован на основе упрощённого алгоритма П.Христова http://sites.google.com/site/foliantapp/project-updates/hyphenation
						int nn = tmin(n+3, text->get_length() - (curTextI-n+1)), i = 0;

						while (i<n && wcschr(L"(\"«", last_line.get(i+start).ch))//пропускаем допустимые символы перед началом слова
							charclass[i++] = 0;

						for (; i<nn; i++)
						{
							wchar_t ch;
							if (i < n)
							{
								meta_glyph_s &mg = last_line.get(i+start);
								if (mg.type != meta_glyph_s::CHAR) goto skipHyphenate;
								ch = mg.ch;
							}
							else
							{
								ch = text->get_char(curTextI+i-n+1);
							}
							if (wcschr(L"аеёиоуыэюяaeiouyАЕЁИОУЫЭЮЯAEIOUY", ch))
								charclass[i] = 'g';
							else if (wcschr(L"бвгджзклмнпрстфхцчшщbcdfghjklmnpqrstvwxzБВГДЖЗКЛМНПРСТФХЦЧШЩBCDFGHJKLMNPQRSTVWXZ", ch))
								charclass[i] = 's';
							else if (wcschr(L"йьъЙЬЪ", ch))
								charclass[i] = 'x';
							else if (ch == '-')
								charclass[i] = '-';
							else if (wcschr(L".,;:!?)\"»", ch))//допустимые символы после конца слова
								{nn = i; break;}
							else if (i < n)
								goto skipHyphenate;//нашли хоть один посторонний символ - значит это не простое слово и лучше его не переносить
							else {nn = i; break;}//неизвестно, что это (возможно тег или знак препинания), поэтому просто останавливаемся на этом символе
						}
						for (int i=0; i<nn-2; i++)//[x, l+l]
							if (charclass[i] == 'x')
								charclass[i] = '-';//вставляем - перед началом след. слога, а не после, чтобы этот алгоритм был эквивалентен вставке '-' между слогами, иначе в простейших случаях напр. [каталог/sgsgsgs] будет только один перенос вместо двух возможных
						for (int i=0; i<nn-5; i++)//[g+s+s, s+s+g]
							if ((DWORD&)(charclass[i]) == MAKEFOURCC('g','s','s','s') && (WORD&)(charclass[i+4]) == MAKEWORD('s','g'))
								charclass[i+=2] = '-';
						for (int i=0; i<nn-4; i++)//[g+s+s, s+g], [g+s, s+s+g]
							if ((DWORD&)(charclass[i]) == MAKEFOURCC('g','s','s','s') && charclass[i+4] == 'g')
								charclass[i+1] = '-', charclass[i+=2] = '-';
						for (int i=0; i<nn-3; i++)//[s+g, s+g], [g+s, s+g], [s+g, g+l]
							if ((DWORD&)(charclass[i]) == MAKEFOURCC('s','g','s','g')
							 || (DWORD&)(charclass[i]) == MAKEFOURCC('g','s','s','g')
							 || (DWORD&)(charclass[i]) == MAKEFOURCC('s','g','g','s')
							 || (DWORD&)(charclass[i]) == MAKEFOURCC('s','g','g','g'))
							 charclass[i+=1] = '-';

						int newLineLen = line_len;
						glyph_s &hyphenGlyph = (*fonts_stack.last())[L'-'];
						newLineLen += hyphenGlyph.advance;

						//Ищем последний перенос
						for (int i=n-1; i>2; i--)//не разрешаем переносить менее 3-х букв с начала и конца слова
							if ((newLineLen -= last_line.get(i+start).advance) <= cur_max_line_len && charclass[i-1] == '-' && i < nn-2)
							{
								lineSize = start + i;
								line_len = newLineLen;
								if (last_line.get(i + start - 1).ch != '-')
								{
									lineSize++;
									meta_glyph_s mg = add_meta_glyph(meta_glyph_s::CHAR, curTextI);//добавляем метаглиф
									mg.glyph = &hyphenGlyph;
									mg.advance = mg.glyph->advance;
									last_line.pop();
									last_line.insert(start + i, mg);
								}
								goto end;
							}
skipHyphenate:;
					}

					if (j >= rite_rite)//Пробел был найден - слово не единственное => формируем новую строку
					{
						last_line.remove_slow(j);//удаляем пробел (чтобы он не участвовал в подсчете пробелов ниже, а также чтобы не добавлялся соответствующий глиф - его может быть видно при подчеркивании)
						lineSize = j;//строку прерываем на символе, идущем сразу после пробела
					}
					else//Также формируем новую строку, но уже не по последнему пробелу, а по части слова, вмещающегося в строку
					{
inlineBreak:
						lineSize = last_line.count()-1;//длину строки ограничиваем до текущего символа (невключительно)
						was_inword_break = true;
					}
end:;
				}

				//Считаем высоту строки, а также кол-во пробелов
				int H, spaces, W, WR;
                pen.y += calc_HSW(H, spaces, W, WR, lineSize);

				//Добавляем символы в массив glyphs с учетом выравнивания
				if (glyphs)
				{
					ivec2 offset(0, H);//смещение, добавляемое к каждому символу перед помещением в массив

                    int lineW = W;
					if (paragraph.align == paragraph_s::ARIGHT) offset.x = cur_max_line_len - W - WR;
                    else if (paragraph.align == paragraph_s::AJUSTIFY) { W = cur_max_line_len - W - WR; lineW = cur_max_line_len - WR; }//подгоняем символы (т.о. добавляться они будут уже в корректированном положении)
					else if (paragraph.align == paragraph_s::ACENTER) offset.x = (cur_max_line_len - W - WR)/2;

                    if (prev_line_dim_glyph_index >= 0) glyphs->get(prev_line_dim_glyph_index).next_dim_glyph = glyphs->count();
                    prev_line_dim_glyph_index = glyphs->count();
                    glyph_image_s &gi = glyphs->add(); // служебный глиф с размерами строки

                    gi.pixels = nullptr;
                    gi.outline_index = -1;
                    gi.next_dim_glyph = -1;
                    gi.line_lt.x = (int16)(offset.x + pen.x);
                    gi.line_lt.y = (int16)(offset.y + pen.y - H);
                    gi.line_rb.x = (int16)(gi.line_lt.x + lineW);
                    gi.line_rb.y = (int16)(gi.line_lt.y + H);

					int spaceIndex = 0, offX = 0;//эти две переменные нужны только для AJUSTIFY
                    int oldpenx = pen.x;
					for (j = rite_rite; j < lineSize; j++)
					{
                        const meta_glyph_s &mg = last_line.get(j);
						int add_underline_len = 0;
						ivec2 pos = pen + offset;
						pen.x += mg.advance;

						if (paragraph.align == paragraph_s::AJUSTIFY)
						{
							pos.x += offX;//за счет смещения offX получается, что pen.x фактически не указывает на положение тек. символа, а отстает
							if (last_line.get(j).type == meta_glyph_s::SPACE)
							{
								spaceIndex++;
								int prevOffX = offX;
								offX = W * spaceIndex / spaces;//данная схема исключает накопляемую ошибку, а также усредняет промежутки между словами, т.е. будет не 2,2,3,3, а 2,3,2,3
								add_underline_len = offX - prevOffX;
							}
						}
						mg.add_glyph_image(outlined_glyphs, glyphs, pos, add_underline_len);
                        if (mg.image_offset_Y > 0)
                            for( ivec2 &x : addhs )
                                if (x.x == j) { x.x = pos.x; break; }
					}
                    if (rite_rite)
                    {
                        pen.x = oldpenx;
                        offset.x = cur_max_line_len - WR;
                        for (int i = 0; i < rite_rite; ++i)
                        {
                            meta_glyph_s &mg = last_line.get(i);
                            mg.charindex = -1;
                            mg.add_glyph_image(outlined_glyphs, glyphs, pen + offset);
                            if (mg.image_offset_Y > 0)
                                for (ivec2 &x : addhs)
                                    if (x.x == i) { x.x = pen.x + offset.x; break; }
                            pen.x += mg.advance;
                        }
                        rite_rite = 0;
                    }

				}

				// go next line
				last_line.remove_slow(0, lineSize);
				next_line(H);
				for (j=0; j<last_line.count(); j++) line_len += last_line.get(j).advance;
			}
		}
		end_line(true, 0 != (TO_LASTLINEADDH & flags)); // last line

		if (/*glyphs && */outlined_glyphs.count() > 0)
		{
            if (ASSERT(glyphs->get(0).pixels == nullptr))
            {
                glyphs->get(0).outline_index = glyphs->count();
                glyphs->append_buf(outlined_glyphs);
            }
		}

		return ivec2(maxW, tmax(pen.y - last_line_descender, leftSL.bottom, rightSL.bottom));
	}
};
}


ivec2 parse_text(const wstr_c &text, int max_line_length, GLYPHS *glyphs, TSCOLOR default_color, font_c *default_font, uint32 flags, int boundy)
{
	if (!ASSERT(default_font)) return ivec2(0);
	static text_parser_s text_parser;
    text_parser.setup(text, tabs(max_line_length), glyphs, default_color, default_font, flags, boundy);
	ivec2 r = text_parser.parse();
	if (max_line_length < 0 && text_parser.was_inword_break) r.x = -r.x;
	return r;
}


irect glyphs_bound_rect(const GLYPHS &glyphs)
{
	irect res(0);

    for (const glyph_image_s &gi : glyphs)
	{
        if (gi.pixels == nullptr || gi.charindex < 0)
            continue;

		res.combine( irect(gi.pos, gi.pos+svec2(gi.width, gi.height)) );

		if (gi.thickness > 0)
		{
			ivec2 start( gi.pos + gi.start_pos );
			float thick = (tmax(gi.thickness, 1.f) - 1)*.5f;
			int d = lceil(thick);
			start.y -= d;
			res.combine( irect(start, start + ivec2(gi.length, d*2 + 1)) );
		}
	}

	return res;
}

int glyphs_nearest_glyph(const GLYPHS &glyphs, const ivec2 &p)
{
    int cnt = glyphs.count();
    for (int i=0;i<cnt;)
    {
        const glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels == nullptr)
        {
            if (gi.outline_index >= 0)
            {
                if (gi.outline_index > 0) cnt = gi.outline_index;
                ++i;
                continue;
            }

            if ( p.y <= gi.line_rb.y || gi.next_dim_glyph < 0 )
            {
                if (gi.next_dim_glyph > 0)
                    cnt = gi.next_dim_glyph;

                int mind = maximum<int>::value;
                int index = -1;

                for(int j=i+1; j<cnt; ++j)
                {
                    const glyph_image_s &gic = glyphs.get(j);
                    if (gic.charindex < 0) continue;
                    int d = tabs(gic.pos.x + gic.width / 2 - p.x);
                    if (d < mind)
                    {
                        index = j;
                        mind = d;
                    }
                }
                return index;
            }

            i = gi.next_dim_glyph;
            if (i < 0) break;
            continue;
        }

        FORBIDDEN();
    }
    return -1;
}

int glyphs_get_charindex(const GLYPHS &glyphs, int glyphindex)
{
    int glyphc = glyphs.count();
    if (glyphc && glyphs.get(0).pixels == nullptr && glyphs.get(0).outline_index > 0)
        glyphc =glyphs.get(0).outline_index;
    if (glyphindex < 0) glyphindex = 0;
    if (glyphindex >= glyphc) { glyphindex = glyphc; goto process_last_glyph; }
    while( glyphs.get(glyphindex).pixels == nullptr || glyphs.get(glyphindex).charindex < 0 )
    {
        ++glyphindex;
        if (glyphindex >= glyphc)
        {
            process_last_glyph:
            for( --glyphindex; glyphindex>=0; --glyphindex )
                if (glyphs.get(glyphindex).pixels != nullptr && glyphs.get(glyphindex).charindex >= 0)
                    return glyphs.get(glyphindex).charindex+1;
            return -1;
        }
    }
    return glyphs.get(glyphindex).charindex;
}

} // namespace ts
