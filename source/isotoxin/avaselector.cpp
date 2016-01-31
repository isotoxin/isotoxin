#include "isotoxin.h"
#include "pngquant/lib/libimagequant.h"
#include "pnglib/src/png.h"

#pragma comment(lib, "pngquant.lib")
#pragma comment(lib, "pnglib.lib")

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

        ts::blob_c outbuf;

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

static void encode_lossy_png( ts::blob_c &buf, const ts::bitmap_c &bmp )
{
    ts::bitmap_c b2e;
    if (bmp.info().bytepp() != 4 || bmp.info().pitch != bmp.info().sz.x * 4)
    {
        b2e.create_ARGB(b2e.info().sz);
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

    buf = pngshka.outbuf;
}

/*virtual*/ int dialog_avaselector_c::compressor_s::iterate(int pass)
{
    if (!dlg || bitmap2encode.info().sz < ts::ivec2(1) ) return R_CANCEL;

    ts::aint maximumsize = 16384;
    if (0 == pass)
    {
        encode_lossy_png(encoded, bitmap2encode);
        best = encoded.size();
        bestsz = bitmap2encode.info().sz;

        if (best <= maximumsize)
        {
            encoded_fit_16kb = encoded;
            return R_DONE;
        }

        sz1 = 16;
        sz2 = ts::tmin(bitmap2encode.info().sz.x, bitmap2encode.info().sz.y);
        return 1;
    }

    ts::bitmap_c b;

    if (2 == pass)
    {
        // use best
        temp.clear();
        bitmap2encode.resize_to(b, bestsz, ts::FILTER_LANCZOS3);
        encode_lossy_png(temp, b);
        encoded_fit_16kb = temp;
        return R_DONE;
    }

    ASSERT( 1 == pass );

    int szc = (sz1 + sz2) / 2;

    ts::ivec2 newsz = bitmap2encode.info().sz;
    if (newsz.x < newsz.y)
    {
        newsz.x = szc;
        newsz.y = ts::lround(((float)newsz.x / (float)bitmap2encode.info().sz.x) * (float)bitmap2encode.info().sz.y);
    }
    else
    {
        newsz.y = szc;
        newsz.x = ts::lround(((float)newsz.y / (float)bitmap2encode.info().sz.y) * (float)bitmap2encode.info().sz.x);
    }

    temp.clear();
    bitmap2encode.resize_to(b, newsz, ts::FILTER_LANCZOS3);
    encode_lossy_png(temp, b);
    int sz = temp.size();

    if (sz > maximumsize)
    {
        if (szc != sz1 && szc != sz2)
        {
            sz2 = szc;
            return 1; // next try
        }
    }
    else
    {
        int delta = maximumsize - sz;
        if (delta < best)
        {
            best = delta;
            bestsz = newsz;
            if (szc != sz1 && szc != sz2)
            {
                sz1 = szc;
                return 1; // try next
            }
        }
    }
    return 2; // use best

}
/*virtual*/ void dialog_avaselector_c::compressor_s::done(bool canceled)
{
    if (!canceled && dlg)
        dlg->compressor_job_done(this);

    __super::done(canceled);
}

namespace
{
    struct enum_video_devices_s : public ts::task_c
    {
        vsb_list_t video_devices;
        ts::safe_ptr<dialog_avaselector_c> dlg;

        enum_video_devices_s(dialog_avaselector_c *dlg) :dlg(dlg) {}

        /*virtual*/ int iterate(int pass) override
        {
            enum_video_capture_devices(video_devices, false);
            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (!canceled && dlg)
                dlg->set_video_devices(std::move(video_devices));

            __super::done(canceled);
        }

    };
}


dialog_avaselector_c::dialog_avaselector_c(MAKE_ROOT<dialog_avaselector_c> &data) :gui_isodialog_c(data), avarect(0), protoid(data.prms.protoid) 
{
    deftitle = title_avatar_creation_tool;
    g_app->add_task(TSNEW(enum_video_devices_s, this));

    shadow = gui->theme().get_rect(CONSTASTR("shadow"));

    gui->register_kbd_callback( DELEGATE( this, paste_hotkey_handler ), SSK_V, gui_c::casw_ctrl);
    gui->register_kbd_callback( DELEGATE( this, paste_hotkey_handler ), SSK_INSERT, gui_c::casw_shift);
    gui->register_kbd_callback( DELEGATE( this, space_key ), SSK_SPACE, 0);

    animation(RID(), nullptr);

}

dialog_avaselector_c::~dialog_avaselector_c()
{
    if (compressor)
        compressor->dlg = nullptr;

    if (gui)
    {
        gui->delete_event(DELEGATE(this,animation));
        gui->unregister_kbd_callback(DELEGATE(this, paste_hotkey_handler));
        gui->unregister_kbd_callback(DELEGATE(this, space_key));
    }
}

/*virtual*/ void dialog_avaselector_c::created()
{
    set_theme_rect(CONSTASTR("avasel"), false);
    fd.prepare( get_default_text_color(3), get_default_text_color(4) );
    __super::created();
    tabsel( CONSTASTR("1") );

}

/*virtual*/ int dialog_avaselector_c::additions(ts::irect &)
{

    descmaker dm(this);
    dm << 1;

    ts::wstr_c l(CONSTWSTR("<p=c>"));

    ts::wstr_c ctlopen, ctlpaste, ctlcam;

    ts::ivec2 bsz(50,25);
    const button_desc_s *b = gui->theme().get_button("save"); //-V807
    if (b)
        bsz = b->size;

    ts::wstr_c openimgbuttonface(CONSTWSTR("open"));
    ts::wstr_c pasteimgbuttonface(CONSTWSTR("paste"));
    ts::wstr_c startcamera(CONSTWSTR("capture"));

    ts::ivec2 szopen(200, 25);
    if (const button_desc_s *avopen = gui->theme().get_button(CONSTASTR("avopen")))
        szopen = avopen->size, openimgbuttonface = CONSTWSTR("face=avopen");

    ts::ivec2 szpaste(200, 25);
    if (const button_desc_s *avpaste = gui->theme().get_button(CONSTASTR("avpaste")))
        szpaste = avpaste->size, pasteimgbuttonface = CONSTWSTR("face=avpaste");

    ts::ivec2 szcapture(200, 25);
    if (const button_desc_s *avcapture = gui->theme().get_button(CONSTASTR("avcapture")))
        szcapture = avcapture->size, startcamera = CONSTWSTR("face=avcapture");

    dm().button(ts::wstr_c(), openimgbuttonface, DELEGATE(this, open_image)).width(szopen.x).height(szopen.y).subctl(0,ctlopen).sethint( loc_text(loc_loadimagefromfile) );
    dm().button(ts::wstr_c(), pasteimgbuttonface, DELEGATE(this, paste_hotkey_handler)).width(szpaste.x).height(szpaste.y).subctl(1, ctlpaste).sethint(loc_text(loc_pasteimagefromclipboard));
    dm().button(ts::wstr_c(), startcamera, DELEGATE(this, start_capture_menu)).width(szcapture.x).height(szcapture.y).setname(CONSTASTR("startc")).subctl(2,ctlcam).sethint( loc_text(loc_capturecamera) );
    dm().button(ts::wstr_c(), b ? L"face=save" : L"save", DELEGATE(this, save_image1)).width(bsz.x).height(bsz.y).subctl(3,savebtn1);
    dm().button(ts::wstr_c(), b ? L"face=save" : L"save", DELEGATE(this, save_image2)).width(bsz.x).height(bsz.y).subctl(4,savebtn2);

    l.append(ctlopen).append(CONSTWSTR("<nbsp>"));
    l.append(ctlpaste).append(CONSTWSTR("<nbsp>"));
    l.append(ctlcam);
    dm().label(l);
    dm().hiddenlabel(ts::wstr_c(), false).setname(CONSTASTR("info"));
    dm().vspace(1).setname(CONSTASTR("last"));
    return 0;
}

bool dialog_avaselector_c::animation(RID, GUIPARAM)
{
    if (!videodevicesloaded)
        ctlenable( CONSTASTR("startc"), false );

    ++tickvalue;
    DEFERRED_UNIQUE_CALL(0.05, DELEGATE(this, animation), nullptr);
    getengine().redraw();

    if (camera)
    {
        if (RID b = find(CONSTASTR("dialog_button_1")))
        {
            b.call_enable(false);
            disabled_ok = true;
        }
    }


    return true;
}

void dialog_avaselector_c::set_video_devices(vsb_list_t &&_video_devices)
{
    video_devices = std::move(_video_devices);
    videodevicesloaded = true;
    ctlenable( CONSTASTR("startc"), video_devices.size() > 0 );
}

void dialog_avaselector_c::start_capture_menu_sel(const ts::str_c& prm)
{
    ts::wstr_c id = from_utf8(prm);
    for (const vsb_descriptor_s &vd : video_devices)
    {
        if (vd.id == id)
        {
            start_capture(vd);
            return;
        }
    }
}

void dialog_avaselector_c::start_capture(const vsb_descriptor_s &desc)
{
    animated = false;
    if (compressor)
    {
        compressor->dlg = nullptr; // say goodbye to current compressor
        compressor = nullptr;
    }

    camera.reset( vsb_c::build(desc) );
    getengine().redraw();

    caminit = false;
    update_info();
}


bool dialog_avaselector_c::start_capture_menu(RID, GUIPARAM)
{
    if (video_devices.size() == 1)
    {
        start_capture( video_devices.get(0) );
        return true;
    }

    if (RID b = find(CONSTASTR("startc")))
    {
        ts::irect br = HOLD(b)().getprops().screenrect();
        menu_c m;

        for (const vsb_descriptor_s &vd : video_devices)
        {
            m.add(vd.desc, 0, DELEGATE(this, start_capture_menu_sel), to_utf8(vd.id));
        }

        gui_popup_menu_c::show(menu_anchor_s(br, menu_anchor_s::RELPOS_TYPE_BD), m);
    }

    return true;
}

ts::uint32 dialog_avaselector_c::gm_handler(gmsg<GM_DROPFILES> &p)
{
    if (p.root == getrootrid())
    {
        load_image(p.fn);
        return GMRBIT_ABORT;
    }
    return 0;
}

bool dialog_avaselector_c::paste_hotkey_handler(RID, GUIPARAM)
{
    animated = false;
    ts::bitmap_c bmp = ts::get_clipboard_bitmap();
    if (bmp.info().sz >> 0)
    {
        bitmap = bmp;
        newimage();
    }

    return true;
}

bool dialog_avaselector_c::space_key(RID, GUIPARAM)
{
    if (animated)
    {
        nextframe();
        return true;
    }
    if (camera)
    {
        if (ts::drawable_bitmap_c *b = camera->lockbuf(nullptr))
        {
            animated = false;
            bitmap.create_RGB(b->info().sz);
            bitmap.copy( ts::ivec2(0), b->info().sz, b->extbody(), ts::ivec2(0) );
            camera->unlock(b);
            newimage();
            return true;
        }
    }

    return false;
}

static auto save_buffer = []( rectengine_root_c *root, const ts::wsptr &deffn, const ts::blob_c &buf )
{
    ts::wstr_c fromdir;
    if (prf().is_loaded())
        fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());

    ts::wstr_c title(TTT("Save avatar to png-file",213));

    ts::extension_s e[2];
    e[0].desc = CONSTWSTR("png");
    e[0].ext = e[0].desc;
    e[1].desc = CONSTWSTR("(*.*)");
    e[1].ext = CONSTWSTR("*.*");
    ts::extensions_s exts(e, 2);

    ts::wstr_c fn = root->save_filename_dialog(fromdir, deffn, exts, title);
    if (!fn.is_empty()) buf.save_to_file(fn);
};

