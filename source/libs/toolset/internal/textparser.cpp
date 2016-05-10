#include "toolset.h"
#include "textparser.h"
#include "fourcc.h"

//-V:glyphs:807

#pragma warning (disable:4456) // declaration of 'xxx' hides previous local declaration

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
    struct meta_glyph_s // glyph at parsing step
    {
	    TSCOLOR color;
	    font_c *font;
	    short underlined, // -1 - no underline, 1 - underline, 0 - underline disabled
		      underline_offset;
	    int advance; // horizontal metaglyph size
	    short shadow;
	    wchar_t ch;
        int charindex;
	    TSCOLOR shadow_color, outline_color, bg_color;
	    int image_offset_Y;

	    enum mgtype_e {
		    CHAR,       // normal symbol
		    SPACE,      // space
		    IMAGE,      // picture
            RECTANGLE,  // empty rectangle area with callback (position of area)
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

	    void export_to_glyph_image(GLYPHS *glyphs, const ivec2 &pos, int add_underline_len, TSCOLOR c) const
	    {
		    glyph_image_s &gi = glyphs->add();
		    switch (type)
		    {
		    case CHAR:
		    case SPACE:
			    if (c == 0) // zero value is special
				     gi.color = ARGB(255,255,255,0); // we should provide equivalent value, but not zero
			    else gi.color = c;
			    gi.width  = (uint16)glyph->width;
			    gi.height = (uint16)glyph->height;
                gi.pitch = (uint16)glyph->width; // 1 byte per pixel for glyphs
			    gi.pixels = (uint8*)(glyph+1);
			    gi.pos = svec2(glyph->left, -glyph->top);
			    break;
		    case IMAGE:
			    gi.color = 0; // zero color for images - it will be ignored
			    gi.width  = (uint16)image->width;
			    gi.height = (uint16)image->height;
                gi.pitch = (uint16)image->pitch;
			    gi.pixels = image->pixels;
			    gi.pos = svec2(0, -image->height+image_offset_Y);
			    break;
            case RECTANGLE:
                gi.pixels = GTYPE_RECTANGLE;
                gi.width = (uint16)advance;
                gi.height = shadow;
                gi.pitch = ch;
                gi.pos = svec2(0, -shadow+image_offset_Y);
                break;
		    }

            gi.charindex = charindex;
		    //underline
            gi.length = (uint16)(advance + add_underline_len);
            gi.start_pos = svec2(0, underline_offset) - gi.pos;
            gi.thickness = font->uline_thickness * underlined;

            gi.pos().x += (int16)pos.x;
            gi.pos().y += (int16)pos.y;
	    }

	    void add_glyph_image(GLYPHS &outlined_glyphs, GLYPHS *glyphs, const ivec2 &pos, int add_underline_len) const
	    {
            if ( bg_color )
            {
                if ( meta_glyph_s::CHAR == type || meta_glyph_s::SPACE == type )
                {
                    glyph_image_s &gi = outlined_glyphs.add();
                    gi.pixels = GTYPE_BGCOLOR_MIDDLE;
                    gi.color = bg_color;

                    gi.width = (uint16)glyph->advance+1;
                    gi.height = (uint16)tabs(font->ascender - font->descender);
                    gi.pitch = 0;
                    gi.pos = svec2(glyph->left + pos.x-1, pos.y-font->ascender);
                    gi.charindex = charindex;
                    gi.length = 0;
                    gi.start_pos = svec2(0, underline_offset) - gi.pos;
                    gi.thickness = 0;
                }
            }

            if (type != meta_glyph_s::RECTANGLE)
            {
                // generate shadow glyphs
                for (int i = 1, n = ui_scale(shadow); i <= n; ++i) // valid any direction due same result: lerp(lerp(S, D, A1), D, A2) == lerp(lerp(S, D, A2), D, A1)
                    export_to_glyph_image(glyphs, pos + ui_scale(ivec2(i)), add_underline_len, shadow_color);
            }

		    export_to_glyph_image(glyphs, pos, add_underline_len, color);

		    if (type == CHAR && outline_color)
                glyph->get_outlined_glyph(outlined_glyphs.add(), font, pos, outline_color); // separate glyph array for outline symbols - it will be rendered first
	    }
    };
}

