#include "toolset.h"
#include "glyphscache.h"

namespace ts
{

font_c::~font_c()
{
	for (int i=0; i<LENGTH(glyphs); i++)
		if (glyphs[i])
		{
			if (glyphs[i]->outlined) MM_FREE(glyphs[i]->outlined);
			MM_FREE(glyphs[i]);
		}
}

glyph_s &font_c::operator[](wchar_t c)
{
	if (glyphs[c]) return *glyphs[c];

	FT_Set_Pixel_Sizes( face, font_params.size.x, font_params.size.y );//можно было б запоминать пред. установленный размер чтобы не каждый раз вызывать эту ф-ю, но это оч. быстрая операция (по сравнению с FT_Load_Char)
	CHECK(FT_Load_Char( face, c, font_params.flags | FT_LOAD_RENDER ) == 0);

	FT_Bitmap &b = face->glyph->bitmap;

	//some checks
	ASSERT(b.num_grays == 256 && b.pixel_mode == FT_PIXEL_MODE_GRAY);
	ASSERT(face->glyph->format == FT_GLYPH_FORMAT_BITMAP);
	ASSERT(b.pitch == b.width);//?

	glyphs[c] = (glyph_s*)MM_ALLOC(sizeof(glyph_s) + b.width*b.rows);

	//fill glyph fields
	glyphs[c]->left	   = face->glyph->bitmap_left;
	glyphs[c]->top	   = face->glyph->bitmap_top;
	glyphs[c]->advance = (face->glyph->advance.x + 32) >> 6; // +32 нужно для округления к целому числу пикселей, т.к. glyph->advance хранится в формате 26.6
	glyphs[c]->width   = b.width;
	glyphs[c]->height  = b.rows;
	glyphs[c]->char_index = FT_Get_Char_Index(face, c);
	glyphs[c]->outlined = NULL;

	//copy bitmap data
	char *src = (char*)b.buffer;
	char *dst = (char*)(glyphs[c]+1);
	for (int row=0; row<b.rows; row++, src+=b.pitch, dst+=b.width)//pitch may be negative, so memcpy can't be used here
		memcpy(dst, src, b.width);//but can be used to copy a while row

	return *glyphs[c];
}


static bool operator==(const font_params_s &f1,const font_params_s &f2)
{ return memcmp(&f1, &f2, sizeof(font_params_s)) == 0; }
unsigned GetHash(const font_params_s &f) {return GetHash(&f, sizeof(f));}

str_c font_c::makename_bold()
{
    token<char> t( fontname, '.' );
    pstr_c rawfn = *t;
    ++t;
    bool itl = false;
    for( ;t; ++t)
        if ( t->equals(CONSTASTR("italic")) ) itl = true;

    str_c r( rawfn, CONSTASTR(".bold") ); if (itl) r.append(CONSTASTR(".italic"));
    return r;
}
str_c font_c::makename_light()
{
    token<char> t(fontname, '.');
    pstr_c rawfn = *t;
    ++t;
    bool itl = false;
    for (; t; ++t)
        if (t->equals(CONSTASTR("italic"))) itl = true;

    str_c r(rawfn, CONSTASTR(".light"));
    if (itl) r.append(CONSTASTR(".italic"));
    return r;
}
str_c font_c::makename_italic()
{
    token<char> t(fontname, '.');
    pstr_c rawfn = *t;
    ++t;
    bool bld = false;
    bool lit = false;
    for (; t; ++t)
    {
        if (t->equals(CONSTASTR("bold")) ) bld = true; else
        if (t->equals(CONSTASTR("light"))) lit = true;
    }

    if (bld)
        return str_c(rawfn, CONSTASTR(".bold.italic"));
    if (lit)
        return str_c(rawfn, CONSTASTR(".light.italic"));

    return str_c(rawfn, CONSTASTR(".italic"));
}

namespace
{

    struct font_face_s//базовый класс контейнер шрифта (в FreeType - это face)
    {
	    FT_Face face;
	    blob_c fontFileBuffer;//по требованию FreeType нельзя освобождать буфер с файлом шрифта до вызова FT_Done_Face, поэтому храним его здесь
	    hashmap_t<font_params_s, font_c> fontsCache;