bool dialog_avaselector_c::save_image1(RID, GUIPARAM)
{
    save_buffer(getroot(), CONSTWSTR("avatar.png"), encoded);
    return true;
}

bool dialog_avaselector_c::save_image2(RID, GUIPARAM)
{
    save_buffer(getroot(), CONSTWSTR("avatar_compressed.png"), encoded_fit_16kb);
    return true;
}

bool dialog_avaselector_c::open_image(RID, GUIPARAM)
{
    ts::wstr_c fromdir;
    if (prf().is_loaded())
        fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());

    ts::wstr_c title( loc_text(loc_image) );

    ts::extension_s extsarr[2];

    ts::wstr_c sprtfmts, sprtfmts2(CONSTWSTR("*."));
    ts::enum_supported_formats([&](const char *fmt) {sprtfmts.append(':', ts::to_wstr(fmt)); });
    sprtfmts2.append(sprtfmts);
    sprtfmts.replace_all(CONSTWSTR(":"), CONSTWSTR(", "));
    sprtfmts2.replace_all(CONSTWSTR(":"), CONSTWSTR(";*."));

    extsarr[0].desc = loc_text(loc_images);
    extsarr[0].desc.append(CONSTWSTR(" (")).append(sprtfmts).append_char(')');
    extsarr[0].ext = sprtfmts2;

    extsarr[1].desc = loc_text(loc_anyfiles);
    extsarr[1].desc.append(CONSTWSTR(" (*.*)"));
    extsarr[1].ext = CONSTWSTR("*.*");

    ts::extensions_s exts(extsarr, 2, 0);

    ts::wstr_c fn = getroot()->load_filename_dialog(fromdir, CONSTWSTR(""), exts, title);

    if (!fn.is_empty())
    {
        load_image(fn);
        if (prf().is_loaded()) prf().last_filedir(ts::fn_get_path(fn));
    }

    return true;
}