void glyph_s::get_outlined_glyph(glyph_image_s &gi, font_c *font, const ivec2 &pos, TSCOLOR outline_color)
{
	float invr = tmin(tmax(font->font_params.size)*font->font_params.outline_radius, 6.f); // max outline size limited to 6 pixels to avoid distance field artefacts
	invr = tmax(invr, 1.1f); // <= 1 size is not correct for outline generatation algorithm
	int ir = (int)invr;
    gi.charindex = -1;
	gi.color = outline_color;
	gi.width  = (uint16)(width  + 2*ir);
	gi.height = (uint16)(height + 2*ir);
    gi.pitch = (uint16)gi.width;
	gi.pos = svec2(left, -top);
    gi.pos().x += (int16)(pos.x - ir);
    gi.pos().y += (int16)(pos.y - ir);
	gi.thickness = 0; // do not generate outline for underline

	if (outlined == nullptr)
	{
		outlined = (uint8*)MM_ALLOC(gi.width * gi.height);
		uint8 *pixels = (uint8*)(this+1);

		float oshift = 1.f / (1.f - font->font_params.outline_shift);
		invr = oshift/(invr-1); // decrease radius by 1 for better result (very actual for small font sizes)
		irect imgRect(ivec2(0), ivec2(width, height)-1);
		ivec2 p;
		for (p.y=0; p.y<gi.height; p.y++)
		for (p.x=0; p.x<gi.width ; p.x++)
		{
			float nearest = 10000;
			irect r = irect(p - 2*ir, p).intersect(imgRect);
			vec2 c(p - ir);
			for (int j=r.lt.y; j<=r.rb.y; j++)
			for (int i=r.lt.x; i<=r.rb.x; i++)
				if (pixels[i+width*j] > 0) nearest = tmin(nearest, (vec2((float)i, (float)j) - c).len() /* + 1.f*/ - pixels[i+width*j]/255.f); // if alpha > 0, it means at a distance of no more than one pixel, alpha = 255
			outlined[p.x+gi.width*p.y] = (uint8)ts::lround(CLAMP(oshift - (nearest/* - 1*/) * invr, 0.f, 1.f) * 0xff);
		}
	}
	gi.pixels = outlined;
}

namespace
{
struct text_parser_s
{
	//void operator=(const text_parser_s &) UNUSED;

    CUSTOM_TAG_PARSER ctp;
	const wstr_c *textp;
	GLYPHS *glyphs;
    GLYPHS outlined_glyphs;
    int max_line_length;
	int flags, boundy;

	struct paragraph_s
	{
        MOVABLE( true );
        enum a{ALEFT, ARIGHT, AJUSTIFY, ACENTER};

		DUMMY(paragraph_s);
		paragraph_s():indention(0), line_spacing(0), align(ALEFT), rite(false) {}
		signed indention : 16;
		signed line_spacing : 16; // addition line distance in pixels
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
        MOVABLE( true );
		DUMMY(shadow_pars_s);
		shadow_pars_s() {}
		int len; TSCOLOR color;
	};
	tbuf_t<shadow_pars_s> shadow_stack;
	tbuf_t<TSCOLOR> outline_stack;
    tbuf_t<TSCOLOR> marker_stack;
	tbuf_t<int> underline_stack;
	int hyphenation_tag_nesting_level;
	int line_width, last_line_descender;
	ivec2 pen;
	tbuf_t<meta_glyph_s> last_line; // metaglyphs of last line
	int cur_max_line_len;
    int prev_line_dim_glyph_index;
	struct side_text_limit // wrapping text, only one image per side supported
	{
		int width, bottom;
	} leftSL, rightSL; // wrapping edges for left and right image (used for imgl and imgr)
	int maxW; // max width of line (return value)

    int rite_rite = 0; // index in glyph array for block <r></r> - align to right
    int addhtags = 0; // number of tags increasing height of line
    int full_height_line = 0;

    bool first_char_in_paragraph, was_inword_break, current_line_end_ellipsis, current_line_with_rects;
    bool search_rects;

    text_parser_s() {}

	void setup(const wstr_c &text_, int max_line_length_, CUSTOM_TAG_PARSER ctp_, GLYPHS *glyphs_, TSCOLOR default_color_, font_c *default_font, uint32 flags_, int boundy_)
	{
        ctp = ctp_;
        textp = &text_;
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
        marker_stack.clear();
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
        marker_stack.add(0);

		if (glyphs)
        {
            glyphs->reserve( textp->get_length() );
            glyph_image_s &gi = glyphs->add();
            gi.pixels = nullptr;
            gi.outline_index = 0;
        }

		line_width = 0; // current line width in pixels
		pen = ivec2(0,0); // pen position (real pen position is higher with H pixels, but H is unknown before line completely constructed)
        prev_line_dim_glyph_index = -1;
		cur_max_line_len = max_line_length;
		first_char_in_paragraph = true;
		was_inword_break = false;
		last_line_descender = maxW = 0;

        rite_rite = 0;
        addhtags = 0;
        current_line_end_ellipsis = FLAG(flags,TO_LINE_END_ELLIPSIS);
        current_line_with_rects = false;
        search_rects = false;
        full_height_line = 0;
	}

	void add_indent(aint chari)
	{
		paragraph_s &paragraph = paragraphs_stack.last();
		if (paragraph.indention > 0)
		{
			meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR, chari); // CHAR, not SPACE!
			mg.underlined = 0; // zero underline - special value to do not draw underline under indent
			mg.glyph = &(*fonts_stack.last())[L' '];
			mg.advance = paragraph.indention;
			line_width += mg.advance;
		}
	}

