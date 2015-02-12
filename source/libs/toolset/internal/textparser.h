/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

#include "glyphscache.h"

namespace ts
{
	NUMGEN_START(dopb, 0);

	enum text_options_e
	{
		TO_HCENTER          = SETBIT(NUMGEN_NEXT(dopb)),
		TO_VCENTER          = SETBIT(NUMGEN_NEXT(dopb)),
		TO_CLIP             = SETBIT(NUMGEN_NEXT(dopb)),
		TO_MULTILINE        = SETBIT(NUMGEN_NEXT(dopb)),
		TO_END_ELLIPSIS     = SETBIT(NUMGEN_NEXT(dopb)),//like SINGLELINE, but also works for multiline text
		TO_WRAP_BREAK_WORD  = SETBIT(NUMGEN_NEXT(dopb)),//wrap line by any symbol, not just by space
		TO_NOT_SCALE_FONT   = SETBIT(NUMGEN_NEXT(dopb)),
        TO_LASTLINEADDH     = SETBIT(NUMGEN_NEXT(dopb)),

        TO_LAST_OPTION      = SETBIT(NUMGEN_NEXT(dopb)),
	};

	extern font_desc_c g_default_text_font;

struct glyph_image_s
{
    DUMMY(glyph_image_s);

    union
    {
        struct // на самом деле не глиф, а служебная информация
        {
            void *zeroptr;
            int outline_index; // если -1, следующие 2 поля актуальныы, если 0, то этот глиф надо игнорировать, если >0, то сначала идет отрисовка глифов от этого индекса и до конца массива
            int next_dim_glyph;
            svec2 line_lt;
            svec2 line_rb;
        };
        struct
        {
            const uint8 *pixels;
            int charindex;
            TSCOLOR color; //если не 0, то pixels указывает на alpha-изображение, которое нужно вывести с данным color (rgb берется напрямую из color, а альфа получается перемножением color.a и alpha пикселя)
                           //если = 0, то pixels указывает на rgba-premultiplied-изображение, которое выводится как есть

            /*underline*/ float thickness;//толщина линии (если < 0, то линию рисовать не нужно - это для всяких span-ов, чтоб можно было динамически и независимо включать подчеркивание текста в них)

	        svec2 pos;
            /*underline*/ svec2 start_pos;//начало линии относительно левого верхнего угла изображения
	        uint16 width, height, pitch, /*underline*/ length;
        };
        struct
        {
            uint8 dummy[32];
        };
    };
};

TS_STATIC_CHECK( sizeof(glyph_image_s) == 32, "oops" );

typedef tbuf0_t<glyph_image_s> GLYPHS;

ivec2 parse_text(const wstr_c &text, int max_line_length, GLYPHS *glyphs = nullptr, TSCOLOR default_color = ARGB(0,0,0), font_c *default_font = g_default_text_font, uint32 flags = 0, int boundy = 0);

//Считает прямоугольник, ограничивающий все глифы из заданного массива
irect glyphs_bound_rect(const GLYPHS &glyphs);
int glyphs_nearest_glyph(const GLYPHS &glyphs, const ivec2 &p);
int glyphs_get_charindex(const GLYPHS &glyphs, int glyphindex);

} // namespace ts