void dialog_avaselector_c::nextframe()
{
    anm.firstframe(bitmap);
    rebuild_bitmap();
    recompress();
}

void dialog_avaselector_c::update_info()
{
    bool v = false;
    ts::wstr_c t(CONSTWSTR("<p=c>"));
    if (camera && !camera->still_initializing())
        t.append(loc_text(loc_space2takeimage)), v = true, caminit = true;
    else if (animated)
        t.append(TTT("Press [i]space[/i] to go to the next frame",217)), v = true;

    set_label_text(CONSTASTR("info"), t);

    if (RID no = find(CONSTASTR("info")))
        MODIFY(no).visible(v);

    gui->repos_children( this );
    dirty = true;
}

void dialog_avaselector_c::newimage()
{
    camera.reset();
    update_info();

    user_offset = ts::ivec2(0);
    resize_k = 1.0f;
    dirty = true;
    avarect = ts::irect(0, bitmap.info().sz);
    rebuild_bitmap();
    recompress();

    if (image.info().sz > viewrect.size())
    {
        statrt_scale();
        do_scale(ts::tmin( (float)viewrect.width() / (float)image.info().sz.x, (float)viewrect.height() / (float)image.info().sz.y ));
    }
}

void dialog_avaselector_c::load_image(const ts::wstr_c &fn)
{
    ts::buf_c b; b.load_from_file(fn);
    if (b.size())
    {
        ts::bitmap_c newb;
        if (ts::img_format_e fmt = newb.load_from_file(b))
        {
            bitmap = newb;
            animated = false;
            if (ts::if_gif == fmt)
                if (false != (animated = anm.load(b.data(), b.size())))
                    anm.firstframe(bitmap);
            newimage();
        }
    }
}

