#include "isotoxin.h"
#include "pngquant/lib/libimagequant.h"
#include "pnglib/src/png.h"

#pragma comment(lib, "pngquant.lib")
#pragma comment(lib, "pnglib.lib")

DWORD WINAPI dialog_avaselector_c::worker(LPVOID ap)
{
    UNSTABLE_CODE_PROLOG
        ((dialog_avaselector_c *)ap)->compressor();
    UNSTABLE_CODE_EPILOG
    return 0;
}

namespace
{
    struct png8_image_s
    {
        int width;
        int height;
        ts::uint8 **row_pointers = nullptr;
        ts::uint8 *indexed_data = nullptr;
        int num_palette;
        int num_trans;
        png_color palette[256];
        ts::uint8 trans[256];

        ts::buf_c outbuf;

        ~png8_image_s()
        {
            MM_FREE(row_pointers);
            MM_FREE(indexed_data);
        }
    };

    static void rwpng_error_handler(png_structp png_ptr, png_const_charp msg)
    {}

    static int rwpng_write_image_init(png8_image_s &png8info, png_structpp png_ptr_p, png_infopp info_ptr_p)
    {
        *png_ptr_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, &png8info, rwpng_error_handler, nullptr);

        if (!(*png_ptr_p)) {
            return -1;   /* out of memory */
        }

        *info_ptr_p = png_create_info_struct(*png_ptr_p);
        if (!(*info_ptr_p)) {
            png_destroy_write_struct(png_ptr_p, nullptr);
            return -1;   /* out of memory */
        }

        //png_set_bgr(*png_ptr_p);
        png_set_compression_level(*png_ptr_p, Z_BEST_COMPRESSION);
        png_set_compression_mem_level(*png_ptr_p, 5); // judging by optipng results, smaller mem makes libpng compress slightly better

        return 0;
    }

    static void user_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
    {
        png8_image_s *write_state = (png8_image_s *)png_get_io_ptr(png_ptr);
        write_state->outbuf.append_buf(data, length);
    }

    static void user_flush_data(png_structp png_ptr)
    {
        // libpng never calls this :(
    }

    static void rwpng_write_end(png_infopp info_ptr_p, png_structpp png_ptr_p, png_bytepp row_pointers)
    {
        png_write_info(*png_ptr_p, *info_ptr_p);
        png_set_packing(*png_ptr_p);
        png_write_image(*png_ptr_p, row_pointers);
        png_write_end(*png_ptr_p, nullptr);
        png_destroy_write_struct(png_ptr_p, info_ptr_p);
    }

    static int rwpng_write_image8(png8_image_s &png8info)
    {
        png_structp png_ptr;
        png_infop info_ptr;

        int retval = rwpng_write_image_init(png8info, &png_ptr, &info_ptr);
        if (retval < 0) return retval;

        png_set_write_fn(png_ptr, &png8info, user_write_data, user_flush_data);

        // Palette images generally don't gain anything from filtering
        png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_VALUE_NONE);

        int sample_depth = 8;
        if (png8info.num_palette <= 2)
            sample_depth = 1;
        else if (png8info.num_palette <= 4)
            sample_depth = 2;
        else if (png8info.num_palette <= 16)
            sample_depth = 4;

        png_set_IHDR(png_ptr, info_ptr, png8info.width, png8info.height, sample_depth, PNG_COLOR_TYPE_PALETTE, 0, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);
        png_set_PLTE(png_ptr, info_ptr, &png8info.palette[0], png8info.num_palette);

        if (png8info.num_trans > 0)
            png_set_tRNS(png_ptr, info_ptr, png8info.trans, png8info.num_trans, nullptr);

        rwpng_write_end(&info_ptr, &png_ptr, png8info.row_pointers);

        return 0;
    }

    static int prepare_output_image(liq_result *result, liq_image *input_image, png8_image_s &output_image)
    {
        output_image.width = liq_image_get_width(input_image);
        output_image.height = liq_image_get_height(input_image);

        /*
        ** Step 3.7 [GRR]: allocate memory for the entire indexed image
        */

        output_image.indexed_data = (ts::uint8 *)MM_ALLOC(output_image.height * output_image.width);
        output_image.row_pointers = (ts::uint8 **)MM_ALLOC(output_image.height * sizeof(output_image.row_pointers[0]));

        if (!output_image.indexed_data || !output_image.row_pointers) {
            return -1;
        }

        for(int row = 0;  row < output_image.height;  ++row) {
            output_image.row_pointers[row] = output_image.indexed_data + row*output_image.width;
        }

        const liq_palette *palette = liq_get_palette(result);
        // tRNS, etc.
        output_image.num_palette = palette->count;
        output_image.num_trans = 0;
        for(unsigned int i=0; i < palette->count; i++)
            if (palette->entries[i].a < 255)
                output_image.num_trans = i+1;

        return 0;
    }

    static void set_palette(liq_result *result, png8_image_s &output_image)
    {
        const liq_palette *palette = liq_get_palette(result);

        // tRNS, etc.
        output_image.num_palette = palette->count;
        output_image.num_trans = 0;
        for (unsigned int i = 0; i < palette->count; i++) {
            liq_color px = palette->entries[i];
            if (px.a < 255) {
                output_image.num_trans = i + 1;
            }
            png_color & pngc = output_image.palette[i];
            pngc.red = px.r;
            pngc.green = px.g;
            pngc.blue = px.b;
            output_image.trans[i] = px.a;
        }
    }
}

