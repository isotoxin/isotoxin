#include "stdafx.h"

using namespace ts;

#define target_x 620
#define target_y 420

struct data4test_s
{
    buf_c buf;
    bitmap_c bmp;
    bitmap_c bmpt;
    int test;
    int n = 100;

    fastdelegate::FastDelegate<void ()> process_f = []() {};

    data4test_s(int test):test(test)
    {
        switch (test)
        {
            case 0:
                Print("i420 to RGB test\n");
                buf.load_from_disk_file(L"C:\\2\\1\\i420.bin");
                bmp.create_RGBA(ivec2(1920, 1200));
                process_f = DELEGATE( this, process_i420_to_rgb );
                process_f();
                bmp.save_as_png(L"C:\\2\\1\\t00.png");
                break;
            case 10:
                Print("i420 to RGB test 2\n");
                buf.load_from_disk_file(L"C:\\2\\1\\i420.bin");
                bmp.create_RGBA(ivec2(1920, 1200));
                process_f = DELEGATE(this, process_i420_to_rgb_2);
                process_f();
                bmp.save_as_png(L"C:\\2\\1\\t10.png");
                break;
            case 1:
                Print("RGB to i420 test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                process_f = DELEGATE( this, process_rgb_to_i420 );
                break;
            case 2:
                Print("direct lanczos resample test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                bmpt.create_RGBA( ivec2(target_x, target_y) );
                process_f = DELEGATE(this, process_lanczos);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\lanczos.png");
                break;
            case 3:
                Print("indirect lanczos resample test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                bmpt.create_RGBA(ivec2(target_x, target_y));
                process_f = DELEGATE(this, process_lanczos2);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\lanczos2.png");
                break;
            case 4:
                Print("direct bicubic resample test\n");
                bmp.load_from_file(L"C:\\2\\1\\test0.png");
                bmpt.create_RGBA(ivec2(target_x, target_y));
                process_f = DELEGATE(this, process_bicubic);
                process_f();
                bmpt.save_as_png(L"C:\\2\\1\\lanczos3.png");
                break;
            default:
                Print("bad test num %i\n", test);
                break;
        }
    
    }

    void process()
    {
        for (int total = 0, cnt = 1;; ++cnt)
        {
            int ct = timeGetTime();
            for (int i = 0; i < n; ++i)
            {
                process_f();
            }
            int delta = timeGetTime() - ct;
            total += delta;
            Print("work time: %i (%i) ms\n", delta, int(total / cnt));
            Sleep(1);
            if (delta > 10000)
                break;
        }

    }

    void process_bicubic()
    {
        bmp.resize_to(bmpt.extbody(), FILTER_BICUBIC);
    }

    void process_lanczos()
    {
        bmp.resize_to( bmpt.extbody(), FILTER_LANCZOS3 );
    }
    void process_lanczos2()
    {
        /*
        bitmap_c b = bmp;
        ts::ivec2 sz2 = bmp.info().sz/2;
        for(;sz2 >>= bmpt.info().sz;)
        {
            bitmap_c b2; b2.create_RGBA(sz2);
            b.shrink_2x_to(ivec2(0), b.info().sz, b2.extbody());
            b = b2;
            sz2 = b2.info().sz / 2;
        }
        */

        bmp.resize_to(bmpt.extbody(), FILTER_BOX_LANCZOS3);
    }

    void process_i420_to_rgb()
    {
        bmp.convert_from_yuv(ivec2(0), bmp.info().sz, buf.data(), YFORMAT_I420);
    }

    void process_i420_to_rgb_2()
    {
        //void TSCALL img_helper_i420_to_ARGB(const uint8* src_y, int src_stride_y, const uint8* src_u, int src_stride_u, const uint8* src_v, int src_stride_v, uint8* dst_argb, int dst_stride_argb, int width, int height);

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


};

int proc_test(const wstrings_c & pars)
{
    TSCOLOR c = ARGB<int>(-1,-2,-3,0);
    if (c != 0)
        __debugbreak();

    int test = 0;
    if (pars.size() > 1) test = pars.get(1).as_int();


    data4test_s data(test);
    data.process();



    return 0;
}
