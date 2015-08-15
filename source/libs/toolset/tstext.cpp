#include "toolset.h"

#pragma comment (lib, "freetype.lib")

namespace ts
{

    FORCEINLINE ivec4 TSCOLORtoVec4(TSCOLOR color)
    {
        return ivec4(RED(color), GREEN(color), BLUE(color), ALPHA(color));
    }


inline void writePixelBlended(TSCOLOR &dst, const ivec4 &srcColor)
{
	ivec4 dstColor(TSCOLORtoVec4(dst));
	dstColor.rgb() = srcColor.rgb()*srcColor.a + dstColor.rgb()*(255-srcColor.a);
	dstColor.a = 255*256 - (255-dstColor.a)*(255-srcColor.a);
	dst = ((dstColor.r & 0xFF00) << 8) | (dstColor.g & 0xFF00) | (dstColor.b >> 8) | ((dstColor.a & 0xFF00) << 16);
}

__forceinline void write_pixel_pm( TSCOLOR &dst, TSCOLOR src )
{
    TSCOLOR c = dst;

    uint8 ba = ALPHA(src); if (ba == 0) return;
    float a = (float)((double)(ba) * (1.0 / 255.0));
    float not_a = 1.0f - a;

    auint oiB = lround(float(BLUE(c)) * not_a) + BLUE(src);
    auint oiG = lround(float(GREEN(c)) * not_a) + GREEN(src);
    auint oiR = lround(float(RED(c)) * not_a) + RED(src);
    auint oiA = lround(float(ALPHA(c)) * not_a) + ALPHA(src);

    dst = oiB | (oiG << 8) | (oiR << 16) | (oiA << 24);

}

__forceinline void write_pixel(TSCOLOR &dst, TSCOLOR src, uint8 aa)
{
    TSCOLOR c = dst;

    float a = (float)((double)(ALPHA(src) * aa) * (1.0 / 65025.0));
    float not_a = 1.0f - a;

    auint oiB = lround(float(BLUE(c)) * not_a + float(BLUE(src)) * a);
    auint oiG = lround(float(GREEN(c)) * not_a + float(GREEN(src)) * a);
    auint oiR = lround(float(RED(c)) * not_a + float(RED(src)) * a);
    auint oiA = lround(float(ALPHA(c)) * not_a + a * 255.0);

    dst = oiB | (oiG << 8) | (oiR << 16) | (oiA << 24);
}

bool text_rect_c::draw_glyphs(uint8 *dst_, int width, int height, int pitch, const array_wrapper_c<const glyph_image_s> &glyphs, const ivec2 &offset, bool prior_clear)
{
    // sorry guys, russian description of algorithm

	/*
      TAV:
      Для рисования глифов в текстуру используется специальный алгоритм блендинга. Т.к. глифы могут рисоваться поверх друг друга,
	  в частности при использовании специальных тегов или интегрированной в текстуру картинки с фоном, то просто рисовать глифы
	  поверх друг друга или использовать простые техники (напр. рисовать цвет, если альфа > 0, и брать максимальную альфу) нельзя.
	  Необходимо, чтобы полученный результат был эквивалентен независимому отображению всех заданных глифов, по очереди, со
	  стандартным блендингом (ALPHA, 1-ALPHA) как независимых изображений. Ниже представлен подход, который позволяется добиться
	  такого результата запеканием в одну текстуру.

	--- Rationale ---
	Задача: сгенерировать буфер с текстурой с альфой для наложения на заранее неизвестный background (во фреймбуфер),
			в буфер нужно скопировать картинку с альфой и наложить на неё текст с альфой.
	d1 - background (заранее не известен)
	s1 - картинка
	s2 - текст
	По стандартной формуле смешения (ALPHA, 1-ALPHA) если бы background был известен, итоговое значение цвета (d3) можно было бы найти так:
	d2 = s1*s1a+d1*(1-s1a)
	d3 = s2*s2a+d2*(1-s2a) = s2*s2a + (s1*s1a+d1*(1-s1a)) * (1-s2a)
		= (_ s2*s2a + s1*s1a*(1-s2a) _)  +  d1 * (_ (1-s1a)*(1-s2a) _)
	Часть слева обозначим за RGB, а справа - за А. Обе части будем считать независимо друг от друга. Т.к. значение d1 не известно, подставлять его нельзя.
	Изначально буфер dst заполним (0,0,0,255). Рисовать софтово в этот буфер (dst) из исходного буфера (src,alpha) будем по формуле:
	RGB = src*alpha + dst*(1-alpha)
	A = A*(1-alpha)
	Т.о. в буфере dst не зависимо от кол-ва рисований поверх, в RGB будет цветовая часть, а в A - альфа.
	Чтобы корректно вывести полученный буфер на GPU, нужно включить бленд (ONE, ALPHA).*/

	//Сначала подготавливаем буфер назначения, т.е. заполняем его нулями (не (0,0,0,255), т.к. альфу нужно получить в итоге инвертированную).

	uint8 *dst = dst_;
	if (prior_clear)
		for (int h=height; h; h--, dst+=pitch)
			memset(dst, 0, width*sizeof(TSCOLOR));

    int g = 0;
    if (glyphs.size() && glyphs[0].pixels == nullptr)
    {
        // special glyph changing rendering order
        if ( glyphs[0].outline_index > 0 )
        {
            draw_glyphs(dst_, width, height, pitch, glyphs.subarray(glyphs[0].outline_index), offset, false);
            return draw_glyphs(dst_, width, height, pitch, glyphs.subarray(1, glyphs[0].outline_index), offset, false);
        }
        g = 1;
    }

	// now render glyphs, clipping them by buffer area
    bool rectangles = false;
	for (; g<glyphs.size(); g++)
	{
		const glyph_image_s &glyph = glyphs[g];
        if (glyph.pixels < (uint8 *)16)
        {
            rectangles |= (glyph.pixels == (uint8 *)1);
            continue; // pointer value < 16 means special glyph. Special glyph should be skipped while rendering
        }

		int bpp = glyph.color ? 1 : 4; // bytes per pixel
		int glyphPitch = glyph.pitch;
		ivec4 glyphColor = TSCOLORtoVec4(glyph.color);
		ivec2 pos(glyph.pos);  pos += offset; // glyph rendering position
        ts::TSCOLOR gcol = glyph.color;

		// draw underline
		if (glyph.thickness > 0)
		{
			ivec2 start = pos + ivec2(glyph.start_pos);
			float thick = (tmax(glyph.thickness, 1.f) - 1)*.5f;
			int d = lceil(thick);
			start.y -= d;
			ivec2 end = tmin(ivec2(width, height), start + ivec2(glyph.length, d*2 + 1));
			start = tmax(ivec2(0,0), start);
			if (end.x > start.x && end.y > start.y)
			{
				dst = dst_ + start.y * pitch;
				ivec4 c = bpp == 1 ? glyphColor : ivec4(255);
				ivec4 ca(c.rgb(), lround(c.a*(1 + thick - d)));
				// begin
				for (int i=start.x; i<end.x; i++) writePixelBlended(((TSCOLOR*)dst)[i], ca);
				start.y++, dst += pitch;
				// middle
				for (; start.y<end.y-1; start.y++, dst += pitch)
					for (int i=start.x; i<end.x; i++) writePixelBlended(((TSCOLOR*)dst)[i], c);
				// tail
				if (start.y<end.y)
					for (int i=start.x; i<end.x; i++) writePixelBlended(((TSCOLOR*)dst)[i], ca);
			}
		}

		// draw glyph
		int clippedWidth  = glyph.width;
		int clippedHeight = glyph.height;
		const uint8 *src = glyph.pixels;
		// clipping
		if (pos.x < 0) src -= pos.x*bpp       , clippedWidth  += pos.x, pos.x = 0;
		if (pos.y < 0) src -= pos.y*glyphPitch, clippedHeight += pos.y, pos.y = 0;
		if (pos.x + clippedWidth  > width ) clippedWidth  = width  - pos.x;
		if (pos.y + clippedHeight > height) clippedHeight = height - pos.y;

		if (clippedWidth <= 0 || clippedHeight <= 0) continue; // fully clipped - skip

		dst = dst_ + pos.x * sizeof(TSCOLOR) + pos.y * pitch;

        if (bpp == 1)
        {
            for (; clippedHeight; clippedHeight--, src += glyphPitch, dst += pitch)
                for (int i = 0; i < clippedWidth; i++)
                    write_pixel( ((TSCOLOR*)dst)[i], gcol, src[i] );
        } else
        {
            for (; clippedHeight; clippedHeight--, src += glyphPitch, dst += pitch)
                for (int i = 0; i < clippedWidth; i++)
                    write_pixel_pm( ((TSCOLOR*)dst)[i], ((TSCOLOR*)src)[i] );
        }

	}
    return rectangles;
}


text_rect_c::~text_rect_c()
{
}

void text_rect_c::update_rectangles( ts::ivec2 &offset, rectangle_update_s * updr )
{
    offset += updr->offset;
    for (const glyph_image_s &gi : glyphs().array())
        if (gi.pixels == (uint8 *)1)
            updr->updrect(updr->param, gi.pitch, ivec2(gi.pos.x + offset.x, gi.pos.y + offset.y));
}

void text_rect_c::update_rectangles( rectangle_update_s * updr )
{
    ASSERT(!is_dirty());
    ivec2 toffset(ui_scale(margin_left), ui_scale(margin_top) - scroll_top + (flags.is(TO_VCENTER) ? (size.y - text_height) / 2 : 0));
    update_rectangles(toffset, updr);
}

void text_rect_c::render_texture(rectangle_update_s * updr)
{
    if (glyphs().count() == 0) { safe_destruct(texture); return; }
    CHECK(!flags.is(F_INVALID_SIZE|F_INVALID_GLYPHS));
	if (!CHECK(size.x > 0 && size.y > 0)) return;
    texture.ajust(size,false);
    flags.clear(F_DIRTY|F_INVALID_TEXTURE);
    ivec2 toffset(ui_scale(margin_left), ui_scale(margin_top) - scroll_top + (flags.is(TO_VCENTER) ? (size.y-text_height)/2 : 0));
	if (draw_glyphs((uint8*)texture.body(), size.x, size.y, texture.info().pitch, glyphs().array(), toffset) && updr)
        update_rectangles(toffset, updr);
}

void text_rect_c::render_texture( rectangle_update_s * updr, fastdelegate::FastDelegate< void (drawable_bitmap_c&) > clearp )
{
    if (glyphs().count() == 0) return safe_destruct(texture);
    CHECK(!flags.is(F_INVALID_SIZE|F_INVALID_GLYPHS));
    if (!CHECK(size.x > 0 && size.y > 0)) return;
    texture.ajust(size, false);
    flags.clear(F_DIRTY|F_INVALID_TEXTURE);
    clearp(texture);
    ivec2 toffset(ui_scale(margin_left), ui_scale(margin_top) - scroll_top + (flags.is(TO_VCENTER) ? (size.y - text_height) / 2 : 0));
    if (draw_glyphs((uint8*)texture.body(), size.x, size.y, texture.info().pitch, glyphs().array(), toffset, false) && updr)
        update_rectangles(toffset, updr);
}

bool text_rect_c::set_text(const wstr_c &text_, CUSTOM_TAG_PARSER ctp, bool do_parse_and_render_texture)
{
    bool dirty = flags.is(F_INVALID_SIZE) || texture.ajust(size,false) || flags.is(F_DIRTY);

	if (dirty || !text.equals(text_))
	{
        flags.set(F_DIRTY|F_INVALID_TEXTURE);
		text = text_;
		if (do_parse_and_render_texture && (*font)) 
            parse_and_render_texture(nullptr, ctp); 
        return true;
	}
    return false;
}

void text_rect_c::parse_and_render_texture(rectangle_update_s * updr, CUSTOM_TAG_PARSER ctp, bool do_render)
{
	glyphs().clear();
	int f = flags & (TO_WRAP_BREAK_WORD | TO_HCENTER | TO_LASTLINEADDH | TO_FORCE_SINGLELINE | TO_END_ELLIPSIS | TO_LINE_END_ELLIPSIS);
    flags.clear(F_INVALID_GLYPHS);
	lastdrawsize = parse_text(text, size.x-ui_scale(margin_left)-ui_scale(margin_right), ctp, &glyphs(), default_color, (*font), f, size.y - ui_scale(margin_top));
	text_height = lastdrawsize.y + ui_scale(margin_top);
	lastdrawsize.x += ui_scale(margin_left) + ui_scale(margin_right);
    lastdrawsize.y = text_height;
	if (do_render) render_texture(updr);
}

ivec2 text_rect_c::calc_text_size(int maxwidth, CUSTOM_TAG_PARSER ctp) const
{
    if (!is_dirty() && (size.x == maxwidth || maxwidth < 0)) return lastdrawsize;

    int w = maxwidth; if (w < 0) w = 16384;
    int f = flags & (TO_WRAP_BREAK_WORD | TO_HCENTER | TO_LASTLINEADDH | TO_FORCE_SINGLELINE | TO_END_ELLIPSIS | TO_LINE_END_ELLIPSIS);
    ts::ivec2 sz = parse_text(text, w-ui_scale(margin_left)-ui_scale(margin_right), ctp, nullptr, default_color, (*font), f, 0);

    return sz + ts::ivec2(ui_scale(margin_left) + ui_scale(margin_right), margin_top);
}

ivec2 text_rect_c::calc_text_size( const font_desc_c& f, const wstr_c& t, int maxwidth, uint flgs, CUSTOM_TAG_PARSER ctp ) const
{
    return parse_text(t, maxwidth, ctp, nullptr, ARGB(0,0,0), f, flgs, 0);
}

} // namespace ts