static void encode_lossy_png( ts::buf_c &buf, const ts::bitmap_c &bmp )
{
    ts::bitmap_c b2e;
    if (bmp.info().bytepp() != 4 || bmp.info().pitch != bmp.info().sz.x * 4)
    {
        b2e.create_RGBA(b2e.info().sz);
        b2e.copy(ts::ivec2(0), b2e.info().sz, bmp.extbody(), ts::ivec2(0));
    } else
        b2e = bmp;

    b2e.swap_byte(ts::ivec2(0), b2e.info().sz, 0, 2);

    liq_attr *attr = liq_attr_create();
    liq_image *image = liq_image_create_rgba(attr, b2e.body(), b2e.info().sz.x, b2e.info().sz.y, 0);
    liq_result *res = liq_quantize_image(attr, image);

    liq_set_dithering_level(res, 1.0f);

    png8_image_s pngshka;
    prepare_output_image(res, image, pngshka);

    liq_write_remapped_image_rows(res, image, pngshka.row_pointers);
    set_palette(res, pngshka);

    rwpng_write_image8(pngshka);

    liq_attr_destroy(attr);
    liq_image_destroy(image);
    liq_result_destroy(res);

    buf = std::move(pngshka.outbuf);
}

void dialog_avaselector_c::compressor()
{
    ts::tmpalloc_c t;
    sync.lock_write()().compressor_working = true;

    int maximumsize = 16384;

    int sln = 10;
    for(;; Sleep(sln))
    {
        auto w = sync.lock_write();
        if (w().compressor_should_stop) break;

        if (w().bitmap2encode.info().sz >> 0) ; else
        {
            sln = 10;
            continue;
        }

        ts::bitmap_c b2e = w().bitmap2encode;
        w().bitmap2encode.clear();
        int tag = w().source_tag;
        w.unlock();

        ts::buf_c encoded;
        encode_lossy_png(encoded, b2e);

        w = sync.lock_write();
        if (tag != w().source_tag)
        {
            sln = 0;
            continue;
        }
        int sz = encoded.size();
        w().encoded = std::move(encoded);
        if (sz <= maximumsize)
            w().encoded_fit_16kb = w().encoded;

        w.unlock();

        sln = 10;

        if (sz <= maximumsize)
            continue;

        int sz1 = 16;
        int sz2 = ts::tmin(b2e.info().sz.x, b2e.info().sz.y);
        int szc = (sz1 + sz2) / 2;

        int best = sz;
        ts::ivec2 bestsz = b2e.info().sz;

        ts::buf_c e2;
        for (;;)
        {
            if (tag != sync.lock_read()().source_tag)
            {
                sln = 0;
                break;
            }
            ts::ivec2 newsz = b2e.info().sz;
            if (newsz.x < newsz.y)
            {
                newsz.x = szc;
                newsz.y = lround(((float)newsz.x / (float)b2e.info().sz.x) * (float)b2e.info().sz.y);
            } else
            {
                newsz.y = szc;
                newsz.x = lround(((float)newsz.y / (float)b2e.info().sz.y) * (float)b2e.info().sz.x);
            }
            ts::bitmap_c b;
            e2.clear();
            b2e.resize(b, newsz, ts::FILTER_LANCZOS3);
            encode_lossy_png(e2, b);
            sz = e2.size();

            if (sz > maximumsize)
            {
                sz2 = szc;
                szc = (sz1 + sz2) / 2;
                if (szc == sz1 || szc == sz2) goto usebestrslt;
                continue;
            } else
            {
                int delta = maximumsize - sz;
                if (delta < best)
                {
                    best = delta;
                    bestsz = newsz;
                    sz1 = szc;
                    szc = (sz1 + sz2) / 2;
                    if (szc == sz1 || szc == sz2) goto usebestrslt;
                    continue;
                } else
                {
                    usebestrslt:
                    e2.clear();
                    b2e.resize(b, bestsz, ts::FILTER_LANCZOS3);
                    encode_lossy_png(e2, b);
                    sz = e2.size();

                    sync.lock_write()().encoded_fit_16kb = std::move(e2);
                    break;
                }
            }

        }

    }

    sync.lock_write()().compressor_working = false;
}