	void next_line(int H = INT_MAX) // moves "pen" to next line
	{
		if (H != INT_MAX) pen.y += H + paragraphs_stack.last().line_spacing + fonts_stack.last()->font_params.additional_line_spacing;
		cur_max_line_len = max_line_length;
		if (pen.y <  leftSL.bottom) cur_max_line_len -= (pen.x = leftSL.width); else pen.x = 0;
		if (pen.y < rightSL.bottom) cur_max_line_len -= rightSL.width;
		line_width = 0;
        current_line_end_ellipsis = FLAG(flags,TO_LINE_END_ELLIPSIS);
        full_height_line = 0;
	}

	void end_line(bool nextLn = true, bool acceptaddhnow = false) // break current line
	{
		int H, spaces, W, WR;
		pen.y += calc_HSW(H, spaces, W, WR, last_line.count());

		if (glyphs)
		{
			ivec2 offset(0, H); // offset for every symbol before it will be put into array

			if (paragraphs_stack.last().align == paragraph_s::ARIGHT) offset.x = cur_max_line_len - W - WR;
			else if (paragraphs_stack.last().align == paragraph_s::ACENTER) offset.x = (cur_max_line_len - W - WR)/2;

            if (prev_line_dim_glyph_index >= 0) glyphs->get(prev_line_dim_glyph_index).next_dim_glyph = (int)glyphs->count();
            prev_line_dim_glyph_index = (int)glyphs->count();
            glyph_image_s &gi = glyphs->add();
            gi.pixels = nullptr;
            gi.outline_index = -1;
            gi.next_dim_glyph = -1;
            gi.line_lt().x = (int16) (offset.x + pen.x);
            gi.line_lt().y = (int16) (offset.y + pen.y - H);
            gi.line_rb().x = (int16) (gi.line_lt().x + W);
            gi.line_rb().y = (int16) (gi.line_lt().y + H);

            int oldpenx = pen.x;
            for (int i = rite_rite, cnt = (int)last_line.count(); i < cnt; ++i)
            {
                const meta_glyph_s &mg = last_line.get(i);
                mg.add_glyph_image(outlined_glyphs, glyphs, pen + offset, 0);
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
                    mg.add_glyph_image(outlined_glyphs, glyphs, pen + offset, 0);
                    if (mg.image_offset_Y > 0)
                        for (ivec2 &x : addhs)
                            if (x.x == i) { x.x = pen.x + offset.x; break; }
                    pen.x += mg.advance;
                }
                rite_rite = 0;
            }

		} else
            rite_rite = 0;

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

