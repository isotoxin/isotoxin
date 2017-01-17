/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

#define LCD_RENDER_MODE 0 // experimental, still bad looks

struct FT_FaceRec_;
typedef FT_FaceRec_*  FT_Face;

namespace ts
{

template <class T> T ui_scale(const T &v) // global scale. not used
{
	return v; //T((v * global_scale + sign(v) * 50) / 100);
}

struct glyph_s
{
	int advance,    // width of symbol in pixels
		top,	    // distance from baseline to top edge of glyph image
		left,	    // horizontal distance from pen to glyph image start
		width,	    // glyph image width
		height,	    // glyph image height
		char_index; // symbol index by FT_Get_Char_Index
	uint8 *outlined;
	void get_outlined_glyph(struct glyph_image_s &gi, class font_c *font, const ivec2 &pos, TSCOLOR outlineColor);
	// next bytes is glyph image with width*height size
};

struct scaled_image_s
{
	static scaled_image_s *load(const wsptr &fileName, const ivec2 &scale);
	int width, height, pitch;
	const uint8 *pixels; // scaled RGBA-image
};

struct font_params_s //-V690
{
    str_c fontname;
    wstr_c filename; // .ttf
	ivec2 size;
	int flags;
	int additional_line_spacing;
	float outline_radius, outline_shift;
    bool dirty = true;
    font_params_s(): size(0), flags(0), additional_line_spacing(0), outline_radius(0), outline_shift(0) {}
    font_params_s( const ivec2 &size, int flags, int additional_line_spacing, float outline_radius, float outline_shift ):
        size(size), flags(flags), additional_line_spacing(additional_line_spacing), outline_radius(outline_radius), outline_shift(outline_shift) {}
    font_params_s &operator=(const font_params_s&ofp)
    {
        if (*this != ofp)
        {
            fontname = ofp.fontname;
            filename = ofp.filename;
            size = ofp.size;
            flags = ofp.flags;
            additional_line_spacing = ofp.additional_line_spacing;
            outline_radius = ofp.outline_radius;
            outline_shift = ofp.outline_shift;
            dirty = true;
        }
        return *this;
    }
    bool operator!=(const font_params_s &ofp) const
    {
        return fontname != ofp.fontname || filename != ofp.filename || size != ofp.size || flags != ofp.flags || additional_line_spacing != ofp.additional_line_spacing
            || outline_radius != ofp.outline_radius || outline_shift != ofp.outline_shift; //-V550
    }

    bool operator==(const font_params_s &ofp) const
    {
        return fontname == ofp.fontname && filename == ofp.filename && size == ofp.size && flags == ofp.flags && additional_line_spacing == ofp.additional_line_spacing
            && outline_radius == ofp.outline_radius && outline_shift == ofp.outline_shift; //-V550
    }


    void setup(const asptr &sparams); // returns font filename
};

class font_c // "parametrized" font
{
    str_c fontname; // default or default.bold ... etc
	FT_Face face;
	glyph_s *glyphs[0xFFFF]; // cache itself

public:
	font_params_s font_params;
	int ascender;           // maximum height relative to baseline
	int descender;          // bottom edge relative to baseline
	int height;             // distance between lines (baseline-to-baseline distance)
	int underline_add_y;    // addition Y for underline relative to baseline (>0, if underline above baseline)
	float uline_thickness;  // underline thickness

	~font_c();
	glyph_s &operator[](wchar c);
	static font_c &buildfont(const str_c &fontname, const font_params_s&fp);
	int kerning_ci(int left, int right);
	int kerning( wchar left, wchar right) { return kerning_ci(operator[](left).char_index, operator[](right).char_index); }

    str_c makename_bold();
    str_c makename_light();
    str_c makename_italic();
};

class font_desc_c
{
	font_c *font = nullptr;
	str_c fontname;
    font_params_s fp;
	int fontsig = 0;
	font_c *get_font() const;

public:
	font_desc_c() {}
	//explicit font_desc_c(const asptr &params) : font(nullptr) { assign(params); }
    const str_c &name() const {return fontname;}
	bool assign(const str_c &fontname);
	void update();

    int height() const {return font ? font->height : 0;}
    int fheight() const { return font ? (font->ascender-font->descender) : 0; }

	operator font_c * () const {return get_font();}
	font_c *operator->() const {return get_font();}
};

void add_font(const asptr &name, const font_params_s &fprms);

void set_fonts_dir(const wsptr&dir, bool add = false);
void set_images_dir(const wsptr&dir, bool add = false); // for parser
void add_image(const wsptr&name, const bitmap_c&bmp, const irect& rect);
void add_image(const wsptr&name, const uint8* data, const imgdesc_s &imgdesc, bool copyin = true); // set external bitmap data. it must be 4 byte-per-pixel RGBA
bmpcore_exbody_s get_image(const wsptr&name);
void clear_glyphs_cache();

blob_c load_image( const wsptr&fn ); // try load image from one of image-paths

} // namespace ts