	    ~font_face_s()
	    {
		    FT_Done_Face( face );
	    }
    };

    struct scaled_image_key_s
    {
        wstr_c name;
        ivec2 scale;
        bool operator==(const scaled_image_key_s &s) const { return name == s.name && scale == s.scale; }
    };
    static unsigned GetHash(const scaled_image_key_s &i)
    {
        return GetHash(i.name) ^ GetHash(&i.scale, sizeof(i.scale));
    }
    struct scaled_image_container_s : scaled_image_s
    {
        bitmap_c bitmap;
    };

    struct internal_data_s
    {
        FT_Library ftlibrary;
        hashmap_t<wstr_c, font_face_s> font_faces_cache;
	    hashmap_t<scaled_image_key_s, scaled_image_container_s> scaled_images_cache;
	    hashmap_t<str_c, font_alias_s> fonts;
	    wstr_c fonts_dir;
	    wstr_c images_dir;
	    int font_cache_sig;
	    internal_data_s() :font_cache_sig(0)
        {
            //FreeType
            FT_Init_FreeType(&ftlibrary);
        }
        ~internal_data_s()
        {
            // clear hash tables before kill FreeType
            font_faces_cache.clear();
            scaled_images_cache.clear();
            fonts.clear();
            FT_Done_FreeType( ftlibrary );
        }
    };
}

static_setup<internal_data_s> idata;

void set_fonts_dir(const wsptr &dir)
{
	idata().fonts_dir = dir;
}
void set_images_dir(const wsptr &dir) // для парсера
{
	idata().images_dir = dir;
    if (idata().images_dir.get_last_char() != '\\' && idata().images_dir.get_last_char() != '/')
        idata().images_dir.append_char('\\');
}
void add_image(const wsptr&name, const bitmap_c&bmp, const irect& rect)
{
    bool added;
    scaled_image_key_s sik = {name, ivec2(100)};
    scaled_image_container_s &i = idata().scaled_images_cache.add(sik, added);
    i.bitmap = bmp;
    i.width = rect.width();
    i.height = rect.height();
    i.pitch = bmp.info().pitch;
    i.pixels = bmp.body( rect.lt );
}

void add_image(const wsptr&name, const uint8* data, const imgdesc_s &imgdesc, bool copyin)
{
    bool added;
    scaled_image_key_s sik = { name, ivec2(100) };
    scaled_image_container_s &i = idata().scaled_images_cache.add(sik, added);

    ASSERT(imgdesc.bytepp() == 4);

    if (copyin)
    {
        i.bitmap.create_RGBA(imgdesc.sz);
        img_helper_copy(i.bitmap.body(),data, i.bitmap.info(), imgdesc);

        i.pitch = i.bitmap.info().pitch;
        i.pixels = i.bitmap.body();
    } else
    {
        i.bitmap.clear();
        i.pitch = imgdesc.pitch;
        i.pixels = data;
    }

    i.width = imgdesc.sz.x;
    i.height = imgdesc.sz.y;
}

font_c &font_c::buildfont(const wstr_c &filename, const str_c &fontname, ivec2 size, bool hinting, int additional_line_spacing, float outline_radius, float outline_shift)
{
    internal_data_s &idta = idata();
    wstr_c face(filename);
	//face.makeLowerCase();
	if (face.find_pos('.') < 0) face.append(CONSTWSTR(".otf"));
	bool added;
	font_face_s &ff = idta.font_faces_cache.add(face, added);
	if (added)//если только что добавили, выполняем инициализацию
	{
		blob_c buf = g_fileop->load(fn_join(idta.fonts_dir, face));
		while (!buf)//если не смогли загрузить, берем просто первый попавшийся шрифт
		{
            swstr_t<MAX_PATH+32> sysdir(MAX_PATH,false);
            GetWindowsDirectoryW(sysdir.str(), MAX_PATH); sysdir.set_length();
            if (sysdir.get_last_char() != '\\') sysdir.append_char('\\');
            sysdir.append(CONSTWSTR("fonts\\"));
            sysdir.append(face);
            buf = g_fileop->load(sysdir);
            if (buf) break;

            ERROR("Font '%s' not found. Check path! Current path is: %s", face.cstr(), idta.fonts_dir.cstr());

			auto it = idta.font_faces_cache.begin();
			if (it.key() == face) it++;//на случай когда первым в списке идет только что добавленный незагруженный шрифт
			buf = it->fontFileBuffer;
            break;
		}

		CHECK(FT_New_Memory_Face( idta.ftlibrary, (FT_Byte*)buf.data(), buf.size(), 0, &ff.face ) == 0);
		ff.fontFileBuffer = buf;//ref++ чтобы буфер с шрифтом не удалился

		//some checks
		ASSERT(FT_IS_SCALABLE(ff.face));
	}

	font_params_s fp(size, hinting ? 0 : FT_LOAD_NO_HINTING, additional_line_spacing, outline_radius, outline_shift);
	font_c &f = ff.fontsCache.add(fp, added);
	if (added)//заполняем параметры шрифта
	{
        f.fontname = fontname;
		f.face = ff.face;
		f.font_params = fp;

		//FT_Set_Pixel_Sizes( f.face, size.x, size.y );//устанавливаем размер, чтобы проинициализировались отмасштабированные значения метрик
		//f.ascender  = (f.face->size->metrics.ascender  + 32) >> 6;
		//f.descender = (f.face->size->metrics.descender + 32) >> 6;
		//f.height    = (f.face->size->metrics.height    + 32) >> 6;
		float design_to_device_K = (float)size.y/f.face->units_per_EM;//коэф-т перевода из системы измерения шрифта (именно в ней хранятся глобальные метрики) к пикселям
		f.ascender  =  lceil(design_to_device_K * f.face->ascender);
		f.descender = lround(design_to_device_K * f.face->descender);
		f.height    = lround(design_to_device_K * f.face->height);
		f.ulinePos  = lround(design_to_device_K * f.face->underline_position);
		f.ulineThickness = float(design_to_device_K * f.face->underline_thickness);

		memset(f.glyphs, 0, sizeof(f.glyphs));
	}

	return f;
}

int font_c::kerning_ci(int left, int right)
{
	if (!FT_HAS_KERNING(face)) return 0;

	FT_Vector delta;
	/*//эта простейшая реализация была заменена оптимизированной версией ниже
	FT_Set_Pixel_Sizes( face, fontParams.size.x, fontParams.size.y );
	FT_Get_Kerning( face, FT_Get_Char_Index(face, left), FT_Get_Char_Index(face, right), FT_KERNING_DEFAULT, &delta );
	return delta.x >> 6;*/
	FT_Get_Kerning( face, left, right, FT_KERNING_UNSCALED, &delta );
	return lround((float)font_params.size.x/face->units_per_EM * delta.x);
}

scaled_image_s *scaled_image_s::load(const wsptr &filename_, const ivec2 &scale)
{
	scaled_image_key_s sik = {filename_, scale};
	bool added;
	scaled_image_container_s &i = idata().scaled_images_cache.add(sik, added);
	if (added)
	{
		i.width = i.height = i.pitch = 0; //инициализация на случай ошибки
		do
		{
            tmp_wstr_c fn;
            if (filename_.s[0] == '/') fn.set(filename_.skip(1));
            else fn.set(idata().images_dir.as_sptr()).append(filename_);
            if (fn.find_pos('.') < 0) fn.append(CONSTWSTR(".png"));
            blob_c buf = g_fileop->load(fn);
			if (buf && i.bitmap.load_from_PNG(buf.data(), buf.size()))
            {
				switch (i.bitmap.info().bitpp)
				{
				case 24:
                    {
                        bitmap_c tmp;
                        i.bitmap.convert_24to32(tmp);
                        i.bitmap = tmp;
                    }
				case 32:
					i.width  = tmax(1, (i.bitmap.info().sz.x * scale.x + 50) / 100);//способ расчета должен быть такой же как в ui.scale
					i.height = tmax(1, (i.bitmap.info().sz.y * scale.y + 50) / 100);
					if (i.width != i.bitmap.info().sz.x || i.height != i.bitmap.info().sz.y)
					{
						bitmap_c temp;
						i.bitmap.resize(temp, ref_cast<ivec2>(i.width, i.height), FILTER_LANCZOS3);
						i.bitmap = temp;
					}
                    i.bitmap.premultiply();
                    i.pitch = i.bitmap.info().pitch;
					i.pixels = i.bitmap.body();
					break;
				default:
					WARNING("Unsupported format for scaled image '%s' : %i bpp", tmp_str_c(filename_).cstr(), i.bitmap.info().bitpp);
					break;
				}
				break;//ok
			}
			WARNING("Can't load scaled image '%s'", to_str(filename_).cstr());
		} while (false);
	}
	return &i;
}

static void add_font_internal(const asptr &name, const asptr &fdata)
{

    font_alias_s &fa = idata().fonts.add(name);
    token<char> t(fdata, ',');
    fa.file = *t;

    ++t; token<char> tt(*t, '/');

    fa.fp.size.x = tt->as_int();
    ++tt; fa.fp.size.y = tt->as_int(fa.fp.size.x);
    ++t; fa.fp.flags = t ? t->as_int(1) : 1;
    ++t; fa.fp.additional_line_spacing = t ? t->as_int() : 0;
    ++t; fa.fp.outline_radius = t ? t->as_float(.2f) : .2f;
    ++t; fa.fp.outline_shift = t ? tmin(t->as_float(.99f), .99f) : 0;
}


void add_font(const asptr &name, const asptr &fdata)
{
    add_font_internal(name,fdata);
}

void load_fonts( const abp_c &bp )
{
	idata().fonts.clear();

	for (auto bpr = bp.begin(); bpr; ++bpr)
        add_font_internal(bpr.name(), bpr->as_string().as_sptr());
}

font_c *font_desc_c::get_font() const
{
	if (font && !CHECK(fontsig == idata().font_cache_sig))
		const_cast<font_desc_c*>(this)->update();
    if(!CHECK(font))
    {
        if (this == &g_default_text_font) __debugbreak();
        return g_default_text_font.get_font();
    }
	return font;
}

bool font_desc_c::assign(const asptr &iparams, bool andUpdate)
{
    if (params.equals(iparams)) return false;
    params.set(iparams);
	
    token<char> t(params,',');
    fontname = *t;
	font_alias_s *fa = idata().fonts.get(fontname);
	if (!CHECK(fa, "Font alias not found: " << (*t)))
    {
        fontname = CONSTASTR("default");
        fa = idata().fonts.get(fontname);
    }
	filename = fa->file;
	fp = fa->fp;
    
    ++t;
	if (t)
	{
        token<char> tt(*t,'/');

		fp.size.x = tt->as_int();
        ++tt; fp.size.y = tt->as_int(fp.size.x);
		fp.additional_line_spacing = (fp.additional_line_spacing * fp.size.y + 50) / 100;
		fp.size = (fa->fp.size * fp.size + 50) / 100;
	}
	if (andUpdate) update();

    return true;
}

void font_desc_c::update(int scale)
{
	font = &font_c::buildfont(filename, fontname, scale ? (fp.size * (ui_scale(100) * scale/* + 50*/) + 5000) / 10000 : fp.size, fp.flags != 0, ui_scale(fp.additional_line_spacing), fp.outline_radius, fp.outline_shift);
	fontsig = idata().font_cache_sig;
}


void clear_glyphs_cache()
{
	for (auto it = idata().font_faces_cache.begin(); it; ++it)
		it->fontsCache.clear();

	idata().scaled_images_cache.clear(); // also clear images cache
	idata().font_cache_sig++; // increment sig to invalidate all exist font_desc_c (avoid access to broken pointer to font)
}

} // namespace ts