	int calc_HSW(int &H, int &spaces, int &W, int &WR, aint line_size)
	{
        ivec2 *curaddhs = addhtags ? (ivec2 *)_alloca(addhtags * sizeof(ivec2)) : nullptr; //-V630
        int curaddhs_n = 0;

		H = 0; spaces = 0; W = 0; WR = 0;
		if (line_size == 0) return 0; // to do not corrupt value of last_line_descender due last empty line (single line)
		last_line_descender = full_height_line;
        full_height_line = 0;
        int addH = 0;
		for (int j = 0; j < line_size; j++)
		{
			meta_glyph_s &mg = last_line.get(j);
			H = tmax(H, pen.y ? mg.font->height : mg.font->ascender);
			last_line_descender = tmin(last_line_descender, mg.calch(), mg.font->underline_add_y-(int)lceil(mg.font->uline_thickness*.5f));
			if		(mg.type == meta_glyph_s::SPACE) spaces++;
            else if (mg.type == meta_glyph_s::IMAGE) // images can increase line height
            {
                H = tmax(H, mg.image->height - mg.image_offset_Y);
                if (mg.image_offset_Y > addH)
                {
                    addH = mg.image_offset_Y;
                    ASSERT(curaddhs && curaddhs_n < addhtags);
                    curaddhs[curaddhs_n++] = ivec2(j,addH);
                }

            } else if (mg.type == meta_glyph_s::RECTANGLE) // rectangles can increase line height
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
			    W += mg.advance; // line width (required for align)
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

	meta_glyph_s &add_meta_glyph(meta_glyph_s::mgtype_e type, aint chari = -1)
	{
		if (first_char_in_paragraph) first_char_in_paragraph = false, add_indent(chari); // if 1st symbol, add indent
		meta_glyph_s &mg = last_line.add(); // add metaglyph
        mg.image_offset_Y = 0;
        mg.charindex = (int)chari;
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
        mg.bg_color = marker_stack.last();
		mg.ch = 0;

		return mg;
	}

	bool process_tag(const wsptr &tag_, const wsptr &tagbody_, aint chari)
	{
        pwstr_c tag(tag_);
        pwstr_c tagbody(tagbody_);

		font_c &font = *fonts_stack.last();

		if (tag == CONSTWSTR("br"))
		{
            if (search_rects)
                line_ellipsis();

            if (last_line.count() > 0) end_line();
            else next_line(font.height); // if current line is empty, just add current font height
            pen.y += ui_scale(tagbody.as_int());

		} else if (tag == CONSTWSTR("nl")) // next line, but only if not first tag
        {
            if (search_rects)
                line_ellipsis();
            
            if (last_line.count() > 0)
            {
                end_line();
                pen.y += ui_scale(tagbody.as_int());
            }
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
			underline_stack.add(ui_scale(tagbody.as_int())-font.underline_add_y);
		}
		else if (tag == CONSTWSTR("/u") || tag == CONSTWSTR("/s"))
		{
			if (CHECK(underline_stack.count()>0)) underline_stack.pop();
		}
		else if (tag == CONSTWSTR("p") || tag == CONSTWSTR("pn"))
		{
            if (search_rects)
                line_ellipsis();

			if (last_line.count() > 0 && tag == CONSTWSTR("p")) end_line();
			else ; // move pen down only for non-empty lines - this is difference between  <p> and <br>

            ASSERT(rite_rite == 0);

            token<wchar> t( tagbody, L',' ); //align,indent,lineSpacing
			paragraph_s pargph = paragraphs_stack.last(); pargph.rite = false;
			if (!t->is_empty())
			{
				if (t->get_char(0) == L'l') pargph.align = paragraph_s::ALEFT;
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
            if (search_rects)
                line_ellipsis();

            if (last_line.count() > 0) end_line(false);
			else ; // do not move pen down if line empty

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
			meta_glyph_s &mg = add_meta_glyph(tag.get_char(0) == 's' ? meta_glyph_s::SPACE : meta_glyph_s::CHAR, chari);
			mg.glyph = &font[L' '];
			mg.advance = tagbody.is_empty() ? mg.glyph->advance : (tagbody.get_last_char() == L'%' ? tagbody.substr(0,tagbody.get_length()-1).as_int() * mg.glyph->advance / 100 : ui_scale(tagbody.as_int()));
			line_width += mg.advance;
		}
		else if (tag == CONSTWSTR("tab"))
		{
			int pos = ui_scale(tagbody.as_int());
			if (pos > line_width)
			{
				meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR, chari);
				mg.glyph = &font[L' '];
				mg.advance = pos - line_width;
				line_width += mg.advance;
			}
		}
		else if (tag == CONSTWSTR("b"))
		{
            font_c &f = *fonts_stack.last();
            font_desc_c ff; ff.assign(f.makename_bold());
			fonts_stack.add(ff);
		}
        else if (tag == CONSTWSTR("l"))
        {
            font_c &f = *fonts_stack.last();
            font_desc_c ff; ff.assign(f.makename_light());
            fonts_stack.add(ff);
        }
        else if (tag == CONSTWSTR("i"))
        {
            font_c &f = *fonts_stack.last();
            font_desc_c ff; ff.assign(f.makename_italic());
            fonts_stack.add(ff);
        }
        else if (tag == CONSTWSTR("font"))
        {
            font_desc_c ff; ff.assign(to_str(tagbody));
            fonts_stack.add(ff);
        }
		else if (tag == CONSTWSTR("/font") || tag == CONSTWSTR("/b") || tag == CONSTWSTR("/l") || tag == CONSTWSTR("/i"))
		{
			if (CHECK(fonts_stack.count() > 1)) fonts_stack.pop();
		}
		else if (tag == CONSTWSTR("hr"))
		{
            if (search_rects)
                line_ellipsis();

            if (last_line.count() > 0) end_line();

            token<wchar> t( tagbody, L',' ); //vertindent[,sideindent,[thickness]]
			
			int vertindent = ui_scale((t && !t->is_empty()) ? t->as_int() : 7);
            ++t; int sideindent = ui_scale((t && !t->is_empty()) ? t->as_int() : 3);

			if (glyphs)
			{
				glyph_image_s &gi = glyphs->add();
                gi.pixels = GTYPE_DRAWABLE;
				gi.color = colors_stack.last();
				gi.width = gi.height = 0; // zeros - no one lookup to gi.pixels 
				gi.pos().x = (int16)(pen.x + sideindent);
                gi.pos().y = (int16)(pen.y + vertindent);
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
                if ( mg.image_offset_Y < 0 )
                    mg.image_offset_Y = mg.image->height - mg.font->ascender + (mg.font->height - mg.image->height)/2; // ( mg.image->height - mg.font->height ) / 2 + 1;
                if (mg.image_offset_Y > 0)
                    ++addhtags;
            }
			mg.advance = mg.image->width;
			line_width += mg.advance;
		}
        else if (tag == CONSTWSTR("shadow"))
        {
            shadow_pars_s &sp = shadow_stack.add();
            token<wchar> t(tagbody, L';');
            sp.len = t ? t->as_int() : 2;
            ++t;
            sp.color = t ? parsecolor(t->as_sptr(), ARGB(0, 0, 0)) : ARGB(0, 0, 0);
        }
        else if (tag == CONSTWSTR("/shadow"))
        {
            if (CHECK(shadow_stack.count() > 0)) shadow_stack.pop();
        }
        else if (tag == CONSTWSTR("outline"))
        {
            outline_stack.add(parsecolor(tagbody.as_sptr(), ARGB(0, 0, 0)));
        }
        else if (tag == CONSTWSTR("/outline"))
        {
            if (CHECK(outline_stack.count() > 1)) outline_stack.pop();
        }
        else if (tag == CONSTWSTR("mark"))
        {
            marker_stack.add(parsecolor(tagbody.as_sptr(), ARGB(0, 0, 0)));
        }
        else if (tag == CONSTWSTR("/mark"))
        {
            if (CHECK(marker_stack.count() > 1)) marker_stack.pop();
        }
		else if (tag == CONSTWSTR("imgl") || tag == CONSTWSTR("imgr"))
		{
			side_text_limit &sl = tag == CONSTWSTR("imgl") ? leftSL : rightSL;

            if (search_rects)
                line_ellipsis();

			if (last_line.count() > 0) end_line();
			if (pen.y < sl.bottom) pen.y = sl.bottom;

			// add wrapped image
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
				gi.pos().x = (int16)(&sl == &leftSL ? 0 : max_line_length - si->width);
                gi.pos().y = (int16)pen.y;
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
            rite_rite = (int)last_line.count();
            if (rite_rite == 0)
                paragraphs_stack.last().rite = false;
        }
        else if (tag == CONSTWSTR("null"))
        {
            // do nothing with null tag
        } 
        else if (tag == CONSTWSTR("cstm"))
        {
            if (ctp)
            {
                wstr_c r;
                if (ctp(r, tagbody_))
                    parse(r, 0);
            }
        }
        else if (tag == CONSTWSTR("hyphenation"))
        {
            hyphenation_tag_nesting_level++;
        }
        else if (tag == CONSTWSTR("/hyphenation"))
        {
            if (CHECK(hyphenation_tag_nesting_level > 0)) hyphenation_tag_nesting_level--;
        }
        else if (tag == CONSTWSTR("ee"))
        {
            current_line_end_ellipsis = true;
        }
        else if (tag == CONSTWSTR("rect"))
        {
            current_line_with_rects = true;
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
            line_width += mg.advance;
        } else if (tag == CONSTWSTR("fullheight"))
        {
            font_c *f = fonts_stack.last();
            glyph_s &g = (*f)['_'];
            int h = g.top - g.height;
            full_height_line = tmin(last_line_descender, h, f->underline_add_y-(int)lceil(f->uline_thickness*.5f));

        } else if (tag == CONSTWSTR(".")) // <.> always ignored
			;
		else
			return false;

		return true;
	}

    void addchar(font_c &font, wchar ch, aint chari)
    {
        meta_glyph_s &mg = add_meta_glyph(ch == L' ' ? meta_glyph_s::SPACE : meta_glyph_s::CHAR, chari);
        mg.glyph = &font[mg.ch = ch];
        mg.advance = mg.glyph->advance;
        line_width += mg.advance;
        //Kerning processing
        meta_glyph_s *prev;
        aint cnt = last_line.count();
        if (cnt > 1 && (prev = &last_line.get(cnt - 2))->type == meta_glyph_s::CHAR && (cnt-1) != rite_rite)
        {
            int k = font.kerning_ci(prev->glyph->char_index, mg.glyph->char_index);
            prev->advance += k; // fix prev symbol width instead of pen moving (lighter code)
            line_width += k;
        }
    }

    void line_ellipsis()
    {
        aint j = last_line.count() - 1;

        glyph_s &dot_glyph = (*fonts_stack.last())[L'.'];
        line_width += dot_glyph.advance * 3; // advance of ...

        auto setupglyphspecial = [this](meta_glyph_s &mg, aint j)
        {
            for (aint i = j; i >= 0; i--)
            {
                const meta_glyph_s& mgf = last_line.get(i);
                if (mgf.type == meta_glyph_s::CHAR)
                {
                    mg.font = mgf.font;
                    mg.color = mgf.color;
                    mg.shadow = mgf.shadow;
                    mg.shadow_color = mgf.shadow_color;
                    mg.outline_color = mgf.outline_color;
                    mg.bg_color = mgf.bg_color;
                    mg.underlined = mgf.underlined;
                    mg.underline_offset = mgf.underline_offset;
                    break;
                }
            }
        };

        if (current_line_with_rects)
        {
            for (; j > 0; j--)
            {
                if (last_line.get(j).type == meta_glyph_s::RECTANGLE) continue;
                if ((line_width -= last_line.get(j).advance) > cur_max_line_len)
                    last_line.remove_slow(j);
                else break;
            }
            meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR); // append dot
            setupglyphspecial(mg,j);
            ++j;

            mg.glyph = &dot_glyph;
            mg.advance = mg.glyph->advance;
            meta_glyph_s mgt = mg;
            last_line.set_count(last_line.count() - 1);

            last_line.insert(j,mgt);
            last_line.insert(j+1,mgt);
            mgt.advance += 3; last_line.insert(j+2,mgt);


        } else
        {
            for (; j > 0; j--) if ((line_width -= last_line.get(j).advance) <= cur_max_line_len) break;
            last_line.set_count(j);

            meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR); // append dot
            setupglyphspecial(mg,last_line.count()-2);
            mg.glyph = &dot_glyph;
            mg.advance = mg.glyph->advance;
            meta_glyph_s mgt = mg;
            last_line.add(mgt);
            mgt.advance += 3; last_line.add(mgt);
        }

        search_rects = false;
        current_line_with_rects = false;
    }