dialog_avaselector_c::dialog_avaselector_c(initial_rect_data_s &data) :gui_isodialog_c(data), avarect(0) 
{
    CloseHandle(CreateThread(nullptr, 0, worker, this, 0, nullptr));
    while( !sync.lock_read()().compressor_working ) Sleep(1);
}

dialog_avaselector_c::~dialog_avaselector_c()
{
    auto w = sync.lock_write();
    w().compressor_should_stop = true;
    w().source_tag += 1;
    w.unlock();
    if (gui) gui->delete_event(DELEGATE(this,flashavarect));
    while( sync.lock_read()().compressor_working ) Sleep(1);
}

/*virtual*/ void dialog_avaselector_c::created()
{
    set_theme_rect(CONSTASTR("avasel"), false);
    fd.prepare( get_default_text_color(4), get_default_text_color(5) );
    __super::created();
    tabsel( CONSTASTR("1") );

}

/*virtual*/ int dialog_avaselector_c::additions(ts::irect & border)
{

    descmaker dm(descs);
    dm << 1;

    ts::wstr_c openimgbuttonface( TTT("Открыть изображение",212) );
    ts::wstr_c l(CONSTWSTR("<p=l>"));
    ts::wstr_c s(TTT("Поддерживаемые форматы: $", 216) / CONSTWSTR("png, jpg, bmp, tga, dds"));
    s.insert(0,CONSTWSTR("<br>"));

    ts::wstr_c ctl;

    ts::ivec2 bsz(50,25);
    const button_desc_s *b = gui->theme().get_button("save");
    if (b)
        bsz = b->size;

    dm().button(ts::wstr_c(), openimgbuttonface, DELEGATE(this, open_image)).width(200).height(25).subctl(0,ctl);
    dm().button(ts::wstr_c(), b ? L"face=save" : L"save", DELEGATE(this, save_image)).width(bsz.x).height(bsz.y).subctl(1,savebtn);

    dm().page_header(l+(TTT("Откройте изображение одним из способов:[br] 1. Перетащите сюда изображение в поддерживаемом формате[br] 2. Вставьте из буфера обмена (Ctrl+V)[br] 3. Воспользуйтесь кнопкой $",211) / ctl)+s);
    
    
    dm().vspace(1).setname(CONSTASTR("last"));
    return 0;
}

bool dialog_avaselector_c::flashavarect(RID, GUIPARAM)
{
    DELAY_CALL_R(0.13, DELEGATE(this,flashavarect), nullptr);
    ++tickvalue;
    getengine().redraw();
    return true;
}

ts::uint32 dialog_avaselector_c::gm_handler(gmsg<GM_DROPFILES> & p)
{
    if (p.root == getroot())
    {
        load_image(p.fn);
        return GMRBIT_ABORT;
    }
    return 0;
}

ts::uint32 dialog_avaselector_c::gm_handler(gmsg<GM_PASTE_HOTKEY> & p)
{
    ts::bitmap_c bmp = ts::get_clipboard_bitmap();
    if (bmp.info().sz >> 0)
    {
        bitmap = bmp;
        newimage();
    }

    return 0;
}

bool dialog_avaselector_c::save_image(RID, GUIPARAM)
{
    bool savesmall = (GetAsyncKeyState(VK_CONTROL)  & 0x8000)==0x8000;
    ts::wstr_c fromdir;
    if (prf().is_loaded())
        fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());

    ts::wstr_c title(TTT("Сохранить аватар в png-файл",217));

    ts::wstr_c filter(CONSTWSTR("png/*.png/(*.*)/*.*//"));

    ts::wstr_c fn = ts::get_save_filename_dialog(fromdir, CONSTWSTR("avatar.png"), filter, L"png", title);
    if (!fn.is_empty())
    {
        if (savesmall)
            sync.lock_read()().encoded_fit_16kb.save_to_file(fn);
        else
            sync.lock_read()().encoded.save_to_file(fn);
    }
    return true;
}

