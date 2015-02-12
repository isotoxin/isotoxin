#pragma once

#include "internal/textparser.h"

namespace ts
{

typedef fastdelegate::FastDelegate< void (void *, int, const ivec2 &) > UPDATE_RECTANGLE;

struct rectangle_update_s
{
    UPDATE_RECTANGLE updrect;
    ts::ivec2 offset;
    void *param;
};

class text_rect_c // формирователь текстуры с текстом
{
    TSCOLOR default_color;
public:
	font_desc_c default_font;
	wstr_c format_;//формат текста (просто строчка, которая добавляется в начало текста)
	wstr_c text_;//текст
    ivec2 size;
    ivec2 lastdrawsize;
	drawable_bitmap_c texture;//текстура с текстом
	GLYPHS  glyphs_internal;//массив с глифами
    GLYPHS *glyphs_external = nullptr;
	int scroll_top;
	int text_height;
	int margin_left, margin_right, margin_top;//смещение текста в текстуре (для bottom не указывается, т.к. нижняя граница всегда берется на окончании текста)
	flags32_s flags;

    static const flags32_s::BITS F_DIRTY            = TO_LAST_OPTION << 0;

    GLYPHS & glyphs() { return glyphs_external ? (*glyphs_external) : glyphs_internal; }

public:

    text_rect_c() : margin_left(0), margin_right(0), margin_top(0), scroll_top(0), default_color(ARGB(0, 0, 0)), default_font(g_default_text_font), size(0), lastdrawsize(0) {}
	~text_rect_c();
    
    void use_external_glyphs( GLYPHS *eglyphs )
    {
        glyphs_external = eglyphs;
    }

    void set_def_color( TSCOLOR c ) { if (default_color != c) { flags.set(F_DIRTY); default_color = c; } }
    void set_size(const ts::ivec2 &sz) { if (size != sz) { flags.set(F_DIRTY); size = sz; } }
    void set_text_only(const wstr_c &text) { text_ = text; flags.set(F_DIRTY); }
	bool set_text(const wstr_c &text, bool do_parse_and_render_texture = true);
	const wstr_c& get_text() const { return text_; }
    void set_options(ts::flags32_s nf)
    {
        if ((flags.__bits & (TO_LAST_OPTION - 1)) != (nf.__bits & (TO_LAST_OPTION - 1)))
        {
            flags.__bits &= ~(TO_LAST_OPTION - 1);
            flags.__bits |= nf.__bits & (TO_LAST_OPTION - 1);
            flags.set(F_DIRTY);
        }
    }
	void set_font(const asptr& font) 
    {
        if (default_font.assign(font, false))
        {
            default_font.update(flags.is(ts::TO_NOT_SCALE_FONT) ? 0 : 100);
            flags.set(F_DIRTY);
        }
    }
	void parse_and_render_texture( rectangle_update_s * updr, bool do_render = true );
    void render_texture( rectangle_update_s * updr, fastdelegate::FastDelegate< void (drawable_bitmap_c&) > clearp ); // custom clear
    void render_texture( rectangle_update_s * updr );

	int  scrollTop() const {return scroll_top;}
	//void scrollTo(int y) {scrollTop_ = y; render_texture();}
	void disableFontScaling(bool f = true) 
    { 
        if (flags.is(ts::TO_NOT_SCALE_FONT) != f)
        {
            flags.set(F_DIRTY);
            flags.init(ts::TO_NOT_SCALE_FONT, f);
            default_font.update(flags.is(ts::TO_NOT_SCALE_FONT) ? 0 : 100);
        }
    }

    drawable_bitmap_c &get_texture() {return texture;};

	//Рисует глифы в область памяти RGBA-изображения dst с полным блендингом.
	//Параметр offset задает смещение, на которое сдвигается каждый глиф перед отрисовкой и может использоваться для скроллинга, а также для margin.
	//pitch - смещение в байтах для перехода к следующей строке буфера изображения, обычно = width * 4.
	static bool draw_glyphs(uint8 *dst, int width, int height, int pitch, const array_wrapper_c<const glyph_image_s> &glyphs, ivec2 offset = ivec2(0), bool prior_clear = true);

    ivec2 calc_text_size( font_desc_c& font, const wstr_c&text, int maxwidth, uint flags ); // uses glyphs array! be careful
};

} // namespace ts