	void parse(const ts::wsptr &text, int cur_text_index = 0)
	{
		for (; cur_text_index<text.l; ++cur_text_index)
		{
			font_c &font = *fonts_stack.last();
			paragraph_s &paragraph = paragraphs_stack.last();
			wchar ch = text.s[cur_text_index];

			if (ch == L'\n')
			{
                if (search_rects)
                    line_ellipsis();

                if (last_line.count() > 0) end_line();
                else next_line(font.height);// if empty line, just add height of current font

			} else if (ch != L'<') // just simple symbol
			{
            add_char:
                addchar(font, ch, cur_text_index);
			} else if (cur_text_index < text.l-1 && text.s[cur_text_index+1] == '|') // escaped '<'
			{
				cur_text_index++; //skip |
				goto add_char;
			} else // this is tag
			{
				int i = pwstr_c(text).find_pos(cur_text_index + 1, L'>');
				if (i == -1) {WARNING("Tag '%s' not closed", to_str(text.skip(cur_text_index)).cstr()); break;}

                token<wchar> t(pwstr_c(text).substr(cur_text_index+1, i), L'=');
                wsptr tag(*t);
                ++t; wsptr tagbody; if (t) tagbody = *t;
				if (!process_tag(tag, tagbody, cur_text_index))
					goto add_char; // unknown tag - keep it as is
				cur_text_index = i;
			}

			// wrap words
			if (!search_rects && line_width > cur_max_line_len && last_line.count() > 1) // check special case: 1 symbol cannot be inserted to line
			{
				aint j = last_line.count() - 1, line_size;

				if (FLAG(flags,TO_FORCE_SINGLELINE) || (FLAG(flags,TO_END_ELLIPSIS) && pen.y + fonts_stack.last()->height * 3 / 2 >= boundy))
				{
					glyph_s &dot_glyph = (*fonts_stack.last())[L'.'];
					line_width += dot_glyph.advance * 3; // advance of ...

					for (; j > 0; j--) if ((line_width -= last_line.get(j).advance) <= cur_max_line_len) break;
					last_line.set_count(j);

					for (int i=0; i<3; i++)
					{
						meta_glyph_s &mg = add_meta_glyph(meta_glyph_s::CHAR, cur_text_index); // append dot
						mg.glyph = &dot_glyph;
						mg.advance = mg.glyph->advance;
					}

					break;//early exit
				} else if (current_line_end_ellipsis)
                {
                    search_rects = true;
                    continue;

                } else
				{
					if (FLAG(flags,TO_WRAP_BREAK_WORD)) goto inlineBreak;

					//—разу формировать новую строку нельз€ - если окажетс€, что это единственное слово в строке, то его надо не переносить, а разбивать
					//ѕоиск последнего разделител€ (только пробел)
					//≈сли пробел стоит в начале строки, а сама строка состоит из одного слова, то слово всЄ равно переноситс€ на другую строку - это нормально
					while (j >= rite_rite && last_line.get(j).type != meta_glyph_s::SPACE) j--;

					char charclass[30];
					aint start = j+1, n = last_line.count() - start;
					if (hyphenation_tag_nesting_level && n < ARRAY_SIZE(charclass)-3)//нужно смотреть на 3 буквы вперЄд, чтобы работали правила gss-ssg и gs-ssg
					{
						//ѕеренос слов по слогам реализован на основе упрощЄнного алгоритма ѕ.’ристова http://sites.google.com/site/foliantapp/project-updates/hyphenation
						aint nn = tmin(n+3, text.l - (cur_text_index-n+1)), i = 0;

						while (i<n && wcschr(L"(\"Ђ", last_line.get(i+start).ch)) // skip valid characters before word begin
							charclass[i++] = 0;

						for (; i<nn; i++)
						{
							if (i < n)
							{
								meta_glyph_s &mg = last_line.get(i+start);
								if (mg.type != meta_glyph_s::CHAR) goto skipHyphenate;
								ch = mg.ch;
							}
							else
							{
								ch = text.s[cur_text_index+i-n+1];
							}
							if (wcschr(L"аеЄиоуыэю€aeiouyј≈®»ќ”џЁёяAEIOUY", ch))
								charclass[i] = 'g';
							else if (wcschr(L"бвгджзклмнпрстфхцчшщbcdfghjklmnpqrstvwxzЅ¬√ƒ∆« ЋћЌѕ–—“‘’÷„ЎўBCDFGHJKLMNPQRSTVWXZ", ch))
								charclass[i] = 's';
							else if (wcschr(L"йьъ…№Џ", ch))
								charclass[i] = 'x';
							else if (ch == '-')
								charclass[i] = '-';
							else if (wcschr(L".,;:!?)\"ї", ch)) // valid characters after word end
								{nn = i; break;}
							else if (i < n)
								goto skipHyphenate; // found unknown character - do not hyphenate this word
							else {nn = i; break;} // unknown word (may be tag or punctuation mark) => just stop at this word
						}
						for (int i=0; i<nn-2; i++)//[x, l+l]
							if (charclass[i] == 'x')
								charclass[i] = '-';//вставл€ем - перед началом след. слога, а не после, чтобы этот алгоритм был эквивалентен вставке '-' между слогами, иначе в простейших случа€х напр. [каталог/sgsgsgs] будет только один перенос вместо двух возможных
						for (int i=0; i<nn-5; i++)//[g+s+s, s+s+g]
							if ((uint32&)(charclass[i]) == MAKEFOURCC('g','s','s','s') && (uint16&)(charclass[i+4]) == MAKEWORD('s','g'))
								charclass[i+=2] = '-';
						for (int i=0; i<nn-4; i++)//[g+s+s, s+g], [g+s, s+s+g]
							if ((uint32&)(charclass[i]) == MAKEFOURCC('g','s','s','s') && charclass[i+4] == 'g')
								charclass[i+1] = '-', charclass[i+=2] = '-';
						for (int i=0; i<nn-3; i++)//[s+g, s+g], [g+s, s+g], [s+g, g+l]
							if ((uint32&)(charclass[i]) == MAKEFOURCC('s','g','s','g')
							 || (uint32&)(charclass[i]) == MAKEFOURCC('g','s','s','g')
							 || (uint32&)(charclass[i]) == MAKEFOURCC('s','g','g','s')
							 || (uint32&)(charclass[i]) == MAKEFOURCC('s','g','g','g'))
							 charclass[i+=1] = '-';

						int newLineLen = line_width;
						glyph_s &hyphenGlyph = (*fonts_stack.last())[L'-'];
						newLineLen += hyphenGlyph.advance;

						//»щем последний перенос
						for (aint i=n-1; i>2; i--)//не разрешаем переносить менее 3-х букв с начала и конца слова
							if ((newLineLen -= last_line.get(i+start).advance) <= cur_max_line_len && charclass[i-1] == '-' && i < nn-2)
							{
								line_size = start + i;
								line_width = newLineLen;
								if (last_line.get(i + start - 1).ch != '-')
								{
									line_size++;
									meta_glyph_s mg = add_meta_glyph(meta_glyph_s::CHAR, cur_text_index);
									mg.glyph = &hyphenGlyph;
									mg.advance = mg.glyph->advance;
									last_line.pop();
									last_line.insert(start + i, mg);
								}
								goto end;
							}
skipHyphenate:;
					}

					if (j >= rite_rite) // space found - word is not single => generate new line
					{
						last_line.remove_slow(j); // удал€ем пробел (чтобы он не участвовал в подсчете пробелов ниже, а также чтобы не добавл€лс€ соответствующий глиф - его может быть видно при подчеркивании)
						line_size = j; // break line on character after space
					}
					else // “акже формируем новую строку, но уже не по последнему пробелу, а по части слова, вмещающегос€ в строку
					{
inlineBreak:
						line_size = last_line.count()-1;//длину строки ограничиваем до текущего символа (невключительно)
						was_inword_break = true;
					}
end:;
				}

				int H, spaces, W, WR;
                pen.y += calc_HSW(H, spaces, W, WR, line_size); // calculate line height and spaces count

				if (glyphs)
				{
					ivec2 offset(0, H); // offset for every symbol before it put to glyphs array

                    int lineW = W;
					if (paragraph.align == paragraph_s::ARIGHT) offset.x = cur_max_line_len - W - WR;
                    else if (paragraph.align == paragraph_s::AJUSTIFY) { W = cur_max_line_len - W - WR; lineW = cur_max_line_len - WR; }
					else if (paragraph.align == paragraph_s::ACENTER) offset.x = (cur_max_line_len - W - WR)/2;

                    if (prev_line_dim_glyph_index >= 0) glyphs->get(prev_line_dim_glyph_index).next_dim_glyph = (int)glyphs->count();
                    prev_line_dim_glyph_index = (int)glyphs->count();
                    glyph_image_s &gi = glyphs->add(); // service glyph with line dimension

                    gi.pixels = nullptr;
                    gi.outline_index = -1;
                    gi.next_dim_glyph = -1;
                    gi.line_lt().x = (int16)(offset.x + pen.x);
                    gi.line_lt().y = (int16)(offset.y + pen.y - H);
                    gi.line_rb().x = (int16)(gi.line_lt().x + lineW);
                    gi.line_rb().y = (int16)(gi.line_lt().y + H);

					int spaceIndex = 0, offX = 0; // AJUSTIFY
                    int oldpenx = pen.x;
					for (j = rite_rite; j < line_size; j++)
					{
                        const meta_glyph_s &mg = last_line.get(j);
						int add_underline_len = 0;
						ivec2 pos = pen + offset;
						pen.x += mg.advance;

						if (paragraph.align == paragraph_s::AJUSTIFY)
						{
							pos.x += offX;
							if (last_line.get(j).type == meta_glyph_s::SPACE)
							{
								spaceIndex++;
								int prevOffX = offX;
								offX = W * spaceIndex / spaces;
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
                            mg.add_glyph_image(outlined_glyphs, glyphs, pen + offset, 0);
                            if (mg.image_offset_Y > 0)
                                for (ivec2 &x : addhs)
                                    if (x.x == i) { x.x = pen.x + offset.x; break; }
                            pen.x += mg.advance;
                        }
                        rite_rite = 0;
                    }

				} else
                    rite_rite = 0;

				// go next line
				last_line.remove_slow(0, line_size);
				next_line(H);
				for (j=0; j<last_line.count(); j++) line_width += last_line.get(j).advance;
			}
		}
    }

    ivec2 parse()
    {
        parse(textp->as_sptr(), 0);
        if (search_rects)
            line_ellipsis();
		end_line(true, 0 != (TO_LASTLINEADDH & flags)); // last line

		if (/*glyphs && */outlined_glyphs.count() > 0)
		{
            if (ASSERT(glyphs->get(0).pixels == nullptr))
            {
                glyphs->get(0).outline_index = (int)glyphs->count();
                glyphs->append_buf(outlined_glyphs);
            }
		}

		return ivec2(maxW, tmax(pen.y - last_line_descender, leftSL.bottom, rightSL.bottom));
	}
};
}