bool dialog_avaselector_c::open_image(RID, GUIPARAM)
{
    ts::wstr_c fromdir;
    if (prf().is_loaded())
        fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());

    ts::wstr_c title( TTT("Изображение",213) );
    ts::wstr_c filter( CONSTWSTR("<imgs> (png, jpeg, bmp, dds, tga)/*.png;*.jpg;*.jpeg;*.dds;*.bmp;*.tga/<all> (*.*)/*.*//") );
    filter.replace_all(CONSTWSTR("<imgs>"), TTT("Изображения",214));
    filter.replace_all(CONSTWSTR("<all>"), TTT("Любые файлы",215));

    ts::wstr_c fn = ts::get_load_filename_dialog(fromdir, CONSTWSTR(""), filter, L"png", title);
    if (!fn.is_empty())
    {
        load_image(fn);
        if (prf().is_loaded()) prf().last_filedir(ts::fn_get_path(fn));
    }

    return true;
}

void dialog_avaselector_c::newimage()
{
    user_offset = ts::ivec2(0);
    resize_k = 1.0f;
    dirty = true;
    avarect = ts::irect(0, bitmap.info().sz);
    rebuild_bitmap();
    flashavarect(RID(), nullptr);
    recompress();

    if (image.info().sz > viewrect.size())
    {
        statrt_scale();
        do_scale(ts::tmin( (float)viewrect.width() / (float)image.info().sz.x, (float)viewrect.height() / (float)image.info().sz.y ));
    }
}

void dialog_avaselector_c::load_image(const ts::wstr_c &fn)
{
    bitmap.load_from_file(fn);
    newimage();
}

void dialog_avaselector_c::rebuild_bitmap()
{
    if (bitmap.info().sz >> 0)
    {
        ts::bitmap_c b;
        if (bitmap.info().bytepp() != 4)
        {
            b.create_RGBA(bitmap.info().sz);
            b.copy(ts::ivec2(0), bitmap.info().sz, bitmap.extbody(), ts::ivec2(0));
            bitmap = b;
        }

        if (resize_k < 1.0f)
        {
            bitmap.resize(b, ts::ivec2( ts::lround(resize_k * bitmap.info().sz.x), ts::lround(resize_k * bitmap.info().sz.y)), ts::FILTER_LANCZOS3 );
        } else
            b = bitmap;

        alpha = image.create_from_bitmap(b, false, true, true);
        dirty = true;
        getengine().redraw();
    }
}

/*virtual*/ ts::wstr_c dialog_avaselector_c::get_name() const
{
    return TTT("[appname]: Выбор изображения аватара",210);
}

void dialog_avaselector_c::prepare_stuff()
{
    if (!dirty) return;
    dirty = false;
    int y0 = 120;
    ts::ivec2 sz1( getprops().size() - ts::ivec2(30) );
    if (RID b = find(CONSTASTR("last")))
        y0 = HOLD(b)().getprops().pos().y;
    if (RID b = find( CONSTASTR("dialog_button_1") ))
        sz1 = HOLD(b)().getprops().pos();

    ts::irect ca = get_client_area();
    viewrect = ts::irect( ca.lt.x + 10, y0 + 2, ca.rb.x - 10, sz1.y - 2 );

    imgrect.lt = ts::ivec2(0);
    imgrect.rb = image.info().sz;
    out.lt = (viewrect.size() - imgrect.size()) / 2;
    offset = viewrect.lt + out.lt + user_offset;

    //out.rb = out.lt + imgrect.rb - viewrect.size();
    //out.lt = -out.lt;

    imgrect.intersect( viewrect - offset );
    if (viewrect.lt.x > offset.x) offset.x = viewrect.lt.x;
    if (viewrect.lt.y > offset.y) offset.y = viewrect.lt.y;

    if (imgrect.lt.x > 0)
        out.lt.x = imgrect.lt.x;
    else
        out.lt.x = viewrect.lt.x - offset.x;

    if (imgrect.lt.y > 0)
        out.lt.y = imgrect.lt.y;
    else
        out.lt.y = viewrect.lt.y - offset.y;

    if (imgrect.rb.x < image.info().sz.x)
        out.rb.x = image.info().sz.x - imgrect.rb.x;
    else
        out.rb.x = offset.x + imgrect.width() - viewrect.rb.x;

    if (imgrect.rb.y < image.info().sz.y)
        out.rb.y = image.info().sz.y - imgrect.rb.y;
    else
        out.rb.y = offset.y + imgrect.height() - viewrect.rb.y;


    inforect.lt.x = ca.lt.x + 5;
    inforect.lt.y = sz1.y;

    inforect.rb.x = sz1.x;
    inforect.rb.y = ca.rb.y;

    clamp_user_offset();
}