void dialog_avaselector_c::rebuild_bitmap()
{
    if (bitmap.info().sz >> 0)
    {
        ts::bitmap_c b;
        if (bitmap.info().bytepp() != 4)
        {
            b.create_ARGB(bitmap.info().sz);
            b.copy(ts::ivec2(0), bitmap.info().sz, bitmap.extbody(), ts::ivec2(0));
            bitmap = b;
        }

        if (resize_k < 1.0f)
        {
            bitmap.resize_to(b, ts::ivec2( ts::lround(resize_k * bitmap.info().sz.x), ts::lround(resize_k * bitmap.info().sz.y)), ts::FILTER_LANCZOS3 );
        } else
            b = bitmap;

        image = std::move(b);
        alpha = image.premultiply();
        dirty = true;
        getengine().redraw();
    }
}

void dialog_avaselector_c::prepare_stuff()
{
    if (!dirty) return;
    dirty = false;
    int y0 = 120;
    ts::ivec2 sz1( getprops().size() - ts::ivec2(30) );
    if (RID b = find(CONSTASTR("last")))
        y0 = HOLD(b)().getprops().pos().y + 2;
    if (RID b = find( CONSTASTR("dialog_button_1") ))
        sz1 = HOLD(b)().getprops().pos() - ts::ivec2(2,10);

    ts::irect ca = get_client_area();
    viewrect = ts::irect( ca.lt.x + 10, y0 + 2, ca.rb.x - 10, sz1.y );

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
    inforect.lt.y = sz1.y + 2;

    inforect.rb.x = sz1.x;
    inforect.rb.y = ca.rb.y - 2;

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

void dialog_avaselector_c::recompress()
{
    if (compressor)
        compressor->dlg = nullptr; // say goodbye to current compressor

    if (nullptr == gui->mtrack(getrid(), MTT_APPDEFINED1))
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
    b.create_ARGB( avarect.size() );
    b.copy(ts::ivec2(0), b.info().sz, image.extbody(), avarect.lt );
    compressor = TSNEW(compressor_s, this, b);
    b.clear(); // ref count decreased // compressor now can safely work in other thread

    encoded.clear();
    encoded_fit_16kb.clear();

    if (RID bok = find(CONSTASTR("dialog_button_1")))
    {
        bok.call_enable(false);
        disabled_ok = true;
    }

    g_app->add_task(compressor); // run compressor
}

void dialog_avaselector_c::clamp_user_offset()
{
    if (gui->mtrack(getrid(), MTT_MOVECONTENT)) return;
    if (!imgrect)
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
    storeavarect.lt.x = ts::lround(storeavarect.lt.x / resize_k);
    storeavarect.lt.y = ts::lround(storeavarect.lt.y / resize_k);
    storeavarect.rb.x = ts::lround(storeavarect.rb.x / resize_k);
    storeavarect.rb.y = ts::lround(storeavarect.rb.y / resize_k);

}
void dialog_avaselector_c::do_scale(float sf)
{
    float mink = ts::tmax((16.0f / (float)bitmap.info().sz.x), (16.0f / (float)bitmap.info().sz.y));

    resize_k = sf * click_resize_k;
    if (resize_k > 1.0f) resize_k = 1.0f;
    if (resize_k < mink) resize_k = mink;
    rebuild_bitmap();

    avarect = storeavarect;
    avarect.lt.x = ts::lround(resize_k * avarect.lt.x);
    avarect.lt.y = ts::lround(resize_k * avarect.lt.y);
    avarect.rb.x = ts::lround(resize_k * avarect.rb.x);
    avarect.rb.y = ts::lround(resize_k * avarect.rb.y);
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

void dialog_avaselector_c::draw_process(ts::TSCOLOR col, bool cam, bool cambusy)
{
    removerctl(3);
    removerctl(4);

    pa.render();

    if (cam)
    {
        ts::wstr_c ainfo(loc_text(loc_initialization));
        if (cambusy) ainfo.append(CONSTWSTR("<br>")).append(loc_text(loc_camerabusy));
        draw_initialization(&getengine(), pa.bmp, viewrect, col, ainfo );
    } else
    {
        draw_data_s &dd = getengine().begin_draw();
        text_draw_params_s tdp;
        tdp.forecolor = &col;
        ts::flags32_s f(ts::TO_END_ELLIPSIS | ts::TO_VCENTER);
        tdp.textoptions = &f;
        getengine().draw(inforect.lt + ts::ivec2(0, (inforect.height() - pa.bmp.info().sz.y) / 2), pa.bmp.extbody(), true);
        int paw = pa.bmp.info().sz.x + 2;
        dd.offset = inforect.lt + ts::ivec2(paw, 0);
        dd.size = inforect.size(); dd.size.x -= paw;
        getengine().draw(TTT("Compressing...",219), tdp);
        getengine().end_draw();
    }

}

/*virtual*/ bool dialog_avaselector_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid == getrid())
    {
        bool camdraw = false;

        switch (qp)
        {
        case SQ_DRAW:
            __super::sq_evt(qp, rid, data);
            prepare_stuff();

            selrectvisible = false;
            allowdndinfo = true;

            if (camera)
            {
                allowdndinfo = false;
                if (!camera->still_initializing())
                {
                    if (!caminit)
                        update_info();

                    ts::ivec2 sz = viewrect.size();
                    ts::ivec2 shadow_size(0);
                    if (shadow)
                    {
                        shadow_size = ts::ivec2(shadow->clborder_x(), shadow->clborder_y());
                        sz -= shadow_size;
                    }

                    sz = camera->fit_to_size(sz);
                    if (ts::drawable_bitmap_c *b = camera->lockbuf(nullptr))
                    {
                        if (sz == b->info().sz)
                        {
                            getengine().begin_draw();

                            ts::ivec2 vsz = viewrect.size();
                            ts::ivec2 pos = (vsz - sz) / 2;

                            getengine().draw(pos + viewrect.lt, b->extbody(), false);

                            if (shadow)
                            {
                                evt_data_s d;
                                d.draw_thr.rect.get().lt = pos + viewrect.lt - shadow_size/2; //-V807
                                d.draw_thr.rect.get().rb = d.draw_thr.rect.get().lt + sz + shadow_size;
                                getengine().draw(*shadow, DTHRO_BORDER_RECT, &d);
                            }

                            getengine().end_draw();
                            camdraw = true;
                            allowdndinfo = false;

                        }

                        camera->unlock(b);
                    }
                }
                if (!camdraw)
                {
                    ts::TSCOLOR info_c = ts::ARGB(0, 0, 0);
                    if (const theme_rect_s *tr = themerect())
                        info_c = tr->color(2);
                    draw_process(info_c, true, camera->is_busy());
                    allowdndinfo = false;
                }
            }
            
            if (camera == nullptr && (image.info().sz > ts::ivec2(0)))
            {
                allowdndinfo = false;

                ts::TSCOLOR info_c = ts::ARGB(0,0,0);
                if (const theme_rect_s *tr = themerect())
                {
                    if (alpha)
                        draw_chessboard(getengine(), viewrect, tr->color(0), tr->color(1));
                    info_c = tr->color(2);
                }

                if (imgrect)
                {
                    draw_data_s &dd = getengine().begin_draw();
                    dd.cliprect = viewrect;
                    getengine().draw(offset, image.extbody(imgrect), alpha);
                    ts::ivec2 o = offset - imgrect.lt;

                    fd.draw(getengine(), avarect + o, tickvalue);

                    if (avarect.lt.x > 0)
                    {
                        // draw dark at left
                        getengine().draw( ts::irect( o.x, o.y + avarect.lt.y, o.x + avarect.lt.x, o.y + avarect.rb.y ), ts::ARGB(0,0,50,50) );
                    }
                    int rw = image.info().sz.x - avarect.rb.x - 1;
                    if ( rw > 0 )
                    {
                        // draw dark at rite
                        getengine().draw( ts::irect( o.x + avarect.rb.x, o.y + avarect.lt.y, o.x + image.info().sz.x, o.y + avarect.rb.y ), ts::ARGB(0,0,50,50) );
                    }
                    if (avarect.lt.y > 0)
                    {
                        // draw dark at top
                        getengine().draw( ts::irect( o.x, o.y, o.x + image.info().sz.x, o.y + avarect.lt.y ), ts::ARGB(0,0,50,50) );
                    }
                    int rh = image.info().sz.y - avarect.rb.y - 1;
                    if (rh > 0)
                    {
                        // draw dark at bottom
                        getengine().draw(ts::irect(o.x, o.y + avarect.rb.y, o.x + image.info().sz.x, o.y + image.info().sz.y), ts::ARGB(0, 0, 50, 50));
                    }


                    getengine().end_draw();
                    selrectvisible = true;
                }
                
                if (compressor)
                {
                    draw_process( info_c, false, false );

                } else
                {
                    ts::wstr_c infostr(ts::roundstr<ts::wstr_c, float>(resize_k * 100.0f, 1));
                    infostr.append(CONSTWSTR("%, "));
                    infostr.append_as_uint(avarect.width());
                    infostr.append(CONSTWSTR(" x "));
                    infostr.append_as_uint(avarect.height());
                    int sz = encoded.size();
                    if (sz > 0 || prevsize > 0)
                    {
                        auto add_size_str = [](ts::wstr_c &s, int sz)
                        {
                            s.append(CONSTWSTR(" ("));
                            s.append(text_sizebytes(sz));
                            s.append(CONSTWSTR(") "));
                        };

                        add_size_str(infostr, sz ? sz : prevsize);

                        if (encoded_fit_16kb.size())
                        {
                            infostr.append(savebtn1);
                            if (disabled_ok)
                            {
                                if (RID b = find(CONSTASTR("dialog_button_1")))
                                    b.call_enable(true);
                                disabled_ok = false;
                            }
                            ts::wstr_c szs;
                            szs.append_as_uint(encoded_fit_16kb_size.x);
                            szs.append(CONSTWSTR(" x "));
                            szs.append_as_uint(encoded_fit_16kb_size.y);
                            add_size_str(szs, encoded_fit_16kb.size());

                            infostr.append(CONSTWSTR("<br>")).append(TTT("Compressed: $",221) / szs);
                            infostr.append(savebtn2);
                        }
                    }
                    if (sz > 0) prevsize = sz;

                    draw_data_s &dd = getengine().begin_draw();
                    dd.offset = inforect.lt;
                    dd.size = inforect.size();
                    text_draw_params_s tdp;
                    tdp.rectupdate = getrectupdate();
                    tdp.forecolor = &info_c;
                    ts::flags32_s f(ts::TO_END_ELLIPSIS | ts::TO_VCENTER);
                    tdp.textoptions = &f;
                    getengine().draw(infostr, tdp);
                    getengine().end_draw();

                }
            }

            if (allowdndinfo)
            {
                // empty

                draw_data_s&dd = getengine().begin_draw();

                ts::wstr_c t(CONSTWSTR("<l>"),  loc_text(loc_dropimagehere)); t.append(CONSTWSTR("</l>"));
                ts::ivec2 tsz = gui->textsize(ts::g_default_text_font, t);
                ts::ivec2 tpos = (viewrect.size() - tsz) / 2;


                if (const theme_image_s *img = gui->theme().get_image(CONSTASTR("avdroptarget")))
                {
                    ts::ivec2 imgpos(viewrect.lt + (viewrect.size() - img->info().sz) / 2);
                    imgpos.y -= tsz.y / 2 + 2;
                    img->draw(getengine(), imgpos);
                    tpos.y += img->info().sz.y/2 + 1;
                }


                ts::TSCOLOR info_c = ts::ARGB(0, 0, 0);
                if (const theme_rect_s *tr = themerect())
                    info_c = tr->color(2);


                text_draw_params_s tdp;
                tdp.forecolor = &info_c;

                dd.offset += tpos + viewrect.lt;
                dd.size = tsz;

                getengine().draw(t, tdp);

                getengine().end_draw();

            }

            return true;
        case SQ_DETECT_AREA:
            if (nullptr == gui->mtrack(getrid(), MTT_APPDEFINED1))
            {
                area = 0;
                if (data.detectarea.area == 0 && selrectvisible)
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
            if (camera == nullptr && viewrect.inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
            {
                mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_MOVECONTENT);
                opd.mpos = user_offset - data.mouse.screenpos();
                return true;
            }
            break;
        case SQ_MOUSE_WHEELUP:
            if (camera == nullptr)
            {
                statrt_scale();
                do_scale(1.0f / 0.9f);
            }
            break;
        case SQ_MOUSE_WHEELDOWN:
            if (camera == nullptr)
            {
                statrt_scale();
                do_scale(0.9f);
            }
            break;
        case SQ_MOUSE_MDOWN:
            if (camera == nullptr && viewrect.inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
            {
                mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_SCALECONTENT);
                opd.mpos.y = data.mouse.screenpos().y;
                statrt_scale();
                return true;
            }
            break;
        case SQ_MOUSE_LDOWN:
            if (camera == nullptr)
            {
                if (area)
                {
                    mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_APPDEFINED1);
                    opd.area = area;
                    opd.mpos = data.mouse.screenpos();
                    opd.rect = avarect;
                    return true;
                }
                else if ((avarect + offset - imgrect.lt).inside(to_local(data.mouse.screenpos)) && (bitmap.info().sz >> 0))
                {
                    mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_APPDEFINED2);
                    opd.mpos = data.mouse.screenpos();
                    storeavarect = avarect;
                    return true;
                }

            }
            break;
        case SQ_MOUSE_MUP:
            gui->end_mousetrack(getrid(), MTT_SCALECONTENT);
            break;
        case SQ_MOUSE_RUP:
            if (gui->end_mousetrack(getrid(), MTT_MOVECONTENT))
                clamp_user_offset();
            break;
        case SQ_MOUSE_LUP:
            if (gui->end_mousetrack(getrid(), MTT_APPDEFINED1) || gui->end_mousetrack(getrid(), MTT_APPDEFINED2))
                recompress();
            break;
        case SQ_MOUSE_MOVE_OP:
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_MOVECONTENT))
            {
                ts::ivec2 o = user_offset;
                user_offset = data.mouse.screenpos() + opd->mpos;
                if (o != user_offset)
                {
                    dirty = true;
                    getengine().redraw();
                }
            }
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_SCALECONTENT))
            {
                double sf = pow(2.0, -(data.mouse.screenpos().y - opd->mpos.y) * 0.01);
                do_scale((float)sf);
            }
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_APPDEFINED1))
            {
                if ( 0 != (opd->area & AREA_LEFT) )
                {
                    avarect.lt.x = opd->rect.lt.x + data.mouse.screenpos().x - opd->mpos.x;
                    if (avarect.lt.x < 0) avarect.lt.x = 0;
                    if (avarect.width() < 16) avarect.lt.x = avarect.rb.x - 16;
                    avarect.setheight( avarect.width() );
                    if (avarect.rb.y > image.info().sz.y)
                    {
                        avarect.lt.y -= avarect.rb.y - image.info().sz.y;
                        avarect.rb.y = image.info().sz.y;
                    }
                    if (avarect.lt.y < 0)
                    {
                        avarect.lt.x -= avarect.lt.y;
                        avarect.lt.y = 0;
                    }
                    ASSERT( avarect.width() == avarect.height() );
                }
                else if(0 != (opd->area & AREA_RITE))
                {
                    avarect.rb.x = opd->rect.rb.x + data.mouse.screenpos().x - opd->mpos.x;
                    if (avarect.rb.x > image.info().sz.x) avarect.rb.x = image.info().sz.x;
                    if (avarect.width() < 16) avarect.rb.x = avarect.lt.x + 16;
                    
                    avarect.setheight(avarect.width());
                    if (avarect.rb.y > image.info().sz.y)
                    {
                        avarect.lt.y -= avarect.rb.y - image.info().sz.y;
                        avarect.rb.y = image.info().sz.y;
                    }
                    if (avarect.lt.y < 0)
                    {
                        avarect.rb.x += avarect.lt.y;
                        avarect.lt.y = 0;
                    }
                    ASSERT(avarect.width() == avarect.height());
                }

                if (0 != (opd->area & AREA_TOP))
                {
                    avarect.lt.y = opd->rect.lt.y + data.mouse.screenpos().y - opd->mpos.y;
                    if (avarect.lt.y < 0) avarect.lt.y = 0;
                    if (avarect.height() < 16) avarect.lt.y = avarect.rb.y - 16;

                    avarect.setwidth(avarect.height());
                    if (avarect.rb.x > image.info().sz.x)
                    {
                        avarect.lt.x -= avarect.rb.x - image.info().sz.x;
                        avarect.rb.x = image.info().sz.x;
                    }
                    if (avarect.lt.x < 0)
                    {
                        avarect.lt.y -= avarect.lt.x;
                        avarect.lt.x = 0;
                    }
                    ASSERT(avarect.width() == avarect.height());


                }
                else if (0 != (opd->area & AREA_BOTTOM))
                {
                    avarect.rb.y = opd->rect.rb.y + data.mouse.screenpos().y - opd->mpos.y;
                    if (avarect.rb.y > image.info().sz.y) avarect.rb.y = image.info().sz.y;
                    if (avarect.height() < 16) avarect.rb.y = avarect.lt.y + 16;

                    avarect.setwidth(avarect.height());
                    if (avarect.rb.x > image.info().sz.x)
                    {
                        avarect.lt.x -= avarect.rb.x - image.info().sz.x;
                        avarect.rb.x = image.info().sz.x;
                    }
                    if (avarect.lt.x < 0)
                    {
                        avarect.rb.y += avarect.lt.x;
                        avarect.lt.x = 0;
                    }
                    ASSERT(avarect.width() == avarect.height());

                }
            }
            if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_APPDEFINED2))
            {
                avarect = storeavarect + data.mouse.screenpos() - opd->mpos;

                if (avarect.lt.x < 0)
                {
                    avarect.lt.x = 0;
                    if (avarect.width() < 16) avarect.rb.x = 16;
                    avarect.setheight( avarect.width() );
                }
                if (avarect.rb.x > image.info().sz.x)
                {
                    avarect.rb.x = image.info().sz.x;
                    if (avarect.width() < 16) avarect.lt.x = avarect.rb.x - 16;
                    avarect.setheight( avarect.width() );
                }
                if (avarect.lt.y < 0)
                {
                    avarect.lt.y = 0;
                    if (avarect.height() < 16) avarect.rb.y = 16;
                    avarect.setwidth( avarect.height() );
                }
                if (avarect.rb.y > image.info().sz.y)
                {
                    avarect.rb.y = image.info().sz.y;
                    if (avarect.height() < 16) avarect.lt.y = avarect.rb.y - 16;
                    avarect.setwidth( avarect.height() );
                }
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

    ts::blob_c ava = encoded_fit_16kb;

    if (0 == protoid)
    {
        prf().iterate_aps([&](const active_protocol_c &cap) {
            if (active_protocol_c *ap = prf().ap(cap.getid())) // bad
                ap->set_avatar(ava);
        });

    } else if (active_protocol_c *ap = prf().ap(protoid))
        ap->set_avatar(ava);

    gmsg<ISOGM_CHANGED_SETTINGS>(protoid, PP_AVATAR).send();
    __super::on_confirm();
}