ivec2 parse_text(const wstr_c &text, int max_line_length, CUSTOM_TAG_PARSER ctp, GLYPHS *glyphs, TSCOLOR default_color, font_c *default_font, uint32 flags, int boundy)
{
	if (!ASSERT(default_font)) return ivec2(0);
	static text_parser_s text_parser;
    text_parser.setup(text, tabs(max_line_length), ctp, glyphs, default_color, default_font, flags, boundy);
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

		res.combine( irect(gi.pos(), ts::ivec2( gi.pos() + svec2(gi.width, gi.height))) );

		if (gi.thickness > 0)
		{
			ivec2 start( gi.pos() + gi.start_pos() );
			float thick = (tmax(gi.thickness, 1.f) - 1)*.5f;
			int d = lceil(thick);
			start.y -= d;
			res.combine( irect(start, start + ivec2(gi.length, d*2 + 1)) );
		}
	}

	return res;
}

int glyphs_nearest_glyph(const GLYPHS &glyphs, const ivec2 &p, bool strong)
{
    aint cnt = glyphs.count();
    int glyphs_start = 0;
    if (cnt && glyphs.get(0).pixels == nullptr && glyphs.get(0).outline_index > 0)
    {
        glyphs_start = 1;
        cnt = glyphs.get(0).outline_index;
    }
    for (int i=glyphs_start;i<cnt;)
    {
        const glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels == nullptr)
        {
            if( gi.outline_index == 0 )
            {
                ++i;
                continue;
            }

            if ( p.y <= gi.line_rb().y || gi.next_dim_glyph < 0 )
            {
                if (gi.next_dim_glyph > 0)
                    cnt = gi.next_dim_glyph;

                int maxw = 0;
                int mind = maximum<int>::value;
                int index = -1;

                for(int j=i+1; j<cnt; ++j)
                {
                    const glyph_image_s &gic = glyphs.get(j);
                    if (gic.charindex < 0) continue;
                    int d = tabs(gic.pos().x + gic.width / 2 - p.x);
                    if (d < mind)
                    {
                        mind = d;
                        index = j;
                    }
                    if (strong && gic.width > maxw)
                        maxw = gic.width;
                }
                if (index >= 0 && strong && mind > maxw) return -1;
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

int glyphs_get_charindex(const GLYPHS &glyphs, aint glyphindex)
{
    if (glyphindex < 0) return -1;
    aint glyphc = glyphs.count();
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
