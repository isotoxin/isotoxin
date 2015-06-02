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

class text_rect_c // texture with text
{
    TSCOLOR default_color = ARGB(0, 0, 0);
    wstr_c text;
    flags32_s flags;
    int scroll_top = 0;
    int margin_left = 0, margin_right = 0, margin_top = 0; // offset of text in texture
    int text_height;
public:
	const font_desc_c *font;
    ivec2 size;
    ivec2 lastdrawsize;
	drawable_bitmap_c texture;
	GLYPHS  glyphs_internal;
    //GLYPHS *glyphs_external = nullptr;

    static const flags32_s::BITS F_DIRTY            = TO_LAST_OPTION << 0;
    static const flags32_s::BITS F_INVALID_SIZE     = TO_LAST_OPTION << 1;
    static const flags32_s::BITS F_INVALID_TEXTURE  = TO_LAST_OPTION << 2;
    static const flags32_s::BITS F_INVALID_GLYPHS   = TO_LAST_OPTION << 3;

    //GLYPHS & glyphs() { return glyphs_external ? (*glyphs_external) : glyphs_internal; }
    GLYPHS & glyphs() { return glyphs_internal; }
    const GLYPHS & glyphs() const { return glyphs_internal; }
    void update_rectangles( ts::ivec2 &offset, rectangle_update_s * updr ); // internal

public:

    text_rect_c() : font(&g_default_text_font), size(0), lastdrawsize(0) { flags.setup(F_INVALID_SIZE|F_INVALID_TEXTURE|F_INVALID_GLYPHS); }
	~text_rect_c();
    
    void make_dirty( bool dirty_common = true, bool dirty_glyphs = true, bool dirty_size = false )
    {
        if (dirty_common) flags.set(F_DIRTY);
        if (dirty_glyphs) flags.set(F_INVALID_GLYPHS);
        if (dirty_size) flags.set(F_INVALID_SIZE);
    }
    bool is_dirty() const { return flags.is(F_DIRTY); }
    bool is_dirty_size() const { return flags.is(F_INVALID_SIZE); }
    bool is_dirty_glyphs() const { return flags.is(F_INVALID_GLYPHS); }
    
    void set_margins(int mleft, int mtop, int mrite)
    {
        if (mleft != margin_left) { flags.set(F_DIRTY|F_INVALID_TEXTURE|F_INVALID_GLYPHS); margin_left = mleft; }
        if (mtop != margin_top) { flags.set(F_DIRTY|F_INVALID_TEXTURE|F_INVALID_GLYPHS); margin_top = mtop; }
        if (mrite != margin_right) { flags.set(F_DIRTY|F_INVALID_TEXTURE|F_INVALID_GLYPHS); margin_right = mrite; }
    }
    void set_def_color( TSCOLOR c ) { if (default_color != c) { flags.set(F_DIRTY|F_INVALID_TEXTURE|F_INVALID_GLYPHS); default_color = c; } }
    void set_size(const ts::ivec2 &sz) { flags.init(F_INVALID_SIZE, !(sz >> 0)); if (size != sz) { flags.set(F_DIRTY|F_INVALID_TEXTURE|F_INVALID_GLYPHS); size = sz; } }
    void set_text_only(const wstr_c &text_, bool forcedirty) { if (forcedirty || !text.equals(text_)) { flags.set(F_DIRTY|F_INVALID_SIZE|F_INVALID_TEXTURE|F_INVALID_GLYPHS); text = text_; } }
	bool set_text(const wstr_c &text, CUSTOM_TAG_PARSER ctp, bool do_parse_and_render_texture);
	const wstr_c& get_text() const { return text; }
    void set_options(ts::flags32_s nf)
    {
        if ((flags.__bits & (TO_LAST_OPTION - 1)) != (nf.__bits & (TO_LAST_OPTION - 1)))
        {
            flags.__bits &= ~(TO_LAST_OPTION - 1);
            flags.__bits |= nf.__bits & (TO_LAST_OPTION - 1);
            flags.set(F_DIRTY);
        }
    }
	bool set_font(const font_desc_c *fd) 
    {
        if (fd == nullptr) fd = &g_default_text_font;
        if (fd != font)
        {
            font = fd;
            flags.set(F_DIRTY|F_INVALID_TEXTURE|F_INVALID_GLYPHS);
            return true;
        }
        return false;
    }
	void parse_and_render_texture( rectangle_update_s * updr, CUSTOM_TAG_PARSER ctp, bool do_render = true );
    void render_texture( rectangle_update_s * updr, fastdelegate::FastDelegate< void (drawable_bitmap_c&) > clearp ); // custom clear
    void render_texture( rectangle_update_s * updr );
    void update_rectangles( rectangle_update_s * updr );

    ivec2 calc_text_size(int maxwidth, CUSTOM_TAG_PARSER ctp) const; // also it renders texture
    ivec2 calc_text_size(const font_desc_c& font, const wstr_c&text, int maxwidth, uint flags, CUSTOM_TAG_PARSER ctp) const;

	int  get_scroll_top() const {return scroll_top;}
	//void scrollTo(int y) {scrollTop_ = y; render_texture();}

    drawable_bitmap_c &get_texture() { ASSERT(!flags.is(F_INVALID_TEXTURE|F_INVALID_GLYPHS)); return texture;};

	//Рисует глифы в область памяти RGBA-изображения dst с полным блендингом.
	//Параметр offset задает смещение, на которое сдвигается каждый глиф перед отрисовкой и может использоваться для скроллинга, а также для margin.
	//pitch - смещение в байтах для перехода к следующей строке буфера изображения, обычно = width * 4.
	static bool draw_glyphs(uint8 *dst, int width, int height, int pitch, const array_wrapper_c<const glyph_image_s> &glyphs, ivec2 offset = ivec2(0), bool prior_clear = true);

};

} // namespace ts

