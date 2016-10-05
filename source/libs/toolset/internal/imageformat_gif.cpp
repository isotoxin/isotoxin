#include "toolset.h"

#include "giflib/lib/gif_lib.h"

#pragma comment(lib, "giflib.lib")

namespace ts
{
    struct gifread_s
    {
        GifFileType *gif;
        const uint8 *sourcebuf;
        const SavedImage *prevsi;
        ts::aint ibuflen;
        int offset;
        int frame;
        int prevdisposal;
        TSCOLOR *colors; // only for animated_c

        static int InputFunc(GifFileType *gif, GifByteType *buf, int len)
        {
            gifread_s *me = (gifread_s *)gif->UserData;
            ts::aint readsz = tmin(len, me->ibuflen - me->offset);
            memcpy( buf, me->sourcebuf + me->offset, readsz );
            me->offset += len;
            return (int)readsz;
        }
        void close()
        {
            int err = 0;
            if (gif)
                DGifCloseFile(gif,&err);
            if (colors)
                MM_FREE(colors);
            memset(this, 0, sizeof(gifread_s));
        }
    };

    static bool gifdatareader(img_reader_s &r, void * buf, int pitch)
    {
        gifread_s & br = ref_cast<gifread_s>(r.data);

        if (GIF_ERROR == DGifSlurp(br.gif))
        {
            br.close();
            return false;
        }
        const SavedImage *si = br.gif->SavedImages;
        uint8 *pixels = (uint8 *)buf;
        int outlinebytesize = r.size.x * 4;
        for(int y=0;y < si->ImageDesc.Top;++y, pixels += pitch)
            memset(pixels,0,outlinebytesize);
        
        const ColorMapObject *cm = si->ImageDesc.ColorMap ? si->ImageDesc.ColorMap : br.gif->SColorMap;
        GraphicsControlBlock gcb;
        DGifSavedExtensionToGCB(br.gif,0,&gcb);

        TSCOLOR colors[256];
        for (int i=0;i<cm->ColorCount;++i)
        {
            const GifColorType &c = cm->Colors[i];
            colors[i] = ARGB( c.Red, c.Green, c.Blue );
        }
        if (gcb.TransparentColor >= 0)
            colors[gcb.TransparentColor] &= 0xFFFFFF; // clear alpha

        const uint8 *spixels = (uint8 *)si->RasterBits;
        int lsz = si->ImageDesc.Left * 4;
        int rsz = (r.size.x - (si->ImageDesc.Left + si->ImageDesc.Width)) * 4;
        for (int y = 0; y < si->ImageDesc.Height; ++y, pixels += pitch)
        {
            memset(pixels,0,lsz);
            if (rsz) memset(pixels + (si->ImageDesc.Left+si->ImageDesc.Width) * 4,0,rsz);
            uint32 *opixels = (uint32 *)(pixels + si->ImageDesc.Left * 4);
            for( int x = 0; x<si->ImageDesc.Width;++x )
            {
                *opixels = colors[ *spixels ];
                ++opixels;
                ++spixels;
            }
        }

        for (int y = si->ImageDesc.Top+si->ImageDesc.Height; y < r.size.y; ++y, pixels += pitch)
            memset(pixels, 0, outlinebytesize);

        br.close();
        return true;

    }

    image_read_func img_reader_s::detect_gif_format(const void *sourcebuf, aint sourcebufsize, const ivec2& limitsize )
    {
        uint32 tag = *(uint32 *)sourcebuf;
        if (tag != 944130375) return nullptr;

        memset(data, 0, sizeof(gifread_s)); //-V512
        gifread_s & br = ref_cast<gifread_s>(data);
        br.sourcebuf = (const uint8 *)sourcebuf;
        br.ibuflen = (int)sourcebufsize;
        br.offset = 0;
        int err = 0;
        br.gif = DGifOpen(&br,gifread_s::InputFunc,&err);
        if (br.gif == nullptr) return nullptr;
        size.x = br.gif->SWidth;
        size.y = br.gif->SHeight;
        bitpp = 32; // always read GIFs as 32bpp

        if ( limitsize >> 0 )
        {
            if ( size > limitsize )
            {
                return nullptr; // just not supported
            }
        }
        return gifdatareader;
    }


