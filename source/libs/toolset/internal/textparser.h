/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

#include "glyphscache.h"

#define GTYPE_RECTANGLE ((ts::uint8 *)1) // special pointer value - 1 - means rectangle
#define GTYPE_BGCOLOR_BEGIN ((ts::uint8 *)2)
#define GTYPE_BGCOLOR_MIDDLE ((ts::uint8 *)3)
#define GTYPE_BGCOLOR_END ((ts::uint8 *)4)
#define GTYPE_DRAWABLE ((ts::uint8 *)16) // nullptr or value less 16 means special glyph, skipped while draw. 16 value let this glyph drawable

namespace ts
{
	NUMGEN_START(dopb, 0);

	enum text_options_e
	{
		TO_HCENTER              = SETBIT(NUMGEN_NEXT(dopb)),
		TO_VCENTER              = SETBIT(NUMGEN_NEXT(dopb)),
		TO_MULTILINE            = SETBIT(NUMGEN_NEXT(dopb)),
        TO_FORCE_SINGLELINE     = SETBIT(NUMGEN_NEXT(dopb)),
		TO_END_ELLIPSIS         = SETBIT(NUMGEN_NEXT(dopb)), // like SINGLELINE, but also works for multiline text
        TO_LINE_END_ELLIPSIS    = SETBIT(NUMGEN_NEXT(dopb)), // do not wrap words in multiline text
		TO_WRAP_BREAK_WORD      = SETBIT(NUMGEN_NEXT(dopb)), // wrap line by any symbol, not just by space
        TO_LASTLINEADDH         = SETBIT(NUMGEN_NEXT(dopb)),

        TO_LAST_OPTION      = SETBIT(NUMGEN_NEXT(dopb)),
	};

	extern font_desc_c g_default_text_font;

struct glyph_image_s
{
    DUMMY(glyph_image_s);

    union
    {
        struct // not a glyph, but service info
        {
            void *zeroptr; // nullptr // same as pixels
            int outline_index; // if -1, then next 3 fields are actual, if 0, this glyph should be skipped, if >0, then rendering order should be changed according value
            int next_dim_glyph;
            svec2 line_lt;
            svec2 line_rb;
        };
        struct
        {
            const uint8 *pixels; // not nullptr
            int charindex;
            TSCOLOR color; //if !0, then pixels points to alpha-image (8 bit per pixel); image should be rendered with color
                           //if ==0, то pixels points to rgba-premultiplied-image; image should be rendered as is

            /*underline*/ float thickness; // if < 0, then underline shouldn't be drawn

	        svec2 pos;
            /*underline*/ svec2 start_pos; // underline start position
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

typedef fastdelegate::FastDelegate<bool (wstr_c &, const wsptr &)> CUSTOM_TAG_PARSER;

ivec2 parse_text(const wstr_c &text, int max_line_length, CUSTOM_TAG_PARSER ctp, GLYPHS *glyphs = nullptr, TSCOLOR default_color = ARGB(0,0,0), font_c *default_font = g_default_text_font, uint32 flags = 0, int boundy = 0);

irect glyphs_bound_rect(const GLYPHS &glyphs);
int glyphs_nearest_glyph(const GLYPHS &glyphs, const ivec2 &p, bool strong = false);
int glyphs_get_charindex(const GLYPHS &glyphs, int glyphindex);

} // namespace ts