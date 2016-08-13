#include "stdafx.h"

using namespace ts;

#define target_x 420
#define target_y 270

struct data4test_s
{
    buf_c buf;
    bitmap_c bmp;
    bitmap_c bmpt;
    drawable_bitmap_c tgt;
    drawable_bitmap_c alpha;
    text_rect_static_c txt;
    ts::font_desc_c fd;

    int test;
    int n = 100;
    uint8 alpha_v = 255;
    //TSCOLOR ccc = ts::PREMULTIPLY(ts::ARGB(0,255,105,23));
    TSCOLOR ccc = ts::ARGB(0,255,105,23);

    fastdelegate::FastDelegate<void ()> process_f = []() {};

#ifdef _WIN32
#define RSLTPATH "C:\\2\\1\\"
#elif defined _NIX
#define RSLTPATH "/tmp/"
#endif

    data4test_s(int test, uint8 av):test(test), alpha_v(av)
    {
        switch (test)
        {
            case 0:
                Print("i420 to RGB test\n");
                buf.load_from_disk_file(CONSTWSTR(RSLTPATH "i420.bin"));
                bmp.create_ARGB(ivec2(1920, 1200));
                process_f = DELEGATE( this, process_i420_to_rgb );
                process_f();
                bmp.save_as_png(CONSTWSTR(RSLTPATH "t00.png"));
                break;
            case 10:
                Print("i420 to RGB test 2\n");
                buf.load_from_disk_file(CONSTWSTR(RSLTPATH "i420.bin"));

                bmp.create_ARGB(ivec2(1038, 584));
                process_f = DELEGATE(this, process_i420_to_rgb_2);
                process_f();
                bmp.save_as_png(CONSTWSTR(RSLTPATH "t10.png"));
                break;
            case 1:
                Print("RGB to i420 test\n");
                bmp.load_from_file(CONSTWSTR(RSLTPATH "test0.png"));
                process_f = DELEGATE( this, process_rgb_to_i420 );
                process_f();
                buf.save_to_file(CONSTWSTR(RSLTPATH "i420_1.bin"));
                break;
            case 11:
                Print("RGB to i420 opt test\n");
                bmp.load_from_file(CONSTWSTR(RSLTPATH "test0.png"));
                {
                    int sz = bmp.info().sz.x * bmp.info().sz.y;
                    buf.set_size( sz + sz/2 );
                }
                process_f = DELEGATE(this, process_rgb_to_i420_opt);
                process_f();
                buf.save_to_file(CONSTWSTR(RSLTPATH "i420_2.bin"));
                break;
            case 2:
                Print("direct lanczos resample test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                bmpt.create_ARGB( ivec2(target_x, target_y) );
                process_f = DELEGATE(this, process_lanczos);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\lanczos.png");
                break;
            case 3:
                Print("indirect lanczos resample test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                {
                    int *x = (int *)&bmp.info().sz.x;
                    *x = *x - 2;
                }

                bmpt.create_ARGB(ivec2(target_x, target_y));
                process_f = DELEGATE(this, process_lanczos2);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\lanczos2.png");
                break;
            case 4:
                Print("direct bicubic resample test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                bmpt.create_ARGB(ivec2(target_x, target_y));
                process_f = DELEGATE(this, process_bicubic);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\lanczos3.png");
                break;
            case 5:
                Print("shrink 2x x86 asm\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                bmpt.create_ARGB(bmp.info().sz/2);
                process_f = DELEGATE(this, process_shrink2x86);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\shrink2x.png");
                break;
            case 6:
                Print("overfill test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                process_f = DELEGATE(this, process_overfill);
                process_f();
                bmp.save_as_png(L"C:\\2\\1\\overfill.png");
                break;
            case 7:
            case 17:
                test == 7 ? Print("my AlphaBlend test\n") : Print("system AlphaBlend test\n");

                //bmpt.load_from_file(L"C:\\2\\1\\alpha555.png");
                bmpt.load_from_file(L"C:\\2\\1\\alpha_full.png");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");

                tgt.create_from_bitmap(bmp);
                alpha.create_from_bitmap(bmpt,false,true);

                process_f = test == 7 ? DELEGATE(this, process_alphablend) : DELEGATE(this, process_alphablend_sys);
                process_f();
                tgt.save_as_png(L"C:\\2\\1\\alphablend.png");
                break;
            case 8:
                Print("draw text test\n");

                bmp.load_from_file(L"C:\\2\\1\\test0.png");

                {
                    font_params_s fp;
                    fp.filename = L"arial.ttf";
                    fp.size = ts::ivec2(25);
                    ts::add_font("default", fp);

                    txt.set_size( ts::ivec2(1920,1200) );
                    fd.assign("default");
                    txt.set_font( &fd );
                    ts::g_default_text_font = fd;
                    
                    ts::wstr_c t( L"fmiweifisfjweiriji sdf iweoi fio weoiriof sodf oiweoit fs doifwf sdf owieowei fsd foiwe fseoifweoiweifsdfsd f sdoiwe fos d " );
                    ts::wstr_c t2(t);

                    for (int i=0;i<100;++i) t.append(t2);
                    
                    t.insert(0, L"<font=default><color=22,255,23>" );

                    txt.set_text(t, nullptr, false);
                    txt.parse_and_render_texture(nullptr, nullptr, false);
                    txt.render_texture(nullptr, DELEGATE(this, clrbt));
                    txt.get_texture().save_as_png(L"C:\\2\\1\\dtext.png");


                //txt.parse_and_render_texture( );

                }

                //ts::parse_text();

                process_f = DELEGATE(this, process_drawtext);
                process_f();
                break;
            default:
                Print("bad test num %i\n", test);
                break;
        }
    
    }

    void process()
    {
        int mint = -1;
        for (int total = 0, cnt = 1;; ++cnt)
        {
            int ct = timeGetTime();
            for (int i = 0; i < n; ++i)
            {
                process_f();
            }
            int delta = timeGetTime() - ct;
            if (mint < 0 || delta < mint)
                mint = delta;
            total += delta;
            Print("work time: %i (%i) min:%i\n", delta, int(total / cnt), mint);
            Sleep(1);
            if (delta > 10000)
                break;
        }

    }

    void process_bicubic()
    {
        bmp.resize_to(bmpt.extbody(), FILTER_BICUBIC);
    }

    void process_shrink2x86()
    {
        // x86 - 303
        
        if (bmpt.info().sz != (bmp.info().sz-ts::ivec2(2))/2)
            bmpt.create_ARGB((bmp.info().sz-ts::ivec2(2))/2);

        bmp.shrink_2x_to(ts::ivec2(0), bmp.info().sz-ts::ivec2(2), bmpt.extbody());
    }

    void process_lanczos()
    {
        bmp.resize_to( bmpt.extbody(), FILTER_LANCZOS3 );
    }
    void process_lanczos2() // 1127
    {
        bmp.resize_to(bmpt.extbody(), FILTER_BOX_LANCZOS3);
    }

    void process_i420_to_rgb()
    {
        bmp.convert_from_yuv(ivec2(0), bmp.info().sz, buf.data(), YFORMAT_I420);
    }

    void process_i420_to_rgb_2()
    {
        int sz = bmp.info().sz.x * bmp.info().sz.y;
        int y_stride = bmp.info().sz.x;

        const uint8 *src_y = buf.data();
        const uint8 *src_u = buf.data() + sz;
        const uint8 *src_v = buf.data() + sz + sz/4;

        img_helper_i420_to_ARGB(src_y, bmp.info().sz.x, src_u, y_stride/2, src_v, y_stride/2, bmp.body(), bmp.info().pitch, bmp.info().sz.x, bmp.info().sz.y);
    }

    void process_rgb_to_i420()
    {
        bmp.convert_to_yuv(ivec2(0), bmp.info().sz, buf, YFORMAT_I420);
    }

    void process_rgb_to_i420_opt()
    {
        int sz = bmp.info().sz.x * bmp.info().sz.y;
        int y_stride = bmp.info().sz.x;
        uint8 *dst_y = buf.data();
        uint8 *dst_u = buf.data() + sz;
        uint8 *dst_v = buf.data() + sz + sz / 4;

        img_helper_ARGB_to_i420( bmp.body(), bmp.info().pitch, dst_y, y_stride, dst_u, y_stride/2, dst_v, y_stride/2, bmp.info().sz.x, bmp.info().sz.y );
    }

    void process_overfill() // no sse pm 742, no sse npm 1536, sse pm 164, sse npm 177
    {
        //bmp.overfill( ts::ivec2(100,0), bmp.info().sz-ts::ivec2(131,200), ccc );
        bmp.overfill( ts::ivec2(0), bmp.info().sz, ccc );
    }

    void alphablend_my(int x, int y, uint8 a)
    {
        img_helper_alpha_blend_pm( tgt.body(ivec2(x,y)), tgt.info().pitch, alpha.body(), alpha.info(), a, true );
    }

    void alphablend_sys(int x, int y, uint8 a)
    {
        BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)a, AC_SRC_ALPHA };
        //AlphaBlend(tgt.DC(), x, y, alpha.info().sz.x, alpha.info().sz.y, alpha.DC(), 0, 0, alpha.info().sz.x, alpha.info().sz.y, blendPixelFunction);
    }

    void process_alphablend() // 496 // 870
    {
        // sys 1316/2431 min
        // sse 348/365 min
        alphablend_my(0, 0, alpha_v);



        //alphablend(111, 111, 255);
        //alphablend(1024, 128, 255);
        //alphablend(500, 428, 126);
    }

    void process_alphablend_sys() // 496 // 870
    {
        // sys 1316/2431 min
        // sse 348/365 min
        alphablend_sys(0, 0, alpha_v);



        //alphablend(111, 111, 255);
        //alphablend(1024, 128, 255);
        //alphablend(500, 428, 126);
    }

    void clrbt( ts::bitmap_c &b, const ts::ivec2 &sz )
    {
        b.copy( ts::ivec2(0), sz, bmp.extbody(), ts::ivec2(0) );
    }

    void process_drawtext() // 1544 // 967
    {
        txt.render_texture(nullptr, DELEGATE(this, clrbt));
    }

};

int proc_test(const wstrings_c & pars)
{
    TSCOLOR c = ARGB<int>(-1,-2,-3,0);
    if (c != 0)
        __debugbreak();

    int test = 0;
    if (pars.size() > 1) test = pars.get(1).as_int();

    uint8 aval = 255;
    for(const wstr_c &p : pars)
    {
        if (p.equals(CONSTWSTR("nosse")))
            g_cpu_caps &= ~(CPU_SSE|CPU_SSE2|CPU_SSE3|CPU_SSSE3);
        else if (p.begins(CONSTWSTR("alpha=")))
            aval = (uint8)p.as_num_part<uint>(255,6);
    }


    data4test_s data(test, aval);
    data.process();



    return 0;
}