/*
static void draw_border(rectengine_c &e, const ts::irect & r, int drawe, ts::TSCOLOR c)
{
    if (AREA_TOP & drawe) e.draw(ts::irect(r.lt, r.rt() + ts::ivec2(0,1)), c);
    if (AREA_BOTTOM & drawe) e.draw(ts::irect(r.lb()-ts::ivec2(0,1), r.rb), c);

    if (AREA_LEFT & drawe) e.draw(ts::irect(r.lt, r.lb() + ts::ivec2(1,0)), c);
    if (AREA_RITE & drawe) e.draw(ts::irect(r.rt()-ts::ivec2(1,0), r.rb), c);

}
*/

static void draw_chessboard(rectengine_c &e, const ts::irect & r, ts::TSCOLOR c1, ts::TSCOLOR c2)
{
    ts::ivec2 sz(32);
    ts::TSCOLOR c[2] = {c1, c2};
    int mx = r.width() / sz.x + 1;
    int my = r.height() / sz.y + 1;
    for(int y = 0; y < my;++y)
    {
        for(int x = 0; x < mx ;++x)
        {
            ts::irect rr(x * sz.x + r.lt.x, y * sz.y + r.lt.y, 0, 0);
            rr.rb.x = ts::tmin(rr.lt.x + sz.x, r.rb.x);
            rr.rb.y = ts::tmin(rr.lt.y + sz.y, r.rb.y);

            if (!rr.zero_area())
                e.draw(rr,c[1 & (x ^ y)]);
        }
    }
}

void dialog_avaselector_c::recompress()
{
    if (getengine().mtrack(getrid(), MTT_APPDEFINED1) == nullptr)
    {
        if (avarect.width() < avarect.height())
        {
            avarect.setheight(avarect.width());
        }
        else if (avarect.width() > avarect.height())
        {
            avarect.setwidth(avarect.height());
        }
    }

    ts::bitmap_c b;
    b.create_RGBA( avarect.size() );
    b.copy(ts::ivec2(0), b.info().sz, image.extbody(), avarect.lt );
    auto w = sync.lock_write();
    ++w().source_tag;
    w().bitmap2encode = b; b.clear();
    w().encoded.clear();
    w().encoded_fit_16kb.clear();

    if (RID b = find(CONSTASTR("dialog_button_1")))
    {
        b.call_enable(false);
        disabled_ok = true;
    }

}

void dialog_avaselector_c::clamp_user_offset()
{
    if (getengine().mtrack(getrid(), MTT_MOVECONTENT)) return;
    if (imgrect.zero_area())
    {
        if (user_offset != ts::ivec2(0))
        {
            user_offset = ts::ivec2(0);
            dirty = true;
            getengine().redraw();
        }
        return;
    }
    ts::ivec2 o = user_offset;
    if (image.info().sz.x <= viewrect.width())
    {
        user_offset.x /= 2;

    } else if (out.lt.x < 0)
    {
        if (out.rb.x > 0)
            user_offset.x += out.lt.x / 2;
    } else if (out.rb.x < 0)
    {
        if (out.lt.x > 0)
            user_offset.x -= out.rb.x / 2;
    }
    if (image.info().sz.y <= viewrect.height())
    {
        user_offset.y /= 2;
    } else if (out.lt.y < 0)
    {
        if (out.rb.y > 0)
            user_offset.y += out.lt.y / 2;
    } else if (out.rb.y < 0)
    {
        if (out.lt.y > 0)
            user_offset.y -= out.rb.y / 2;
    }
    if (o != user_offset)
    {
        dirty = true;
        getengine().redraw();
    }
}