void framedrawer_s::prepare(ts::TSCOLOR col1, ts::TSCOLOR col2)
{
    h.create_ARGB(ts::ivec2(128,1));
    for(int i=0;i<128;++i)
        h.ARGBPixel(i,0,(i&4)?col1:col2);

    v.create_ARGB(ts::ivec2(1, 128));
    for (int i = 0; i < 128; ++i)
        v.ARGBPixel(0, i, (i & 4) ? col1 : col2);
}

void framedrawer_s::draw(rectengine_c &e, const ts::irect &r, int tickvalue)
{
    int shiftr = tickvalue & 7;
    int x = r.lt.x + 120;
    for(; x<=r.rb.x; x+=120)
    {
        e.draw(ts::ivec2(x - 120, r.lt.y), h.extbody(ts::irect(shiftr, 0, 120 + shiftr, 1)), false);
        e.draw(ts::ivec2(x - 120, r.rb.y-1), h.extbody(ts::irect(7 - shiftr, 0, 127 - shiftr, 1)), false);
    }
    e.draw(ts::ivec2(x - 120, r.lt.y), h.extbody(ts::irect(shiftr, 0, r.rb.x - x + 120 + shiftr, 1)), false);
    e.draw(ts::ivec2(x - 120, r.rb.y-1), h.extbody(ts::irect(7 - shiftr, 0, r.rb.x - x + 127 - shiftr, 1)), false);

    int y = r.lt.y + 120;
    for (; y <= r.rb.y; y += 120)
    {
        e.draw(ts::ivec2(r.lt.x, y - 120), v.extbody(ts::irect(0, 7 - shiftr, 1, 127 - shiftr)), false);
        e.draw(ts::ivec2(r.rb.x - 1, y - 120), v.extbody(ts::irect(0, shiftr, 1, 120 + shiftr)), false);
    }
    e.draw(ts::ivec2(r.lt.x, y - 120), v.extbody(ts::irect(0, 7 - shiftr, 1, r.rb.y - y + 127 - shiftr)), false);
    e.draw(ts::ivec2(r.rb.x - 1, y - 120), v.extbody(ts::irect(0, shiftr, 1, r.rb.y - y + 120 + shiftr)), false);

}