    bool save_to_gif_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options)
    {
        buf.clear();

        return false;
    }


    animated_c::animated_c()
    {
        memset(data,0,sizeof(data));
    }

    animated_c::~animated_c()
    {
        gifread_s & br = ref_cast<gifread_s>(data);
        br.close();
    }

    bool animated_c::load(const void *b, ts::aint bsize)
    {
        gifread_s & br = ref_cast<gifread_s>(data);
        br.close();

        uint32 tag = *(uint32 *)b;
        if (tag != 944130375) return false;

        br.sourcebuf = (const uint8 *)b;
        br.ibuflen = bsize;
        br.offset = 0;
        br.prevdisposal = DISPOSE_BACKGROUND;
        int err = 0;
        br.gif = DGifOpen(&br, gifread_s::InputFunc, &err);
        if (br.gif == nullptr) return false;
        if (GIF_ERROR == DGifSlurp(br.gif))
        {
            br.close();
            return false;
        }
        return br.gif->ImageCount > 1;
    }
    int animated_c::numframes() const
    {
        const gifread_s & br = ref_cast<const gifread_s>(data);
        return br.gif->ImageCount;
    }

    size_t animated_c::size() const
    {
        const gifread_s & br = ref_cast<const gifread_s>( data );
        return br.ibuflen;
    }

    int animated_c::firstframe(bitmap_c &bmp)
    {
        gifread_s & br = ref_cast<gifread_s>(data);

        if (br.colors == nullptr)
        {
            ColorMapObject *cm = br.gif->SColorMap;

            br.colors = (TSCOLOR *)MM_ALLOC( sizeof(TSCOLOR) * 256 );
            for (int i = 0; i < cm->ColorCount; ++i)
            {
                GifColorType &c = cm->Colors[i];
                br.colors[i] = ARGB(c.Red, c.Green, c.Blue);
            }
            for (int i = cm->ColorCount; i < 256; ++i)
                br.colors[i] = 0;
        }

        if (bmp.info().sz.x != br.gif->SWidth || bmp.info().sz.y != br.gif->SHeight)
        {
            bmp.create_ARGB( ivec2(br.gif->SWidth, br.gif->SHeight) );
            bmp.fill( br.colors[br.gif->SBackGroundColor] );
        }
        return nextframe( bmp.extbody() );
    }

    int animated_c::nextframe( const bmpcore_exbody_s &bmp )
    {
        gifread_s & br = ref_cast<gifread_s>(data);
        GraphicsControlBlock gcb;
        TSCOLOR colors[256];
        do 
        {
            DGifSavedExtensionToGCB(br.gif, br.frame, &gcb);

            if (DISPOSE_BACKGROUND == br.prevdisposal)
            {
                if (br.prevsi)
                {
                    uint8 *pixels = bmp() + br.prevsi->ImageDesc.Left * 4 + br.prevsi->ImageDesc.Top * bmp.info().pitch;
                    img_helper_fill(pixels, imgdesc_s(bmp.info(), ts::ivec2(br.prevsi->ImageDesc.Width, br.prevsi->ImageDesc.Height)), 0);
                } else
                {
                    img_helper_fill(bmp(), bmp.info(), 0);
                }
            }

            br.prevdisposal = gcb.DisposalMode;
            br.prevsi = br.gif->SavedImages + br.frame;

            uint8 *pixels = bmp() + br.prevsi->ImageDesc.Left * 4 + br.prevsi->ImageDesc.Top * bmp.info().pitch;

            const TSCOLOR *ccolors = br.colors;

            if (const ColorMapObject *cm = br.prevsi->ImageDesc.ColorMap)
            {
                for (int i = 0; i < cm->ColorCount; ++i)
                {
                    const GifColorType &c = cm->Colors[i];
                    colors[i] = ARGB(c.Red, c.Green, c.Blue);
                }
                if (gcb.TransparentColor >= 0)
                    colors[gcb.TransparentColor] &= 0xFFFFFF; // clear alpha

                ccolors = colors;
            }


            const uint8 *spixels = (const uint8 *)br.prevsi->RasterBits;
            for (int y = 0; y < br.prevsi->ImageDesc.Height; ++y, pixels += bmp.info().pitch)
            {
                uint32 *opixels = (uint32 *)pixels;
                for (int x = 0; x < br.prevsi->ImageDesc.Width; ++x)
                {
                    int index = *spixels;
                    if (index != gcb.TransparentColor)
                    {
                        *opixels = ccolors[index];
                    } else if (br.frame == 0)
                        *opixels = 0;

                    ++opixels;
                    ++spixels;
                }
            }

            ++br.frame;
            if (br.frame >= br.gif->ImageCount)
                br.frame = 0, br.prevsi = nullptr, br.prevdisposal = 0;

        } while (br.frame > 0 && gcb.DelayTime == 0);

        return gcb.DelayTime * 10;
    }


} // namespace ts