void dialog_avaselector_c::statrt_scale()
{
    click_resize_k = resize_k;
    storeavarect = avarect;
    storeavarect.lt.x = lround(storeavarect.lt.x / resize_k);
    storeavarect.lt.y = lround(storeavarect.lt.y / resize_k);
    storeavarect.rb.x = lround(storeavarect.rb.x / resize_k);
    storeavarect.rb.y = lround(storeavarect.rb.y / resize_k);

}
void dialog_avaselector_c::do_scale(float sf)
{
    float mink = ts::tmax((16.0f / (float)bitmap.info().sz.x), (16.0f / (float)bitmap.info().sz.y));

    resize_k = sf * click_resize_k;
    if (resize_k > 1.0f) resize_k = 1.0f;
    if (resize_k < mink) resize_k = mink;
    rebuild_bitmap();

    avarect = storeavarect;
    avarect.lt.x = lround(resize_k * avarect.lt.x);
    avarect.lt.y = lround(resize_k * avarect.lt.y);
    avarect.rb.x = lround(resize_k * avarect.rb.x);
    avarect.rb.y = lround(resize_k * avarect.rb.y);
    if (avarect.rb.x > image.info().sz.x)
    {
        avarect.rb.x = image.info().sz.x;
        if (avarect.width() < 16) avarect.lt.x = avarect.rb.x - 16;
    }
    if (avarect.rb.y > image.info().sz.y)
    {
        avarect.rb.y = image.info().sz.y;
        if (avarect.height() < 16) avarect.lt.y = avarect.rb.y - 16;
    }
    if (avarect.width() < 16) { avarect.lt.x = avarect.center().x - 8; avarect.setwidth(16); }
    if (avarect.height() < 16) { avarect.lt.y = avarect.center().y - 8; avarect.setheight(16); }

    recompress();
}


