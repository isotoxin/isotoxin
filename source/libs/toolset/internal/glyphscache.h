/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

namespace ts
{

template <class T> T ui_scale(const T &v) //масштабирует входное значение в соответствии с глобальным масштабом
{
	return v; //T((v * globalScale + sign(v) * 50) / 100);
}


//Glyphs Cache - обобщенное название специального набора кэшей, которые очищаются только явно
//и которые используются для быстрого получения параметров и изображений глифов.

struct glyph_s
{
	int advance,//горизонтальное смещение (в пикселях) "пера" для перехода к следующему символу, другими словами, это длина символа
		top,	//расстояние от базовой линии до верхней границы изображения глифа
		left,	//горизонтальное расстояние от положения "пера" до начала изображения глифа
		width,	//ширина изображения глифа, всегда совпадает с pitch
		height,	//высота изображения глифа
		char_index;//индекс символа, полученный вызовом FT_Get_Char_Index
	uint8 *outlined;
	void get_outlined_glyph(struct glyph_image_s &gi, class font_c *font, const ivec2 &pos, TSCOLOR outlineColor);
	//дальше идет само изображение глифа размером width*height байт
};

struct scaled_image_s
{
	static scaled_image_s *load(const wsptr &fileName, const ivec2 &scale);
	int width, height, pitch;
	const uint8 *pixels;//отмасштабированное RGBA-изображение
};

struct font_params_s
{
	ivec2 size;
	int flags;
	int additional_line_spacing;
	float outline_radius, outline_shift;
    font_params_s(): size(0), flags(0), additional_line_spacing(0), outline_radius(0), outline_shift(0) {}
    font_params_s( const ivec2 &size, int flags, int additional_line_spacing, float outline_radius, float outline_shift ):
        size(size), flags(flags), additional_line_spacing(additional_line_spacing), outline_radius(outline_radius), outline_shift(outline_shift) {}
};

typedef struct FT_FaceRec_*  FT_Face;

class font_c //"параметризованный" шрифт с заданным размером и др. параметрами
{
    str_c fontname;
	FT_Face face;
	glyph_s *glyphs[0xFFFF];//это собственно, и есть кэш глифов

public:
	font_params_s font_params;
	int ascender;   // максимальная высота символа относительно базовой линии (можно использовать как вертикальное положение "пера" для первой строки текста)
	int descender;  // нижняя граница символов относительно базовой линии (можно использовать для определения высоты требуемой для хранения текста текстуры)
	int height;     // расстояние между строк (baseline-to-baseline distance)
	int ulinePos;   // положение линии для подчеркивания символов относительно базовой линии (>0, если линия подчеркивания выше базовой линии)
	float ulineThickness;//толщина этой линии

	~font_c();
	glyph_s &operator[](wchar_t c);
	static font_c &buildfont(const wstr_c &filename, const str_c &fontname, ivec2 size, bool hinting = true, int additional_line_spacing = 0, float outline_radius = .2f, float outline_shift = 0);
	int kerning_ci(int left, int right);
	int kerning(wchar_t left, wchar_t right) { return kerning_ci(operator[](left).char_index, operator[](right).char_index); }

    str_c makename_bold();
    str_c makename_light();
    str_c makename_italic();
};

struct font_alias_s {str_c file; font_params_s fp;};//or struct FontInfo

class font_desc_c
{
	font_c *font;
	str_c params, fontname;
    wstr_c filename;
    font_params_s fp;
	int fontsig;
	font_c *get_font() const;

public:
	font_desc_c() : font(nullptr) {}
	font_desc_c(const asptr &params) : font(nullptr) { assign(params); }

	bool assign(const asptr &params, bool andUpdate = true);
	void update(int scale = 100);

	operator font_c * () const {return get_font();}
	font_c *operator->() const {return get_font();}
};

void add_font(const asptr &name, const asptr &fdata);
void load_fonts(abp_c &bp);
void set_fonts_dir(const wsptr&dir);
void set_images_dir(const wsptr&dir); // для парсера
void add_image(const wsptr&name, const bitmap_c&bmp, const irect& rect);
void add_image(const wsptr&name, const uint8* data, const imgdesc_s &imgdesc, bool copyin = true); // set external bitmap data. it must be 4 byte-per-pixel RGBA
void clear_glyphs_cache();//функция очистки всего кэша глифов, вызывается напр. при изменении масштаба интерфейса

} // namespace ts
