#include "toolset.h"

#pragma comment (lib, "freetype.lib")

namespace ts
{

    INLINE ivec4 TSCOLORtoVec4(TSCOLOR color)
    {
        return ivec4(RED(color), GREEN(color), BLUE(color), ALPHA(color));
    }


inline void writePixelBlended(TSCOLOR &dst, const ivec4 &srcColor)
{
	ivec4 dstColor(TSCOLORtoVec4(dst));
	dstColor.rgb() = srcColor.rgb()*srcColor.a + dstColor.rgb()*(256-srcColor.a);
	dstColor.a = 255*256 - (255-dstColor.a)*(255-srcColor.a);
	dst = ((dstColor.r & 0xFF00) << 8) | (dstColor.g & 0xFF00) | (dstColor.b >> 8) | ((dstColor.a & 0xFF00) << 16);
}

__forceinline void write_pixel(TSCOLOR &dst, TSCOLOR src, uint8 aa)
{
    if (aa == 0) return;
    TSCOLOR c = dst;

    extern uint8 __declspec(align(256)) multbl[256][256];

    uint8 a = multbl[aa][ ALPHA(src) ];
    uint16 not_a = 255 - a;

    uint B = multbl[not_a][BLUE(c)] + multbl[a][BLUE(src)];
    uint G = multbl[not_a][GREEN(c)] + multbl[a][GREEN(src)];
    uint R = multbl[not_a][RED(c)] + multbl[a][RED(src)];
    uint A = multbl[not_a][ALPHA(c)] + a;

    dst = B | (G << 8) | (R << 16) | (A << 24);
}


static const __declspec(align(16)) uint8 prepare_4alphas[16] = { 255, 0, 255, 1, 255, 2, 255, 3, 255, 0, 255, 1, 255, 2, 255, 3 };
static const __declspec(align(16)) uint16 add1[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static const __declspec(align(16)) uint8 invhi[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 255, 0, 255, 0, 255, 0 };

static const __declspec(align(16)) uint8 prepare_a1r[16] = { 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3, 2, 3 };
static const __declspec(align(16)) uint8 prepare_a2r[16] = { 4, 5, 4, 5, 4, 5, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7 };

static const __declspec(align(16)) uint8 prepare_a1l[16] = { 8, 9, 8, 9, 8, 9, 8, 9, 10, 11, 10, 11, 10, 11, 10, 11 };
static const __declspec(align(16)) uint8 prepare_a2l[16] = { 12, 13, 12, 13, 12, 13, 12, 13, 14, 15, 14, 15, 14, 15, 14, 15 };

namespace sse_consts
{
extern __declspec(align(16)) uint8 preparetgtc_1[16];
extern __declspec(align(16)) uint8 preparetgtc_2[16];
extern __declspec(align(16)) uint8 packcback_1[16];
extern __declspec(align(16)) uint8 packcback_2[16];
};

__declspec(naked) void write_row_sse(uint8 *dst_argb, const uint8 *src_argb, int w, const uint16 * color)
{
    __asm {
        push       esi
        mov        edx, [esp + 4 + 4]   // dst_argb
        mov        esi, [esp + 4 + 8]   // src_argb
        mov        ecx, [esp + 4 + 12]  // width
        mov        eax, [esp + 4 + 16]  // color
        movdqa     xmm6, [eax]
        movdqa     xmm7, [eax+16]

        movdqa     xmm3, prepare_4alphas

    alphablendloop :

        mov        eax, [esi]           // get 4 source pixels
        lea        esi, [esi+4]
        movd       xmm5, eax
        pshufb     xmm5, xmm3
        pmulhuw    xmm5, xmm7
        pxor       xmm5, invhi 
        paddw      xmm5, add1           // prepared multiplication of source 4 alphas with srccoloralpha; high part - inv(aa * a), low part - (aa * a)

        lddqu      xmm4, [edx]          // take dst 0..3 pixels
        movdqa     xmm0, xmm4
        pshufb     xmm0, sse_consts::preparetgtc_1   // two dst pixels to xmm0
        movdqa     xmm1, xmm5
        pshufb     xmm1, prepare_a1r
        pmulhuw    xmm1, xmm6
        movdqa     xmm2, xmm5
        pshufb     xmm2, prepare_a1l
        pmulhuw    xmm0, xmm2
        paddw      xmm0, xmm1
        pshufb     xmm0, sse_consts::packcback_1

        pshufb     xmm4, sse_consts::preparetgtc_2
        movdqa     xmm1, xmm5
        pshufb     xmm1, prepare_a2r
        pmulhuw    xmm1, xmm6
        pshufb     xmm5, prepare_a2l
        pmulhuw    xmm4, xmm5
        paddw      xmm4, xmm1
        pshufb     xmm4, sse_consts::packcback_2
        por        xmm0, xmm4

        movdqu    [edx], xmm0

        lea        edx, [edx + 16]

        sub        ecx, 4               // 4 dst pixels at once
        jg         alphablendloop

        pop        esi
        ret
    }
}

void __special_blend(uint8 *dst_argb, int dst_pitch, const uint8 *src_alpha, int src_pitch, int width, int height, TSCOLOR color)
{
    if (CCAPS(CPU_SSSE3))
    {
        int w = width & ~3;

        if (w)
        {
            uint16 ap1 = 1 + ALPHA(color);
            __declspec(align(16)) ts::uint16 precolor[16] =
            { 
                BLUEx256(color), GREENx256(color), REDx256(color), (255 << 8), BLUEx256(color), GREENx256(color), REDx256(color), (255 << 8),
                ap1, ap1, ap1, ap1, ap1, ap1, ap1, ap1
            };

            uint8 * dst_sse = dst_argb;
            const uint8 * src_sse = src_alpha;

            for (int y = 0; y < height; ++y, dst_sse += dst_pitch, src_sse += src_pitch)
                write_row_sse(dst_sse, src_sse, w, precolor);
        }

        if (int ost = width & 3)
        {
            src_alpha += w;
            dst_argb += w * 4;

            for (; height; height--, src_alpha += src_pitch, dst_argb += dst_pitch)
                for (int i = 0; i < ost; ++i)
                    write_pixel(((TSCOLOR*)dst_argb)[i], color, src_alpha[i]);
        }

        return;
    }

    for (; height; height--, src_alpha += src_pitch, dst_argb += dst_pitch)
        for (int i = 0; i < width; ++i)
            write_pixel(((TSCOLOR*)dst_argb)[i], color, src_alpha[i]);
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
        if (glyph.pixels < GTYPE_DRAWABLE)
        {
            if ( glyph.pixels == GTYPE_BGCOLOR_MIDDLE )
            {
                // draw bg
                ivec2 pos(glyph.pos);  pos += offset; // glyph rendering position

                ivec2 clipped_size( glyph.width, glyph.height );
                // clipping
                if (pos.x < 0) clipped_size.x += pos.x, pos.x = 0;
                if (pos.y < 0) clipped_size.y += pos.y, pos.y = 0;
                if (pos.x + clipped_size.x > width) clipped_size.x = width - pos.x;
                if (pos.y + clipped_size.y > height) clipped_size.y = height - pos.y;

                if (clipped_size.x <= 0 || clipped_size.y <= 0) continue; // fully clipped - skip

                dst = dst_ + pos.x * sizeof(TSCOLOR) + pos.y * pitch;

                ts::TSCOLOR col = glyph.color;

                img_helper_overfill(dst, imgdesc_s(clipped_size, 32, (int16)pitch), col);

                /*
                for (; clipped_height; clipped_height--, dst += pitch)
                    for (int i = 0; i < clipped_width; i++)
                        ((TSCOLOR*)dst)[i] = ALPHABLEND_PM(((TSCOLOR*)dst)[i], col);
                */

                continue;
            }

            rectangles |= (glyph.pixels == GTYPE_RECTANGLE);
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
				ivec4 ca(c.rgb(), ts::lround(c.a*(1 + thick - d)));
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
		int clipped_width  = glyph.width;
		int clipped_height = glyph.height;
		const uint8 *src = glyph.pixels;
		// clipping
		if (pos.x < 0) src -= pos.x*bpp       , clipped_width  += pos.x, pos.x = 0;
		if (pos.y < 0) src -= pos.y*glyphPitch, clipped_height += pos.y, pos.y = 0;
		if (pos.x + clipped_width  > width ) clipped_width  = width  - pos.x;
		if (pos.y + clipped_height > height) clipped_height = height - pos.y;

		if (clipped_width <= 0 || clipped_height <= 0) continue; // fully clipped - skip

		dst = dst_ + pos.x * sizeof(TSCOLOR) + pos.y * pitch;

        if (bpp == 1)
        {
            __special_blend( dst, pitch, src, glyphPitch, clipped_width, clipped_height, gcol );

            //for (; clipped_height; clipped_height--, src += glyphPitch, dst += pitch)
            //    for (int i = 0; i < clipped_width; i++)
            //        write_pixel( ((TSCOLOR*)dst)[i], gcol, src[i] );
        } else
        {
            for (; clipped_height; clipped_height--, src += glyphPitch, dst += pitch)
                for (int i = 0; i < clipped_width; i++)
                    ((TSCOLOR*)dst)[i] = ALPHABLEND_PM( ((TSCOLOR*)dst)[i], ((TSCOLOR*)src)[i] );
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
    ivec2 toffset = get_offset();
    update_rectangles(toffset, updr);
}

void text_rect_c::render_texture(rectangle_update_s * updr)
{
    if (glyphs().count() == 0) { texture_no_need(); return; }
    CHECK(!flags.is(F_INVALID_SIZE|F_INVALID_GLYPHS));
	if (!CHECK(size.x > 0 && size.y > 0)) return;
    texture().ajust_ARGB(size,false);
    flags.clear(F_DIRTY|F_INVALID_TEXTURE);
    ivec2 toffset = get_offset();
	if (draw_glyphs((uint8*)texture().body(), size.x, size.y, texture().info().pitch, glyphs().array(), toffset) && updr)
        update_rectangles(toffset, updr);
}

void text_rect_c::render_texture( rectangle_update_s * updr, fastdelegate::FastDelegate< void (bitmap_c&, const ivec2 &size) > clearp )
{
    if (glyphs().count() == 0) return texture_no_need();
    CHECK(!flags.is(F_INVALID_SIZE|F_INVALID_GLYPHS));
    if (!CHECK(size.x > 0 && size.y > 0)) return;
    texture().ajust_ARGB(size, false);
    flags.clear(F_DIRTY|F_INVALID_TEXTURE);
    clearp(texture(), size);
    ivec2 toffset = get_offset();
    if (draw_glyphs((uint8*)texture().body(), size.x, size.y, texture().info().pitch, glyphs().array(), toffset, false) && updr)
        update_rectangles(toffset, updr);
}

bool text_rect_c::set_text(const wstr_c &text_, CUSTOM_TAG_PARSER ctp, bool do_parse_and_render_texture)
{
    bool dirty = flags.is(F_INVALID_SIZE) || texture().ajust_ARGB(size,false) || flags.is(F_DIRTY);

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
	lastdrawsize = parse_text(text, size.x-ui_scale(margins_lt.x)-ui_scale(margin_right), ctp, &glyphs(), default_color, (*font), f, size.y - ui_scale(margins_lt.y));
	text_height = lastdrawsize.y + ui_scale(margins_lt.y);
	lastdrawsize.x += ui_scale(margins_lt.x) + ui_scale(margin_right);
    lastdrawsize.y = text_height;
	if (do_render) render_texture(updr);
}

ivec2 text_rect_c::calc_text_size(int maxwidth, CUSTOM_TAG_PARSER ctp) const
{
    int w = maxwidth; if (w < 0) w = 16384;
    int f = flags & (TO_WRAP_BREAK_WORD | TO_HCENTER | TO_LASTLINEADDH | TO_FORCE_SINGLELINE | TO_END_ELLIPSIS | TO_LINE_END_ELLIPSIS);
    ts::ivec2 sz = parse_text(text, w-ui_scale(margins_lt.x)-ui_scale(margin_right), ctp, nullptr, default_color, (*font), f, 0);

    return sz + ts::ivec2(ui_scale(margins_lt.x) + ui_scale(margin_right), margins_lt.y);
}

ivec2 text_rect_c::calc_text_size( const font_desc_c& f, const wstr_c& t, int maxwidth, uint flgs, CUSTOM_TAG_PARSER ctp ) const
{
    return parse_text(t, maxwidth, ctp, nullptr, ARGB(0,0,0), f, flgs, 0);
}

} // namespace ts