/*virtual*/ bool dialog_avaselector_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid == getrid())
    {
        switch (qp)
        {
        case SQ_DRAW:
            __super::sq_evt(qp, rid, data);
            prepare_stuff();
            if (image.info().sz > ts::ivec2(0))
            {
                ts::TSCOLOR border_c = ts::ARGB(0,0,0);
                ts::TSCOLOR info_c = ts::ARGB(0,0,0);
                if (const theme_rect_s *tr = themerect())
                {
                    if (alpha)
                        draw_chessboard(getengine(), viewrect, tr->color(0), tr->color(1));
                    border_c = tr->color(2);
                    info_c = tr->color(3);
                }

                if (!imgrect.zero_area())
                {
                    draw_data_s &dd = getengine().begin_draw();
                    dd.cliprect = viewrect;

                    //int drawe = 0;
                    //if (out.lt.y <= 0) drawe |= AREA_TOP;
                    //if (out.rb.y <= 0) drawe |= AREA_BOTTOM;
                    //if (out.lt.x <= 0) drawe |= AREA_LEFT;
                    //if (out.rb.x <= 0) drawe |= AREA_RITE;

                    //draw_border(getengine(), ts::irect(offset - 1, offset + imgrect.size() + 1), drawe, border_c);
                    getengine().draw(offset, image, imgrect, alpha);

                    fd.draw(getengine(), avarect + offset - imgrect.lt, tickvalue);
                    getengine().end_draw();
                }

                //draw_border(getengine(), viewrect, -1);

                ts::wstr_c infostr(ts::roundstr<ts::wstr_c, float>(resize_k * 100.0f, 1));
                infostr.append(CONSTWSTR("%, "));
                infostr.append_as_uint( avarect.width() );
                infostr.append(CONSTWSTR(" x "));
                infostr.append_as_uint( avarect.height() );
                int sz = sync.lock_read()().encoded.size();
                if (sz > 0 || prevsize > 0)
                {
                    ts::wsptr working[] = {CONSTWSTR("."), CONSTWSTR(" ."), CONSTWSTR("  ."), CONSTWSTR(" .")};
                    ts::wstr_c n; n.set_as_uint( sz ? sz : prevsize );
                    for(int ix = n.get_length() - 3; ix > 0; ix -= 3)
                        n.insert(ix, '`');
                    if (sz == 0) n.insert(0, CONSTWSTR("<s>")).append(CONSTWSTR("</s>"));
                    infostr.append(CONSTWSTR(" ("));
                    infostr.append( TTT("объем: $ байт",218) / n );
                    infostr.append(CONSTWSTR(") "));
                    if (sz == 0) { infostr.append(working[tickvalue & 3]); removerctl(1); }
                    else if (sync.lock_read()().encoded_fit_16kb.size())
                    {
                        infostr.append(savebtn);
                        if (disabled_ok)
                        {
                            if (RID b = find(CONSTASTR("dialog_button_1")))
                                b.call_enable(true);
                            disabled_ok = false;
                        }
                    }
                }
                if (sz > 0) prevsize = sz;

                draw_data_s &dd = getengine().begin_draw();
                dd.offset = inforect.lt;
                dd.size = inforect.size();
                text_draw_params_s tdp;
                tdp.rectupdate = getrectupdate();
                tdp.forecolor = &info_c;
                ts::flags32_s f( ts::TO_END_ELLIPSIS | ts::TO_VCENTER );
                tdp.textoptions = &f;
                getengine().draw(infostr,tdp);
                getengine().end_draw();
            }
            return true;
        case SQ_DETECT_AREA:
            if (nullptr == getengine().mtrack(getrid(), MTT_APPDEFINED1))
            {
                area = 0;
                if (data.detectarea.area == 0)
                {
                    ts::ivec2 mp = data.detectarea.pos() - offset + imgrect.lt;
                    if (ts::tabs(mp.x - avarect.lt.x) < 4) data.detectarea.area |= AREA_LEFT;
                    else if (ts::tabs(mp.x - avarect.rb.x) < 4) data.detectarea.area |= AREA_RITE;
                    
                    if (ts::tabs(mp.y - avarect.lt.y) < 4) data.detectarea.area |= AREA_TOP;
                    else if (ts::tabs(mp.y - avarect.rb.y) < 4) data.detectarea.area |= AREA_BOTTOM;
                    area = data.detectarea.area;
                    if (area & AREA_RESIZE) data.detectarea.area |= AREA_NORESIZE;
                }

            }
            break;
        case SQ_MOUSE_RDOWN:
            if (viewrect.inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
            {
                mousetrack_data_s &opd = getengine().begin_mousetrack(getrid(), MTT_MOVECONTENT);
                opd.mpos = user_offset - data.mouse.screenpos();
                return true;
            }
            break;
        case SQ_MOUSE_WHEELUP:
            {
                statrt_scale();
                do_scale(1.0f / 0.9f);
            }
            break;
        case SQ_MOUSE_WHEELDOWN:
            {
                statrt_scale();
                do_scale(0.9f);
            }
            break;
        case SQ_MOUSE_MDOWN:
            if (viewrect.inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
            {
                mousetrack_data_s &opd = getengine().begin_mousetrack(getrid(), MTT_SCALECONTENT);
                opd.mpos.y = data.mouse.screenpos().y;
                statrt_scale();
                return true;
            }
            break;
        case SQ_MOUSE_LDOWN:
            if (area)
            {
                mousetrack_data_s &opd = getengine().begin_mousetrack(getrid(), MTT_APPDEFINED1);
                opd.area = area;
                opd.mpos = data.mouse.screenpos();
                opd.rect = avarect;
                return true;
            } else if ((avarect + offset - imgrect.lt).inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
            {
                mousetrack_data_s &opd = getengine().begin_mousetrack(getrid(), MTT_APPDEFINED2);
                opd.mpos = data.mouse.screenpos();
                storeavarect = avarect;
                return true;
            }

            break;
        case SQ_MOUSE_MUP:
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_SCALECONTENT))
                getengine().end_mousetrack(MTT_SCALECONTENT);
            break;
        case SQ_MOUSE_RUP:
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_MOVECONTENT))
            {
                getengine().end_mousetrack(MTT_MOVECONTENT);
                clamp_user_offset();
            }
            break;
        case SQ_MOUSE_LUP:
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_APPDEFINED1))
            {
                getengine().end_mousetrack(MTT_APPDEFINED1);
                recompress();
            }
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_APPDEFINED2))
            {
                getengine().end_mousetrack(MTT_APPDEFINED2);
                recompress();
            }
            break;
        case SQ_MOUSE_MOVE_OP:
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_MOVECONTENT))
            {
                ts::ivec2 o = user_offset;
                user_offset = data.mouse.screenpos() + opd->mpos;
                if (o != user_offset)
                {
                    dirty = true;
                    getengine().redraw();
                }
            }
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_SCALECONTENT))
            {
                double sf = pow(2.0, -(data.mouse.screenpos().y - opd->mpos.y) * 0.01);
                do_scale((float)sf);
            }
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_APPDEFINED1))
            {
                if ( 0 != (opd->area & AREA_LEFT) )
                {
                    avarect.lt.x = opd->rect.lt.x + data.mouse.screenpos().x - opd->mpos.x;
                    if (avarect.lt.x < 0) avarect.lt.x = 0;
                    if (avarect.width() < 16) avarect.lt.x = avarect.rb.x - 16;
                }
                else if(0 != (opd->area & AREA_RITE))
                {
                    avarect.rb.x = opd->rect.rb.x + data.mouse.screenpos().x - opd->mpos.x;
                    if (avarect.rb.x > image.info().sz.x) avarect.rb.x = image.info().sz.x;
                    if (avarect.width() < 16) avarect.rb.x = avarect.lt.x + 16;
                }

                if (0 != (opd->area & AREA_TOP))
                {
                    avarect.lt.y = opd->rect.lt.y + data.mouse.screenpos().y - opd->mpos.y;
                    if (avarect.lt.y < 0) avarect.lt.y = 0;
                    if (avarect.height() < 16) avarect.lt.y = avarect.rb.y - 16;
                }
                else if (0 != (opd->area & AREA_BOTTOM))
                {
                    avarect.rb.y = opd->rect.rb.y + data.mouse.screenpos().y - opd->mpos.y;
                    if (avarect.rb.y > image.info().sz.y) avarect.rb.y = image.info().sz.y;
                    if (avarect.height() < 16) avarect.rb.y = avarect.lt.y + 16;
                }
                recompress();
                getengine().redraw();
            }
            if (mousetrack_data_s *opd = getengine().mtrack(getrid(), MTT_APPDEFINED2))
            {
                avarect = storeavarect + data.mouse.screenpos() - opd->mpos;

                if (avarect.lt.x < 0)
                {
                    avarect.lt.x = 0;
                    if (avarect.width() < 16) avarect.rb.x = 16;
                }
                if (avarect.rb.x > image.info().sz.x)
                {
                    avarect.rb.x = image.info().sz.x;
                    if (avarect.width() < 16) avarect.lt.x = avarect.rb.x - 16;
                }
                if (avarect.lt.y < 0)
                {
                    avarect.lt.y = 0;
                    if (avarect.height() < 16) avarect.rb.y = 16;
                }
                if (avarect.rb.y > image.info().sz.y)
                {
                    avarect.rb.y = image.info().sz.y;
                    if (avarect.height() < 16) avarect.lt.y = avarect.rb.y - 16;
                }
                recompress();
                getengine().redraw();
            }
            break;
        case SQ_RECT_CHANGED:
            dirty = true;
            break;
        }

    }

    return __super::sq_evt(qp,rid,data);
}
/*virtual*/ void dialog_avaselector_c::on_confirm()
{
    if (!prf().is_loaded() || image.info().sz == ts::ivec2(0)) return;

    ts::blob_c ava;
    ava.append_buf( sync.lock_read()().encoded_fit_16kb );

    prf().iterate_aps([&](const active_protocol_c &cap) {
        if (active_protocol_c *ap = prf().ap(cap.getid())) // bad
            ap->set_avatar(ava);
    });
    gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_AVATAR).send();
    __super::on_confirm();
}


void framedrawer_s::prepare(ts::TSCOLOR col1, ts::TSCOLOR col2)
{
    ts::bitmap_c b;
    b.create_RGBA(ts::ivec2(128,1));
    for(int i=0;i<128;++i)
        b.ARGBPixel(i,0,(i&4)?col1:col2);
    h.create_from_bitmap(b);

    b.create_RGBA(ts::ivec2(1, 128));
    for (int i = 0; i < 128; ++i)
        b.ARGBPixel(0, i, (i & 4) ? col1 : col2);
    v.create_from_bitmap(b);
}

void framedrawer_s::draw(rectengine_c &e, const ts::irect &r, int tickvalue)
{
    int shiftr = tickvalue & 7;
    int x = r.lt.x + 120;
    for(; x<=r.rb.x; x+=120)
    {
        e.draw(ts::ivec2(x - 120, r.lt.y), h, ts::irect(shiftr, 0, 120+shiftr, 1), false);
        e.draw(ts::ivec2(x - 120, r.rb.y-1), h, ts::irect(7-shiftr, 0, 127-shiftr, 1), false);
    }
    e.draw(ts::ivec2(x - 120, r.lt.y), h, ts::irect(shiftr, 0, r.rb.x - x + 120 + shiftr, 1), false);
    e.draw(ts::ivec2(x - 120, r.rb.y-1), h, ts::irect(7-shiftr, 0, r.rb.x - x + 127 - shiftr, 1), false);

    int y = r.lt.y + 120;
    for (; y <= r.rb.y; y += 120)
    {
        e.draw(ts::ivec2(r.lt.x, y - 120), v, ts::irect(0, 7 - shiftr, 1, 127 - shiftr), false);
        e.draw(ts::ivec2(r.rb.x - 1, y - 120), v, ts::irect(0, shiftr, 1, 120 + shiftr), false);
    }
    e.draw(ts::ivec2(r.lt.x, y - 120), v, ts::irect(0, 7 - shiftr, 1, r.rb.y - y + 127 - shiftr), false);
    e.draw(ts::ivec2(r.rb.x - 1, y - 120), v, ts::irect(0, shiftr, 1, r.rb.y - y + 120 + shiftr), false);

}

