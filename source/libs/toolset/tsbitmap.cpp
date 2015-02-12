#include "toolset.h"
#include "internal/FilePNG.h"
#include "internal/FileDDS.h"
#include <malloc.h>
#include <ddraw.h>
#include <d3d9types.h>
//#include "squish.h"

#pragma pack (push)
#pragma pack (16)
extern "C" {
#include "jpeg\src\jpeglib.h"
#include "jpeg\src\jerror.h"
}
#pragma pack (pop)

#pragma message("Automatically linking with jpeg.lib")
#pragma comment(lib, "jpeg.lib")

namespace ts
{

static FORCEINLINE int calc_mask_x(int m0, int m1)
{
    return (0xF << m0) & (0xF >> (4 - m1));
}

static FORCEINLINE int calc_mask_y(int m0, int m1)
{
    return (0xFFFF << m0 * 4) & (0xFFFF >> (4 - m1) * 4);
}


void TSCALL img_helper_copy(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info)
{
    ASSERT(sou_info.sz == des_info.sz);

    if (des_info.bytepp() == sou_info.bytepp())
    {
        aint len = sou_info.sz.x * des_info.bytepp();
        if (len == des_info.pitch && des_info.pitch == sou_info.pitch)
        {
            memcpy(des, sou, len * sou_info.sz.y);
        }
        else
        {
            for (int y = 0; y < sou_info.sz.y; ++y, des += des_info.pitch, sou += sou_info.pitch)
            {
                memcpy(des, sou, len);
            }
        }
    }
    else
    {
        uint32 alpha = (sou_info.bytepp() == 3) ? 0xFF000000 : 0;

        for (int y = 0; y < sou_info.sz.y; ++y, des += des_info.pitch, sou += sou_info.pitch)
        {
            uint8 *des1 = des;
            const uint8 *sou1 = sou;
            for (int i = 0; i < (sou_info.sz.x - 1); ++i, sou1 += sou_info.bytepp(), des1 += des_info.bytepp())
            {
                uint32 color = *((uint32 *)sou1) | alpha;
                *((uint32 *)des1) = color;
            }
            int c = tmin(sou_info.bytepp(), des_info.bytepp());
            while (c--)
            {
                *des1++ = *sou1++;
            }
            if (alpha) *des1 = 0xFF;
        }
    }


}

#pragma warning(push)
#pragma warning(disable: 4731)
void TSCALL img_helper_make_2x_smaller(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info)
{
    ASSERT(des_info.sz == (sou_info.sz / 2));
    ivec2 newsz = sou_info.sz / 2;

    aint desnl = des_info.pitch - des_info.sz.x*des_info.bytepp();
    aint sounl = sou_info.pitch - sou_info.sz.x*sou_info.bytepp();

    if (sou_info.bytepp() == 1)
    {
        for (int y = 0; y < sou_info.sz.y; y += 2)
        {
            for (int x = 0; x < sou_info.sz.x; x += 2, ++des)
            {
                int b0 = *(sou + x + 0);

                b0 += *(sou + x + 1);

                b0 += *(sou + x + 0 + sou_info.pitch);

                b0 += *(sou + x + 1 + sou_info.pitch);

                *(des + 0) = (uint8)(b0 / 4);
            }

            sou += sou_info.pitch * 2;
        }
    }
    else if (sou_info.bytepp() == 2)
    {
        for (int y = 0; y < newsz.y; y++, sou += sou_info.pitch + sounl, des += desnl)
        {
            for (int x = 0; x < newsz.x; x++, des += 3, sou += 3 + 3)
            {

                int b0 = *(sou + 0);
                int b1 = *(sou + 1);
                int b2 = *(sou + 2);

                b0 += *(sou + 3);
                b1 += *(sou + 4);
                b2 += *(sou + 5);

                b0 += *(sou + 0 + sou_info.pitch);
                b1 += *(sou + 1 + sou_info.pitch);
                b2 += *(sou + 2 + sou_info.pitch);

                b0 += *(sou + 3 + sou_info.pitch);
                b1 += *(sou + 4 + sou_info.pitch);
                b2 += *(sou + 5 + sou_info.pitch);

                *(des + 0) = (uint8)(b0 >> 2);
                *(des + 1) = (uint8)(b1 >> 2);
                *(des + 2) = (uint8)(b2 >> 2);
            }
        }
    }
    else if (sou_info.bytepp() == 4)
    {
        const int xxx = offsetof(imgdesc_s, sz) + offsetof(ivec2, x);
        const int yyy = offsetof(imgdesc_s, sz) + offsetof(ivec2, y);
        const int zzz = offsetof(imgdesc_s, pitch);
        _asm
        {

            mov esi, sou
            mov edi, des
            movzx ebx, word ptr[sou_info+zzz]//sou_info.pitch

            mov eax, dword ptr [des_info+yyy]
        loopy :
            push eax
            push ebp

            mov eax, dword ptr [des_info+xxx]
        loopx :
            push eax

            xor ebp, ebp
            xor ecx, ecx

            mov ebp, [esi]
            mov ecx, ebp
            and ebp, 0x00FF00FF
            and ecx, 0xFF00FF00

            mov eax, [esi + 4]
            mov edx, eax
            and eax, 0x00FF00FF
            and edx, 0xFF00FF00
            add ebp, eax
            add ecx, edx

            mov eax, [esi + ebx]
            adc ecx, 0
            mov edx, eax
            and eax, 0x00FF00FF
            and edx, 0xFF00FF00
            add ebp, eax
            add ecx, edx

            mov eax, [esi + ebx + 4]
            adc ecx, 0
            mov edx, eax
            and eax, 0x00FF00FF
            and edx, 0xFF00FF00
            add eax, ebp
            add edx, ecx
            adc edx, 0

            ror eax, 2
            ror edx, 2
            and eax, 0x00FF00FF
            and edx, 0xFF00FF00
            or  eax, edx

            mov[edi], eax

            add esi, 8
            add edi, 4

            pop eax
            dec eax
            jnz loopx

            pop ebp

            mov eax, sounl
            add esi, ebx
            add esi, eax
            add edi, desnl

            pop eax
            dec eax
            jnz loopy

        };

    }
}
#pragma warning(pop)

//template<typename CORE> void bitmap_t<CORE>::copy_components(int num_comps, ivec2 size, int dc, int dpitch, uint8 *d, int sc, int spitch, const uint8 *s)
void TSCALL img_helper_copy_components(uint8* des, const uint8* sou, const imgdesc_s &des_info, const imgdesc_s &sou_info, int num_comps)
{
    aint dpitch = des_info.pitch;
    aint spitch = sou_info.pitch;
    aint dszy = des_info.sz.y;
    for (dpitch -= des_info.sz.x*des_info.bytepp(), spitch -= des_info.sz.x*sou_info.bytepp(); dszy > 0; --dszy, des += dpitch, sou += spitch)
        for (int x = des_info.sz.x; x > 0; --x, des += des_info.bytepp(), sou += sou_info.bytepp())
            for (int i = 0; i < num_comps; ++i)
                des[i] = sou[i];
}


//(uint8 *des, int des_info.pitch, const ivec2 &des_size, const ivec2 &from_p, const void *src_bits, int src_pitch, int squish_fmt)
void TSCALL img_helper_get_from_dxt(uint8 *des, const imgdesc_s &des_info, const ivec2 &from_p, const void *sou, int sou_pitch, int squish_fmt)
{

    int src_byte_delta = squish_fmt == dxt::kDxt1 ? 8 : 16;

    int blk_x = from_p.x >> 2;
    int blk_y = from_p.y >> 2;
    int bx = from_p.x - (blk_x << 2);
    int by = from_p.y - (blk_y << 2);

    int blk_cnt_x = ((from_p.x + des_info.sz.x - 1) >> 2) - blk_x + 1;
    int blk_cnt_y = ((from_p.y + des_info.sz.y - 1) >> 2) - blk_y + 1;

    // сколько пикселов справа и снизу нужно отхватить от последнего блока в строке/столбце
    int rx = (from_p.x + des_info.sz.x) - (((from_p.x + des_info.sz.x) >> 2) << 2); if (rx == 0) rx = 4;
    int ry = (from_p.y + des_info.sz.y) - (((from_p.y + des_info.sz.y) >> 2) << 2); if (ry == 0) ry = 4;

    //uint8 *dst = body() - (by != 0 ? info().pitch * 4 : 0) - (bx != 0 ? 16 : 0);
    uint8 *dst = des - by * des_info.pitch - bx * 4;

    for (int y = 0; y < blk_cnt_y; ++y)
    {
        const uint8 *srcline = (const uint8 *)sou + sou_pitch * (y + blk_y);

        int y_mask = calc_mask_y((y == 0) ? by : 0, (y < (blk_cnt_y - 1)) ? 4 : ry);

        for (int x = 0; x < blk_cnt_x; ++x)
        {
            const uint8 *srcblock = srcline + (x + blk_x) * src_byte_delta;
            uint8 *dstblock = dst + x * 4 * 4 + y * des_info.pitch * 4;

            int x_mask = calc_mask_x((x == 0) ? bx : 0, (x < (blk_cnt_x - 1)) ? 4 : rx);
            int the_mask = ((x_mask << 0) | (x_mask << 4) | (x_mask << 8) | (x_mask << 12)) & y_mask;

            dxt::decompress(the_mask, dstblock, des_info.pitch, srcblock, squish_fmt);
        }
    }
}

#pragma warning (push)
#pragma warning (disable : 4731)

void img_helper_merge_with_alpha(uint8 *dst, const uint8 *src, const imgdesc_s &des_info, const imgdesc_s &sou_info, int oalphao)
{
    ASSERT(sou_info.bytepp() == 4);

    if (des_info.bytepp() == 3)
    {

        for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, src += sou_info.pitch)
        {
            uint8 *des = dst;
            const uint32 *sou = (uint32 *)src;
            for (int x = 0; x < des_info.sz.x; x++, des += des_info.bytepp(), ++sou)
            {
                uint32 color = *sou;
                uint32 ocolor = uint32(*(uint16 *)des) | (uint32(*((uint8 *)des + 2)) << 16);

                uint8 alpha = uint8(color >> 24);

                if (alpha == 0) continue;

                if (alpha == 255)
                {
                    ocolor = color;
                }
                else
                {
                    uint8 R = as_byte(color >> 16);
                    uint8 G = as_byte(color >> 8);
                    uint8 B = as_byte(color);

                    uint8 oR = as_byte(ocolor >> 16);
                    uint8 oG = as_byte(ocolor >> 8);
                    uint8 oB = as_byte(ocolor);


                    float A = float(double(alpha) * (1.0 / 255.0));
                    float nA = 1.0f - A;

#define CCGOOD( C, oC ) CLAMP<uint8>( (uint)lround(float(C) * A) + float(oC) * nA )

                    ocolor = CCGOOD(B, oB) | (CCGOOD(G, oG) << 8) | (CCGOOD(R, oR) << 16);
#undef CCGOOD
                }

                *des = as_byte(ocolor);
                *(des + 1) = as_byte(ocolor >> 8);
                *(des + 2) = as_byte(ocolor >> 16);
            }
        }
    }
    else
    {

        if (oalphao < 0)
        {
            for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, src += sou_info.pitch)
            {
                uint8 *des = dst;
                const uint32 *sou = (uint32 *)src;
                for (int x = 0; x < des_info.sz.x; x++, des += des_info.bytepp(), ++sou)
                {
                    uint32 ocolor;
                    uint32 color = *sou;
                    uint8 alpha = uint8(color >> 24);
                    if (alpha == 0) continue;
                    if (alpha == 255)
                    {
                        ocolor = color;
                    }
                    else
                    {

                        uint8 R = as_byte(color >> 16);
                        uint8 G = as_byte(color >> 8);
                        uint8 B = as_byte(color);

                        uint32 ocolor_cur = *(uint32 *)des;
                        uint8 oalpha = as_byte(ocolor_cur >> 24);
                        uint8 oR = as_byte(ocolor_cur >> 16);
                        uint8 oG = as_byte(ocolor_cur >> 8);
                        uint8 oB = as_byte(ocolor_cur);

                        float A = float(alpha) / 255.0f;
                        float nA = 1.0f - A;

#define CCGOOD( C, oC ) CLAMP<uint8>( (uint)lround(float(C) * A) + float(oC) * nA )
                        uint oiA = (uint)lround(oalpha + float(255 - oalpha) * A);
                        ocolor = CCGOOD(B, oB) | (CCGOOD(G, oG) << 8) | (CCGOOD(R, oR) << 16) | (CLAMP<uint8>(oiA) << 24);
#undef CCGOOD

                    }

                    *(uint32 *)des = ocolor;
                }
            }
        }
        else
        {
            for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, src += sou_info.pitch)
            {
                uint8 *des = dst;
                const uint32 *sou = (uint32 *)src;
                for (int x = 0; x < des_info.sz.x; x++, des += des_info.bytepp(), ++sou)
                {
                    uint32 ocolor;
                    uint32 color = *sou;
                    uint8 alpha = uint8(color >> 24);
                    if (alpha == 0) continue;
                    if (alpha == 255)
                    {
                        ocolor = color;
                    }
                    else
                    {

                        uint8 R = as_byte(color >> 16);
                        uint8 G = as_byte(color >> 8);
                        uint8 B = as_byte(color);

                        uint32 ocolor_cur = *(uint32 *)des;
                        uint8 oR = as_byte(ocolor_cur >> 16);
                        uint8 oG = as_byte(ocolor_cur >> 8);
                        uint8 oB = as_byte(ocolor_cur);

                        float A = float(alpha) / 255.0f;
                        float nA = 1.0f - A;


                        int oiB = lround(float(B) * A + float(oB) * nA);
                        int oiG = lround(float(G) * A + float(oG) * nA);
                        int oiR = lround(float(R) * A + float(oR) * nA);

                        int oiA = lround(oalphao + float(255 - oalphao) * A);

                        ocolor = ((oiB > 255) ? 255 : oiB) | (((oiG > 255) ? 255 : oiG) << 8) | (((oiR > 255) ? 255 : oiR) << 16) | (((oiA > 255) ? 255 : oiA) << 24);

                    }

                    *(uint32 *)des = ocolor;
                }

            }
        }
    }

}

#pragma warning (pop)


void    bmpcore_normal_s::before_modify(bitmap_c *me)
{
    if (m_core->m_ref == 1) return;

    bitmap_c b( *me );

    if (b.info().bitpp == 8) me->create_grayscale(b.info().sz);
    else if (b.info().bitpp == 15) me->create_15(b.info().sz);
    else if (b.info().bitpp == 16) me->create_16(b.info().sz);
    else if (b.info().bitpp == 24) me->create_RGB(b.info().sz);
    if (b.info().bitpp == 32) me->create_RGBA(b.info().sz);

    me->copy(ivec2(0), b.info().sz, b, ivec2(0));
}

bool bmpcore_normal_s::operator==(const bmpcore_normal_s & bm) const
{
    if (m_core == bm.m_core) return true;

    if (m_core->m_info != bm.m_core->m_info) return false;
    aint ln = m_core->m_info.sz.x * m_core->m_info.bytepp();
    if ( m_core->m_info.pitch == ln && bm.m_core->m_info.pitch == ln )
    {
        int sz = m_core->m_info.pitch * m_core->m_info.sz.y;
        return 0 == memcmp(m_core + 1, bm.m_core + 1, sz);
    }
    
    const uint8 *d1 = (const uint8 *)m_core + 1;
    const uint8 *d2 = (const uint8 *)bm.m_core + 1;
    aint cnt = m_core->m_info.sz.y;
    for(aint i=0;i<cnt;++i)
    {
        if (0 != memcmp(d1, d2, ln))
            return false;
        d1 += m_core->m_info.pitch;
        d2 += bm.m_core->m_info.pitch;
    }
    return true;
}

bool bmpcore_exbody_s::operator==(const bmpcore_exbody_s & bm) const
{
    if (m_body == bm.m_body) return m_info == bm.m_info;
    if (m_info != bm.m_info) return false;

    aint ln = m_info.sz.x * m_info.bytepp();
    if (m_info.pitch == ln && bm.m_info.pitch == ln)
    {
        int sz = m_info.pitch * m_info.sz.y;
        return 0 == memcmp(m_body, bm.m_body, sz);
    }

    const uint8 *d1 = (const uint8 *)m_body;
    const uint8 *d2 = (const uint8 *)bm.m_body;
    aint cnt = m_info.sz.y;
    for (aint i = 0; i < cnt; ++i)
    {
        if (0 != memcmp(d1, d2, ln))
            return false;
        d1 += m_info.pitch;
        d2 += bm.m_info.pitch;
    }
    return true;
}


#pragma warning (push)
#pragma warning (disable : 4731)

template <typename CORE> void bitmap_t<CORE>::convert_24to32(bitmap_c &imgout) const
{
    if (info().bitpp != 24) return;

	if (info().pitch * info().sz.y % 12 != 0)//дл€ маленьких изображений (напр. 1x1 или 2x3)
	{
		imgout.create_RGBA(info().sz);
		this->copy_components(imgout, 3, 0, 0);
		imgout.fill_alpha(0xFF);
		return;
	}

    int cnt = info().pitch * info().sz.y / 12;
    void *src = body();

    imgout.create( info().sz, 32 );

    void *dst = imgout.body();

    _asm
    {
        push ebp
        mov esi, src;
        mov edi, dst
        mov ebp, cnt

loop_1112:
        mov eax, [esi]
        mov ebx, [esi + 4]
        mov ecx, [esi + 8]

        //  R  G  B   R  G  B   R  G  B   R  G  B
        //  == == ==  == == ==  == == ==  == == ==
        //  C3 C2 C1  C0 B3 B2  B1 B0 A3  A2 A1 A0

        mov edx, eax
        or  edx, 0xFF000000
        mov [edi], edx

        mov ax, bx              // в eax : BxRG
        rol eax, 8              // в eax : xRGB
        or  eax, 0xFF000000
        mov [edi+4], eax

        mov bl, cl              // GBxR
        ror ebx, 16             // xRGB
        or  ebx, 0xFF000000
        mov [edi+8], ebx

        shr ecx, 8              // 
        or  ecx, 0xFF000000
        mov [edi+12], ecx

        add esi, 12
        add edi, 16

        dec ebp
        jnz loop_1112;


        pop ebp
    }
}
#pragma warning (pop)

#if 0

template <typename CORE> void bitmap_t<CORE>::convert_32to16(bitmap_c &imgout) const
{
    if (info().bitpp != 32) return;

    int cnt = info().pitch * info().sz.y / info().bytepp();
    uint32 *src = (uint32 *)body();
    uint16  *dst;
    int newp = m_core->m_pitch >> 1;
    if (m_core->m_ref > 1)
    {
        int sz = m_core->m_size.x * 2 * m_core->m_size.y;
        m_core->ref_dec();
        create_normal( sz );
    }

    dst = (uint16 *)body();


    while (cnt-- > 0)
    {

        uint32 s = *(src);
        *dst = uint16( ((s >> 8) & 0xF800) |
                     ((s >> 5) & 0x07E0) |
                     ((s >> 3) & 0x001F) );

        ++src;
        ++dst;
    }
    m_core->m_bitPP = 16;
    m_core->m_bytePP = 2;
    m_core->m_pitch = newp;

}

template <typename CORE> void bitmap_t<CORE>::convert_32to24(bitmap_c &imgout) const
{
	if (info().bitpp != 32) return;

	int cnt = m_core->m_pitch * m_core->m_size.y / m_core->m_bytePP;
	uint32 *src = (uint32 *)body();
	int newp = m_core->m_pitch * 3 / 4;
	if (m_core->m_ref > 1)
	{
		int sz = m_core->m_size.x * 3 * m_core->m_size.y;
		m_core->ref_dec();
		create_normal( sz );
	}

	uint8 *dst = body();

	while (cnt--)
		*((uint32*)dst) = *src++, dst += 3;

	m_core->m_bitPP = 24;
	m_core->m_bytePP = 3;
	m_core->m_pitch = newp;
}

template<typename CORE> void bitmap_t<CORE>::crop(const ivec2 & left_up,const ivec2 & size)
{
    int nx = size.x;
    int ny = size.y;
    
    int op = info().pitch;
    int obytepp = info().bytepp();
    int obitpp = info().bitpp;

    const uint8 *sou = body() + info().pitch * left_up.y + obytepp * left_up.x;
    if (m_core->m_ref > 1)
    {
        int sz = nx * obytepp * ny;
        m_core->ref_dec();
        create_normal( sz );

        m_core->m_bitPP = obitpp;
        m_core->m_bytePP = obytepp;
    }
    uint8  *des = body();

    m_core->m_size.x = nx;
    m_core->m_size.y = ny;
    m_core->m_pitch = obytepp * nx;
    copy(des,sou,nx,ny,m_core->m_pitch,obytepp,op,obytepp);
}

template<typename CORE> void bitmap_t<CORE>::crop_to_square()
{
    if (info().sz.x > info().sz.y)
    {
        int d = info().sz.x - info().sz.y;
        int d0 = d/2;
        int d1 = d-d0;
        crop( ivec2(d0,0), ivec2(info().sz.x-d0-d1,info().sz.y) );

    } else if (info().sz.x < info().sz.y)
    {
        int d = info().sz.y - info().sz.x;
        int d0 = d/2;
        int d1 = d-d0;
        crop( ivec2(0, d0), ivec2(info().sz.x,info().sz.y-d0-d1) );
    }
}

template<typename CORE> void bitmap_t<CORE>::make_2x_smaller(bitmap_c &des_bitmap, const ivec2 & lu, const ivec2 & sz) const 
{
    if(info().bytepp()==2) return;

    ivec2 newsz = sz / 2;

    if(des_bitmap.info().sz != newsz || des_bitmap.info().bytepp() != info().bytepp() || des_bitmap.m_core->m_ref > 1)
    {
        if(info().bytepp()==1) des_bitmap.create_grayscale(newsz);
        else if(info().bytepp()==3) des_bitmap.create_RGB(newsz);
        else if(info().bytepp()==4) des_bitmap.create_RGBA(newsz);
    }

    uint8 * des=des_bitmap.body();
    const uint8 * sou=body() + lu.x * info().bytepp() + info().pitch*lu.y;

    img_helper_make_2x_smaller(des, sou, des_bitmap.info(), info());
}


template<typename CORE> void bitmap_t<CORE>::make_larger(int factor, bitmap_t<CORE> &out) const
{
	ivec2 newsz = size() * factor;

    if(out.info().sz.x != newx || out.info().sz.y != newy || out.info().bytepp() != info().bytepp() || out.m_core->m_ref > 1)
    {
        if(info().bytepp()==1) out.create_grayscale(newx,newy);
        else if(info().bytepp()==2) out.create_16(newx,newy);
        else if(info().bytepp()==3) out.create_RGB(newx,newy);
		else if(info().bytepp()==4) out.create_RGBA(newx,newy);
	}

	uint8 * des = out.body();;
	const uint8 * sou = body();

    int add = info().bytepp()*factor;
    int addl = info().pitch*factor;

    if(info().bytepp()==1)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                memset(des,*((uint8 *)sou + x), factor);
		    }

            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==2)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint16 w = *(uint16 *)(sou + x * 2);
                for (int k = 0; k<factor; ++k)
                {
                    *(((uint16 *)des) + k) = w;
                }
		    }			
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==3)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint16 w = *(uint16 *)(sou + x * 3);
                uint8 b = *(uint8 *)(sou + x * 3 + 2);
                for (int k = 0; k<factor; ++k)
                {
                    *((uint16 *)(des + k*3)) = w;
                    *((uint8 *)(des + k*3 + 2)) = b;
                }
		    }			
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==4)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint32 dw = *(uint32 *)(sou + x * 4);
                for (int k = 0; k<factor; ++k)
                {
                    *(((uint32 *)des) + k) = dw;
                }
            }
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
        }
    }
}

#endif

template<typename CORE> irect bitmap_t<CORE>::calc_visible_rect() const
{
    irect r;
    r.lt = info().sz - 1;
    r.rb.x = 0;
    r.rb.y = 0;

    uint32 * ptr=(uint32 *)body();

    for (int j = 0; j<info().sz.y; ++j)
    {
        for (int i = 0; i<info().sz.x; ++i)
        {
            uint32 ap = (0xFF000000 & (*(ptr + i)));
            bool t_empty_found = (ap == 0x00000000);
            //bool t_solid_found = (ap == 0xFF000000);

            if (!t_empty_found)
            {
				if (i < r.lt.x) r.lt.x = i;
				if (i > r.rb.x) r.rb.x = i;
				if (j < r.lt.y) r.lt.y = j;
				if (j > r.rb.y) r.rb.y = j;
            }

        }
        ptr = (uint32 *)(((uint8 *)ptr) + info().pitch);
    }

    return r;
}

template<typename CORE> uint32 bitmap_t<CORE>::get_area_type(const ivec2 & p,const ivec2 & size) const
{
    if (info().bytepp()!=4) return IMAGE_AREA_SOLID;
	uint32 * ptr=(uint32 *)((uint8 *)body()+4*p.x+info().pitch*p.y);

    uint32 at = 0;

    for (int j = 0; j<size.y; ++j)
    {
        for (int i = 0; i<size.x; ++i)
        {
            uint32 ap = (0xFF000000 & (*(ptr + i)));
            bool t_empty_found = (ap == 0x00000000);
            bool t_solid_found = (ap == 0xFF000000);

            if ( !t_empty_found && !t_solid_found  ) at |=IMAGE_AREA_SEMITRANSPARENT;
            if (t_empty_found) at |= IMAGE_AREA_TRANSPARENT;
            if (t_solid_found) at |= IMAGE_AREA_SOLID;

            if (at == IMAGE_AREA_ALLTYPE) return at;
        }
        ptr = (uint32 *)(((uint8 *)ptr) + info().pitch);
    }
    return at;
}

template<typename CORE> void bitmap_t<CORE>::tile(const ivec2 & pdes,const ivec2 & desize, const bitmap_t<CORE> & bmsou,const ivec2 & spsou, const ivec2 & szsou)
{
    copy(pdes, szsou, bmsou, spsou);

    // filling by x

    int cx = szsou.x;
    while (cx < desize.x)
    {
        int w = tmin((desize.x-cx), cx);
        copy( ivec2(pdes.x + cx, pdes.y), ivec2(w,szsou.y), *this, pdes);
        cx += w;
    }

    // filling by y

    int cy = szsou.y;
    while (cy < desize.y)
    {
        int h = tmin((desize.y-cy), cy);
        copy( ivec2(pdes.x, pdes.y + cy), ivec2(desize.x,h), *this, pdes);
        cy += h;
    }
}

template<typename CORE> void bitmap_t<CORE>::get_from_dxt( const ivec2 &from_p, const void *src_bits, int src_pitch, int squish_fmt )
{
    if (ASSERT( info().bytepp() == 4 ))
        img_helper_get_from_dxt( body(), info(), from_p, src_bits, src_pitch, squish_fmt );
}

template<typename CORE> void bitmap_t<CORE>::copy_components(bitmap_c &imageout, int num_comps, int dst_first_comp, int src_first_comp) const
{
	ASSERT(imageout.info().sz == info().sz && dst_first_comp+num_comps <= imageout.info().bytepp() && src_first_comp+num_comps <= info().bytepp());
	imageout.before_modify();
	img_helper_copy_components(imageout.body() + dst_first_comp, body() + src_first_comp, imageout.info(), info(), num_comps);
}

template<typename CORE> void bitmap_t<CORE>::copy(const ivec2 &pdes, const ivec2 &size, const bitmap_t<CORE> &bmsou, const ivec2 &spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;

    if (bmsou.info().bytepp() == info().bytepp())
    {
        int len = size.x * info().bytepp();
	    for(int y=0;y<size.y;y++,des+=info().pitch,sou+=bmsou.info().pitch)
        {
            memcpy(des, sou, len);
	    }
    } else
        // TODO : fix this brunch for non 3 or 4 bytePP images
    {
        uint32 alpha = bmsou.info().bytepp()==3?0xFF000000:0;
	    for(int y=0;y<size.y;y++,des+=info().pitch,sou+=bmsou.info().pitch)
        {
            uint8 *des1 = des;
            const uint8 *sou1 = sou;
            for (int i = 0; i<(size.x-1); ++i, sou1 += bmsou.info().bytepp(), des1 += info().bytepp())
            {
                uint32 color = *((uint32 *)sou1) | alpha;
                *((uint32 *)des1) = color;
            }
            int c = tmin(bmsou.info().bytepp(), info().bytepp());
            while (c--)
            {
                *des1++ = *sou1++;
            }
            if (alpha) *des1 = 0xFF;
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::fill_alpha(uint8 a)
{
    ASSERT( info().bytepp() == 4 );
    before_modify();

    if ( info().pitch == 4 * info().sz.x )
    {
        uint8 * o = body() + 3;
        int cnt = info().sz.x * info().sz.y;
        for (; cnt > 0; --cnt, o += 4)
        {
            *o = a;
        }
    } else
        fill_alpha(ts::ivec2(0), info().sz, a);
}

template<typename CORE> void bitmap_t<CORE>::fill_alpha(const ivec2 & pdes, const ivec2 & size, uint8 a)
{
    ASSERT( info().bytepp() == 4 );
    if (pdes.x < 0 || pdes.y < 0) return;
    if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return;

    before_modify();

    uint8 * des = body() + info().bytepp()*pdes.x + info().pitch*pdes.y + 3;
    int desnl = info().pitch - size.x*info().bytepp();
    int desnp = info().bytepp();

    for (int y = 0; y < size.y; y++, des += desnl) {
        for (int x = 0; x < size.x; x++, des += desnp) {
            *(uint8 *)des = a;
        }
    }
}


template<typename CORE> void bitmap_t<CORE>::detect_alpha_channel( const bitmap_t<CORE> & bmsou )
{
    class ADFILTERFILTER
    {
    public:

        bool begin( const bitmap_t<CORE> & tgt, const bitmap_t<CORE> & src )
        {
            return src.info().bitpp == 32 && tgt.info().bitpp == 32;
        }

        void point( uint8 * me, const FMATRIX &m )
        {
            // me = black
            // from = white

            const uint8 * from = m[1][1];

            if ( me[0] == from[0] && me[1] == from[1] && me[2] == from[2] )
            {
                me[3] = 0xFF;
            } else if ( me[0] == 0 && me[1] == 0 && me[2] == 0 && from[0] == 255 && from[1] == 255 && from[2] == 255 )
            {
                me[3] = 0x0;

            } else 
            {
                int a0 = 255 - from[0] + me[0];
                int a1 = 255 - from[1] + me[1];
                int a2 = 255 - from[2] + me[2];

                int a = tmax( a0, a1, a2 );

                me[0] = as_byte( me[0] * 255 / a );
                me[1] = as_byte( me[1] * 255 / a );
                me[2] = as_byte( me[2] * 255 / a );
                me[3] = as_byte( a );

            }

        }
    };
    
    ADFILTERFILTER xx;
    bmsou.copy_with_filter(*this,xx);
}

template<typename CORE> void bitmap_t<CORE>::fill(TSCOLOR color)
{
    before_modify();
    uint8 * des=body();
    int desnl=info().pitch-info().sz.x*info().bytepp();
    int desnp=info().bytepp();

    switch( info().bytepp() )
    {
    case 1:
        for(int y=0;y<info().sz.y;y++,des+=desnl)
            for(int x=0;x<info().sz.x;x++,des+=desnp)
                *(uint8 *)des=(uint8)color;
        break;
    case 2:
        for(int y=0;y<info().sz.y;y++,des+=desnl)
            for(int x=0;x<info().sz.x;x++,des+=desnp)
                *(uint16 *)des=(uint16)color;
        break;
    case 3:
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp) {
                *(uint16 *)des=(uint16)color;
                *(uint8 *)(des+2)=(uint8)(color>>16);
            }			
        }
        break;
    case 4:
        for(int y=0;y<info().sz.y;y++,des+=desnl)
            for(int x=0;x<info().sz.x;x++,des+=desnp)
                *(uint32 *)des=color;
        break;
    }
}

template<typename CORE> void bitmap_t<CORE>::fill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color)
{
	if(pdes.x<0 || pdes.y<0 || (pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y)
    {
        ivec2 pos(pdes);
        ivec2 sz(size);
        if (pos.x < 0) { sz.x += pos.x; pos.x = 0; }
        if (pos.y < 0) { sz.y += pos.y; pos.y = 0; }
        if ((pos.x+sz.x)>info().sz.x) sz.x = info().sz.x - pos.x;
        if ((pos.y+sz.y)>info().sz.y) sz.y = info().sz.y - pos.y;
        if (sz >> 0) fill(pos,sz,color);
        return;
    }
    if (!ASSERT(size >> 0)) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	if(info().bytepp()==1) {
		for(int y=0;y<size.y;y++,des+=desnl) {
			for(int x=0;x<size.x;x++,des+=desnp) {
				*(uint8 *)des=(uint8)color;
			}			
		}
	} else if(info().bytepp()==2) {
		for(int y=0;y<size.y;y++,des+=desnl) {
			for(int x=0;x<size.x;x++,des+=desnp) {
				*(uint16 *)des=(uint16)color;
			}			
		}
	} else if(info().bytepp()==3) {
		for(int y=0;y<size.y;y++,des+=desnl) {
			for(int x=0;x<size.x;x++,des+=desnp) {
				*(uint16 *)des=(uint16)color;
				*(uint8 *)(des+2)=(uint8)(color>>16);
			}			
		}
	} else if(info().bytepp()==4) {
		for(int y=0;y<size.y;y++,des+=desnl) {
			for(int x=0;x<size.x;x++,des+=desnp) {
				*(uint32 *)des=color;
			}			
		}
	}
}

template<typename CORE> void bitmap_t<CORE>::overfill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color)
{
    if (pdes.x<0 || pdes.y<0 || (pdes.x + size.x)>info().sz.x || (pdes.y + size.y)>info().sz.y)
    {
        ivec2 pos(pdes);
        ivec2 sz(size);
        if (pos.x < 0) { sz.x += pos.x; pos.x = 0; }
        if (pos.y < 0) { sz.y += pos.y; pos.y = 0; }
        if ((pos.x + sz.x) > info().sz.x) sz.x = info().sz.x - pos.x;
        if ((pos.y + sz.y) > info().sz.y) sz.y = info().sz.y - pos.y;
        if (sz >> 0) overfill(pos, sz, color);
        return;
    }
    if (!ASSERT(size >> 0)) return;

    before_modify();

    // formula is: (1-src.alpha) * dst + src.color
    // dont check 0 alpha due it can be (1 * desc + src) op

    uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
    int desnl=info().pitch-size.x*info().bytepp();
    int desnp=info().bytepp();

    if(info().bytepp()==1) {
        // unsupported
    } else if(info().bytepp()==2) {
        // unsupported
    } else if(info().bytepp()==3) {

        double a = ((double)ALPHA(color) * 1.0 / 255.0);
        double not_a = 1.0 - a;

        for(int y=0;y<size.y;y++,des+=desnl) {
            for(int x=0;x<size.x;x++,des+=desnp) {

                auint oiB = lround(float(des[0]) * not_a) + BLUE(color);
                auint oiG = lround(float(des[1]) * not_a) + GREEN(color);
                auint oiR = lround(float(des[2]) * not_a) + RED(color);

                *(uint16 *)des = CLAMP<uint8>(oiB) | (CLAMP<uint8>(oiG) << 8);
                *(uint8 *)(des+2) = (uint8)CLAMP<uint8>(oiR);

            }			
        }
    } else if(info().bytepp()==4) {

        double a = ((double)ALPHA(color) * 1.0 / 255.0);
        double not_a = 1.0 - a;

        for(int y=0;y<size.y;y++,des+=desnl) {
            for(int x=0;x<size.x;x++,des+=desnp) {

                TSCOLOR c = *(TSCOLOR *)des;

                auint oiB = lround(float(BLUE(c)) * not_a) + BLUE(color);
                auint oiG = lround(float(GREEN(c)) * not_a) + GREEN(color);
                auint oiR = lround(float(RED(c)) * not_a) + RED(color);
                auint oiA = lround(float(ALPHA(c)) * not_a) + ALPHA(color);

                c = CLAMP<uint8>(oiB) | (CLAMP<uint8>(oiG) << 8) | (CLAMP<uint8>(oiR) << 16) | (CLAMP<uint8>(oiA) << 24);

                *(TSCOLOR *)des = c;
            }			
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::premultiply()
{
    if(info().bytepp()!=4) return;
    before_modify();

    uint8 * des=body();
    int desnl=info().pitch-info().sz.x*4;

    for(int y=0;y<info().sz.y;++y,des+=desnl)
    {
        for(int x=0;x<info().sz.x;++x,des+=4)
        {
            TSCOLOR ocolor;
            TSCOLOR color = *(TSCOLOR *)des;
            uint8 alpha = ALPHA(color);
            if(alpha==255)
            {
                continue;
            } else if (alpha==0)
            {
                ocolor = 0;
            } else
            {
                ocolor = PREMULTIPLY( color );
            }

            *(TSCOLOR *)des = ocolor;
        }			
    }

}


#if 0

template<typename CORE> void bitmap_t<CORE>::rotate_90(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+sizesou.x)>bmsou.info().sz.x || (spsou.y+sizesou.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+sizesou.y)>info().sz.x || (pdes.y+sizesou.x)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-sizesou.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(sizesou.x-1)*sounp;

	for(int y=0;y<sizesou.x;y++,des+=desnl,sou=sou-soull*sizesou.y-sounp) {
		for(int x=0;x<sizesou.y;x++,des+=desnp,sou+=soull) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::rotate_180(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou, const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+sizesou.x)>bmsou.info().sz.x || (spsou.y+sizesou.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+sizesou.x)>info().sz.x || (pdes.y+sizesou.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-sizesou.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int sounl=bmsou.info().pitch-sizesou.x*bmsou.info().bytepp();
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(sizesou.x-1)*sounp+(sizesou.y-1)*soull;

	for(int y=0;y<sizesou.y;y++,des+=desnl,sou=sou-sounl) {
		for(int x=0;x<sizesou.x;x++,des+=desnp,sou-=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::rotate_270(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou, const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+sizesou.x)>bmsou.info().sz.x || (spsou.y+sizesou.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+sizesou.y)>info().sz.x || (pdes.y+sizesou.x)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-sizesou.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	//int sounl=bmsou.info().pitch-sizesou.x*bmsou.info().bytepp();
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(sizesou.y-1)*soull;


	for(int y=0;y<sizesou.x;y++,des+=desnl,sou=sou+soull*sizesou.y+sounp) {
		for(int x=0;x<sizesou.y;x++,des+=desnp,sou-=soull) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

#endif

template<typename CORE> void bitmap_t<CORE>::flip_x(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify(); //TODO: multicore optimizations

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(size.x-1)*sounp;

	for(int y=0;y<size.y;y++,des+=desnl,sou=sou+soull+(size.x)*sounp) {
		for(int x=0;x<size.x;x++,des+=desnp,sou-=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::flip_y(void)
{
    before_modify(); //TODO: multicore optimizations

    uint8 *l1 = body();
    uint8 *l2 = body() + info().pitch * (info().sz.y-1);
    
    while (l1<l2)
    {
        int cnt = info().pitch;
        uint8 *p0 = l1;
        uint8 *p1 = l2;
        while (cnt >= 4)
        {
            uint32 temp = *(uint32 *)p0;
            *(uint32 *)p0 = *(uint32 *)p1;
            *(uint32 *)p1 = temp;
            p0 += 4;
            p1 += 4;
            cnt -= 4;
        }
        while (cnt > 0)
        {
            BYTE temp = *p0;
            *p0 = *p1;
            *p1 = temp;
            ++p0;
            ++p1;
            --cnt;
        }

        l1+=info().pitch;
        l2-=info().pitch;
    }

}

template<typename CORE> void bitmap_t<CORE>::flip_y(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int sounl=bmsou.info().pitch-size.x*bmsou.info().bytepp();
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(size.y-1)*soull;



	for(int y=0;y<size.y;y++,des+=desnl,sou=sou+sounl-soull*2) {
		for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::flip_y(const ivec2 & pdes,const ivec2 & size)
{
	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int sounl=info().pitch-size.x*info().bytepp();
	int soull=info().pitch;
	int sounp=info().bytepp();

	sou=sou+(size.y-1)*soull;

	uint32 t;

	for(int y=0;y<(size.y >> 1);y++,des+=desnl,sou=sou+sounl-soull*2) {
		for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp) {
			switch(info().bytepp()) {
				case 1: 
					t=*(uint8 *)des;
					*(uint8 *)des=*(uint8 *)sou; 
					*(uint8 *)sou=BYTE(t);
					break;
				case 2: 
					t=*(uint16 *)des;
					*(uint16 *)des=*(uint16 *)sou; 
					*(uint16 *)sou=uint16(t);
					break;
				case 3: 
					t=*(uint16 *)des;
					*(uint16 *)des=*(uint16 *)sou; 
					*(uint16 *)sou=uint16(t);

					t=*(uint8 *)(des+2);
					*(uint8 *)(des+2)=*(uint8 *)(sou+2); 
					*(uint8 *)(sou+2)=BYTE(t);
					break;
				case 4: 
					t=*(uint32 *)des;
					*(uint32 *)des=*(uint32 *)sou;
					*(uint32 *)sou=t;
					break;
			}
		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::merge_by_mask(const ivec2 & pdes,const ivec2 & size,
							const bitmap_t<CORE> & bm1,const ivec2 & sp1,
							const bitmap_t<CORE> & bm2,const ivec2 & sp2,
							const bitmap_t<CORE> & mask,const ivec2 & spm)
{
	if(sp1.x<0 || sp1.y<0) return;
	if((sp1.x+size.x)>bm1.info().sz.x || (sp1.y+size.y)>bm1.info().sz.y) return;

	if(sp2.x<0 || sp2.y<0) return;
	if((sp2.x+size.x)>bm2.info().sz.x || (sp2.y+size.y)>bm2.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bm1.info().bitpp!=bm2.info().bitpp) return;
	if(bm1.info().bitpp!=info().bitpp) return;

	if(mask.info().bitpp<8) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();

	const uint8 * sou1=bm1.body()+bm1.info().bytepp()*sp1.x+bm1.info().pitch*sp1.y;
	int sou1nl=bm1.info().pitch-size.x*bm1.info().bytepp();

	uint8 * sou2=bm2.body()+bm2.info().bytepp()*sp2.x+bm2.info().pitch*sp2.y;
	int sou2nl=bm2.info().pitch-size.x*bm2.info().bytepp();

	uint8 * m=mask.body()+mask.info().bytepp()*spm.x+mask.info().pitch*spm.y;
	int mnl=mask.info().pitch-size.x*mask.info().bytepp();
	int mnp=mask.info().bytepp();

	for(int y=0;y<size.y;y++,des+=desnl,sou1+=sou1nl,sou2+=sou2nl,m+=mnl) {
		for(int x=0;x<size.x;x++,m+=mnp) {
			for(int i=0;i<bm1.info().bytepp();i++,des++,sou1++,sou2++) {
				if(*m==0) *des=*sou2;
				else if(*m==255) *des=*sou1;
				else *des=   uint8((uint32(*sou1)*(uint32(*m)<<8))>>16)
							+uint8((uint32(*sou2)*(uint32(255-*m)<<8))>>16); // не совсем точна€ формула =(

			}
		}
	}
}

template<typename CORE> void bitmap_t<CORE>::swap_byte(const ivec2 & pos,const ivec2 & size,int n1,int n2)
{
	if(n1==n2) return;
	if(n1<0 || n1>=info().bytepp()) return;
	if(n2<0 || n2>=info().bytepp()) return;
	if(info().bytepp()<=1) return;

    before_modify();

	uint8 * buf=body()+info().bytepp()*pos.x+info().pitch*pos.y;
	int bufnl=info().pitch-size.x*info().bytepp();
	int bufnp=info().bytepp();

	uint8 zn;

	for(int y=0;y<size.y;y++,buf+=bufnl) {
		for(int x=0;x<size.x;x++,buf+=bufnp) {
			zn=*(buf+n1); *(buf+n1)=*(buf+n2); *(buf+n2)=zn;
		}
	}
}

#if 0
template<typename CORE> void bitmap_t<CORE>::merge_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_c & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(info().bytepp()!=3 && info().bytepp()!=4) return;
	if(bmsou.info().bytepp()!=4) return;

    before_modify();

    merge_with_alpha( 
        body()+info().bytepp()*pdes.x+info().pitch*pdes.y,
        bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y,
        size.x,
        size.y,
        info().pitch,
        info().bytepp(),
        bmsou.info().pitch
        );
}

template<typename CORE> void bitmap_t<CORE>::merge_with_alpha_PM(const ivec2 & pdes,const ivec2 & size, const bitmap_c & bmsou,const ivec2 & spsou)
{

	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=4) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int sounl=bmsou.info().pitch-size.x*bmsou.info().bytepp();
	int sounp=bmsou.info().bytepp();


	for(int y=0;y<size.y;y++,des+=desnl,sou+=sounl)
    {
        for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp)
        {
			uint32 color=*(uint32 *)sou;
            uint32 ocolor = *des + (*(des+1) << 8) + (*(des+2) << 16);
			uint8 alpha = uint8(color>>24);
            uint8 R = uint8(color>>16);
            uint8 G = uint8(color>>8);
            uint8 B = uint8(color);

			uint8 oalpha = uint8(color>>24);
            uint8 oR = uint8(ocolor>>16);
            uint8 oG = uint8(ocolor>>8);
            uint8 oB = uint8(ocolor);

            if (alpha==0) continue;
			else
            if(alpha==255)
            {
                ocolor = color;
            } else
            {
                float A = alpha / 255.0f;

			    //*des=   uint8((uint32(B)*(uint32(alpha)<<8))>>16)+uint8((uint32(oB)*(uint32(255-alpha)<<8))>>16); // не совсем точна€ формула =(

                auint oiB = lround(float(B) + float(oB) * (1.0f - A));
                auint oiG = lround(float(G) + float(oG) * (1.0f - A));
                auint oiR = lround(float(R) + float(oR) * (1.0f - A));

                auint oiA = lround(float(255-oalpha) * A);

				ocolor = CLAMP<uint8>(oiB) | (CLAMP<uint8>(oiG) << 8) | (CLAMP<uint8>(oiR) << 16) | (CLAMP<uint8>(oiA) << 24);

                
                /*
                *des=   uint8((uint32(*sou)*(uint32(alpha)<<8))>>16)
						    +uint8((uint32(*des)*(uint32(255-alpha)<<8))>>16); // не совсем точна€ формула =(
			    sou++; des++;
			    *des=   uint8((uint32(*sou)*(uint32(alpha)<<8))>>16)
						    +uint8((uint32(*des)*(uint32(255-alpha)<<8))>>16); // не совсем точна€ формула =(
			    sou++; des++;

			    *des+=uint8(((255-*des)*(uint32(alpha)<<8))>>16);
                */
            }
            if (info().bytepp() == 3)
            {
                *des = uint8(ocolor);
                *(des+1) = uint8(ocolor>>8);
                *(des+2) = uint8(ocolor>>16);
            } else
            {

                *(uint32 *)des = ocolor;
            }

		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::merge_grayscale_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> * data_src,const ivec2 & data_src_point, const bitmap_t<CORE> * alpha_src, const ivec2 & alpha_src_point)
{
    uint8 * dsou = nullptr;
    int dsounl = 0;

	if(data_src == nullptr)
    {
    } else
    {
        ASSERT(data_src->info().bitpp == 8);
	    dsou = data_src->body() + data_src_point.x + data_src->info().pitch*data_src_point.y;
	    dsounl=data_src->info().pitch-size.x;
    }
    ASSERT(alpha_src != nullptr && info().bitpp == 8);
    if (alpha_src->info().bitpp != 32) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x;

	const uint8 * asou=alpha_src->body() + alpha_src_point.x * 4 + alpha_src->info().pitch*alpha_src_point.y + 3;
	int asounl=alpha_src->info().pitch-size.x*4;

    if (dsou == nullptr)
    {
	    for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl)
        {
		    for(int x=0;x<size.x;x++,++des, asou+=4)
            {
			    uint8 alpha = *asou;
                uint8 odata;

                if (alpha==0) continue;
			    else
                if(alpha==255)
                {
                    odata = 0;
                } else
                {
                    odata = uint8(float(255-alpha)*float(*des)*(1.0/255.0));
                }
                
                *des = odata;

		    }			
	    }
        return;
    }

	for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl, dsou+=dsounl)
    {
		for(int x=0;x<size.x;x++,++des,++dsou, asou+=4)
        {
			uint8 alpha = *asou;
            uint8 odata;

            if (alpha==0) continue;
			else
            if(alpha==255)
            {
                odata = *dsou;
            } else
            {
                odata = uint8((float(255-alpha)*float(*des) + float(alpha)*float(*dsou))*(1.0/255.0));
            }
            
            *des = odata;

		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::merge_grayscale_with_alpha_PM(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> * data_src,const ivec2 & data_src_point, const bitmap_t<CORE> * alpha_src, const ivec2 & alpha_src_point)
{
    uint8 * dsou = nullptr;
    int dsounl = 0;

	if(data_src == nullptr)
    {
    } else
    {
        ASSERT(data_src->info().bitpp == 8);
	    dsou = data_src->body() + data_src_point.x + data_src->info().pitch*data_src_point.y;
	    dsounl=data_src->info().pitch-size.x;
    }
    ASSERT(alpha_src != nullptr && info().bitpp == 8);
    if (alpha_src->info().bitpp != 32) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x;

	const uint8 * asou=alpha_src->body() + alpha_src_point.x * 4 + alpha_src->info().pitch*alpha_src_point.y + 3;
	int asounl=alpha_src->info().pitch-size.x*4;

    if (dsou == nullptr)
    {
	    for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl)
        {
		    for(int x=0;x<size.x;x++,++des, asou+=4)
            {
			    uint8 alpha = *asou;
                uint8 odata;

                if (alpha==0) continue;
			    else
                if(alpha==255)
                {
                    odata = 0;
                } else
                {
                    odata = uint8(float(255-alpha)*float(*des)*(1.0/255.0));
                }
                
                *des = odata;

		    }			
	    }
        return;
    }

	for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl, dsou+=dsounl)
    {
		for(int x=0;x<size.x;x++,++des,++dsou, asou+=4)
        {
            *des = uint8((float(255 - *asou)*float(*des) + 255.0*float(*dsou))*(1.0/255.0));
		}			
	}

}


template<typename CORE> void bitmap_t<CORE>::modulate_grayscale_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> * alpha_src, const ivec2 & alpha_src_point)
{
    ASSERT(alpha_src!=nullptr && info().bitpp == 8);

    if (alpha_src->info().bitpp != 32) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x;

	const uint8 * asou=alpha_src->body() + alpha_src_point.x * 4 + alpha_src->info().pitch*alpha_src_point.y + 3;
	int asounl=alpha_src->info().pitch-size.x*4;

	for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl)
    {
		for(int x=0;x<size.x;x++,++des, asou+=4)
        {
			uint8 alpha = *asou;

            if (alpha==255) continue;
			else
            {
                *des = uint8((float(alpha)*float(*des))*(1.0/255.0));
            }

		}			
	}
}

#endif

void TSCALL sharpen_run(bitmap_c &obm, const uint8 *sou, const imgdesc_s &souinfo, int lv);
template<typename CORE> void bitmap_t<CORE>::sharpen(bitmap_c& outimage, int lv) const
{
    outimage.create_RGBA(info().sz);
    sharpen_run(outimage, body(), info(), lv);
}

#if 0
bool rotate2(bitmap_c &obm, bitmap_c &ibm, int angle, rot_filter_e filt_mode, bool expand_dst=true);

// Angle in [-pi..pi]
template<typename CORE> bool bitmap_t<CORE>::rotate(bitmap_c& obm, float angle_rad, rot_filter_e filt_mode, bool expand_dst)
{
	int ang = lround(angle_rad * 2147483648.0 / 3.14159265358979323846);
	return rotate2(obm, *this, ang, filt_mode,expand_dst);
}

bool resize2(bitmap_c &obm, bitmap_c &ibm, float scale = 2.f, resize_filter_e filt_mode = FILTER_NONE);

template<typename CORE> bool bitmap_t<CORE>::resize(bitmap_c& obm, float scale, resize_filter_e filt_mode)
{
	return resize2(obm, *this, scale, filt_mode);
}

#endif

bool resize3(bitmap_c &obm, const uint8 *sou, const imgdesc_s &souinfo, const ivec2 &newsz, resize_filter_e filt_mode);
template<typename CORE> bool bitmap_t<CORE>::resize(bitmap_c& obm, const ivec2 & newsize, resize_filter_e filt_mode) const
{
    return resize3(obm, body(), info(), newsize, filt_mode);
}

template<typename CORE> void bitmap_t<CORE>::make_grayscale(void)
{
    if (info().bytepp() != 4 || info().bitpp != 32) return;
    before_modify();

    uint8 * des=body();
    int desnl=info().pitch-info().sz.x*info().bytepp();
    int desnp=info().bytepp();

    if(info().bytepp()==3)
    {
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp)
            {
                uint32 color = *(uint32 *)des;

                int  R = as_byte( color>>16 );
                int  G = as_byte( color>>8 );
                int  B = as_byte( color );

                // 0.3 0.5 0.2
                int X = (R * 350 + G * 450 + B * 200) / 1000;

                uint32 ocolor  = (X) | (X<<8) | (X<<16);


                *(uint16 *)des=(uint16)ocolor;
                *(uint8 *)(des+2)=(uint8)(ocolor>>16);
            }			
        }
    } else
    {
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp)
            {
                uint32 color = *(uint32 *)des;

                int  A = as_byte( color>>24 );
                int  R = as_byte( color>>16 );
                int  G = as_byte( color>>8 );
                int  B = as_byte( color );

                // 0.3 0.5 0.2
                int X = (R * 350 + G * 450 + B * 200) / 1000;

                *(uint32 *)des=(X) | (X<<8) | (X<<16) | (A<<24);
            }			
        }
    }
}


template<typename CORE> void bitmap_t<CORE>::save_as_BMP(const wsptr &filename) const
{
    buf_c buf;
    save_as_BMP(buf);
    buf.save_to_file( filename );
}

template<typename CORE> void bitmap_t<CORE>::save_as_BMP(buf_c & buf) const
{
	BITMAPFILEHEADER bmFileHeader = {0};
	BITMAPINFOHEADER bmInfoHeader;

	if(info().bytepp()!=1 && info().bytepp()!=3 && info().bytepp()!=4) return;
	if((info().bitpp>>3)!=info().bytepp()) return;

	int lPitch=((info().bytepp()*info().sz.x-1)/4+1)*4;

	bmFileHeader.bfType=0x4D42;
	bmFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+lPitch*info().sz.y;
	bmFileHeader.bfOffBits=bmFileHeader.bfSize-lPitch*info().sz.y;

	buf.append_buf(&bmFileHeader, sizeof(BITMAPFILEHEADER));

	memset(&bmInfoHeader, 0, sizeof(BITMAPINFOHEADER));
	bmInfoHeader.biSize   = sizeof(BITMAPINFOHEADER);
	bmInfoHeader.biWidth  = info().sz.x;
	bmInfoHeader.biHeight = info().sz.y;
	bmInfoHeader.biPlanes = 1;
	bmInfoHeader.biBitCount=(uint16)info().bitpp;
	bmInfoHeader.biCompression=0;
	bmInfoHeader.biSizeImage=lPitch*info().sz.y;
	buf.append_buf(&bmInfoHeader, sizeof(BITMAPINFOHEADER));

	uint8 * sou=body()+(info().sz.y-1)*info().pitch;

	//if((info().bytepp()==3 || info().bytepp()==4) && m_MColor[0]==0x0ff && m_MColor[1]==0x0ff00 && m_MColor[2]==0x0ff0000) SwapByte(ivec2(0,0),m_Size,0,2);

	int len=info().bytepp()*info().sz.x;
    if (info().bytepp()==3)
    {
	    for(int y=0;y<info().sz.y;y++,sou-=info().pitch)
        {
            uint8 *sb = sou;
            uint8 *db = (uint8 *)buf.expand( len );
            BYTE *de = db + len;

            for (;db < de; db += 3, sb += 3)
            {
                *(db + 0) = *(sb + 0);
                *(db + 1) = *(sb + 1);
                *(db + 2) = *(sb + 2);
            }
		
            if(len<lPitch)
            {
                int sz = lPitch-len;
                memset( buf.expand( sz ), 0, sz );
            }
        }
    } else if (info().bytepp()==4)
    {
	    for(int y=0;y<info().sz.y;y++,sou-=info().pitch)
        {
            uint8 *sb = sou;
            uint8 *db = (uint8 *)buf.expand( len );
            BYTE *de = db + len;

            for (;db < de; db += 4, sb += 4)
            {
                *(db + 0) = *(sb + 0);
                *(db + 1) = *(sb + 1);
                *(db + 2) = *(sb + 2);
                *(db + 3) = *(sb + 3);
            }
		
            if(len<lPitch)
            {
                int sz = lPitch-len;
                memset( buf.expand( sz ), 0, sz );
            }
        }

    } else
    {
	    for(int y=0;y<info().sz.y;y++,sou-=info().pitch)
        {
		    buf.append_buf(sou,len);
            if(len<lPitch)
            {
                int sz = lPitch-len;
                memset( buf.expand( sz ), 0, sz );
            }
	    }
    }

	//if((info().bytepp()==3 || info().bytepp()==4) && m_MColor[0]==0x0ff && m_MColor[1]==0x0ff00 && m_MColor[2]==0x0ff0000) SwapByte(ivec2(0,0),m_Size,0,2);
}

namespace
{
    struct jpgsavebuf : public jpeg_destination_mgr
    {
        static const int BLOCK_SIZE = 4096 / sizeof(JOCTET);

        buf_c &buf;
        jpgsavebuf(buf_c &buf):buf(buf)
        {
            buf.clear();

            init_destination = sljpg_init_destination;
            empty_output_buffer = sljpg_empty_output_buffer;
            term_destination = sljpg_term_destination;

        }
        jpgsavebuf &operator=(const jpgsavebuf &) UNUSED;

        static void sljpg_init_destination(j_compress_ptr cinfo)
        {
            jpgsavebuf *me = (jpgsavebuf *)cinfo->dest;
            me->next_output_byte = (JOCTET *)me->buf.expand(BLOCK_SIZE);
            me->free_in_buffer = BLOCK_SIZE;
        }

        static boolean sljpg_empty_output_buffer(j_compress_ptr cinfo)
        {
            jpgsavebuf *me = (jpgsavebuf *)cinfo->dest;
            me->next_output_byte = (JOCTET *)me->buf.expand(BLOCK_SIZE);
            me->free_in_buffer = BLOCK_SIZE;
            return true;
        }

        static void sljpg_term_destination(j_compress_ptr cinfo)
        {
            jpgsavebuf *me = (jpgsavebuf *)cinfo->dest;
            me->buf.set_size(me->buf.size() - me->free_in_buffer);
        }

    };

    struct jpgreadbuf : public jpeg_source_mgr
    {
        static const int BLOCK_SIZE = 4096 / sizeof(JOCTET);

        const JOCTET * buf;
        int buflen;
        uint curp;
        JOCTET fakeend[2];
        jpgreadbuf(const void * buf, int buflen):buf((const JOCTET *)buf), buflen(buflen)
        {
            curp = 0;
            init_source = sljpg_init_source;
            fill_input_buffer = sljpg_fill_input_buffer;
            skip_input_data = sljpg_skip_input_data;
            resync_to_restart = jpeg_resync_to_restart; /* use default method */
            term_source = sljpg_term_source;
            bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
            next_input_byte = nullptr; /* until buffer loaded */
            fakeend[0] = (JOCTET) 0xFF;
            fakeend[1] = (JOCTET) JPEG_EOI;

        }
        jpgreadbuf &operator=(const jpgreadbuf &) UNUSED;


        static void sljpg_init_source(j_decompress_ptr cinfo)
        {
            jpgreadbuf *me = (jpgreadbuf *)cinfo->src;
            me->curp = 0;
        }

        static boolean sljpg_fill_input_buffer (j_decompress_ptr cinfo)
        {
            jpgreadbuf *src = (jpgreadbuf *)cinfo->src;
            
            if ((int)src->curp >= src->buflen)
            {
                src->next_input_byte = src->fakeend;
                src->bytes_in_buffer = 2;
                return TRUE;
            } else
            {
                src->next_input_byte = src->buf;
                src->bytes_in_buffer = src->buflen;
                src->curp = src->bytes_in_buffer;
                return TRUE;
            }
            //return TRUE;
        }

        static void sljpg_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
        {
            __debugbreak();
        }
        static void sljpg_term_source (j_decompress_ptr cinfo)
        {
            /* no work necessary here */
        }

    };

    static void __error_exit(j_common_ptr cinfo)
    {
        __debugbreak();
    }
}


template<typename CORE> bool bitmap_t<CORE>::load_from_JPG(const void * buf, int buflen)
{
    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = __error_exit;

    jpeg_create_decompress(&cinfo);

    jpgreadbuf tmp(buf, buflen);
    cinfo.src = &tmp;

    cinfo.out_color_space = JCS_RGB;

    if (JPEG_SUSPENDED == jpeg_read_header(&cinfo,true)) return false;
    jpeg_start_decompress(&cinfo);

    int row_stride = cinfo.output_width * cinfo.output_components;
    create_RGBA(ref_cast<ivec2>(cinfo.output_width,cinfo.output_height));

    JSAMPARRAY aTempBuffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    for (int y = 0; y < info().sz.y; y++)
    {
        jpeg_read_scanlines(&cinfo, aTempBuffer, 1);

        uint8* src = aTempBuffer[0];
        uint8* dest = body() + info().pitch*y;

        for (int x = info().sz.x; x > 0; x--, src += cinfo.output_components){
            *dest++ = src[2];
            *dest++ = src[1];
            *dest++ = src[0];
            *dest++ = 0xFF;
        }

    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return true;

}


template<typename CORE> bool bitmap_t<CORE>::save_as_JPG(buf_c &buf, int quality, bool colorspace_rgb) const
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    if (colorspace_rgb) jpeg_set_colorspace(&cinfo, JCS_RGB);
    cinfo.image_width = info().sz.x;
    cinfo.image_height = info().sz.y;
    cinfo.input_components = 3;
    cinfo.optimize_coding = 1;
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpgsavebuf mgr(buf);

    if (cinfo.dest == nullptr)
        cinfo.dest = &mgr;

    jpeg_start_compress(&cinfo, true);

    int row_stride = info().sz.x*3, bpp = info().bytepp();

    unsigned char* aTempBuffer = (unsigned char*)_alloca(row_stride);

    for (int y = 0; y < info().sz.y; y++){
        uint8* src = body() + info().pitch*y;
        uint8* dest = aTempBuffer;

        for (int x = info().sz.x; x > 0; x--, src += bpp){
            *dest++ = src[2];
            *dest++ = src[1];
            *dest++ = src[0];
        }

        jpeg_write_scanlines(&cinfo, &aTempBuffer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return true;
}

template<typename CORE> bool bitmap_t<CORE>::save_as_JPG(const wsptr &filename, int quality, bool colorspace_rgb) const
{
	FILE *fp;

	if ((fp = _wfopen(tmp_wstr_c(filename), L"wb")) == nullptr)
		return false;

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);

	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	if (colorspace_rgb) jpeg_set_colorspace(&cinfo, JCS_RGB);
	cinfo.image_width = info().sz.x;
	cinfo.image_height = info().sz.y;
	cinfo.input_components = 3;
    cinfo.optimize_coding = 1;
    jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_stdio_dest(&cinfo, fp);

	jpeg_start_compress(&cinfo, true);

	int row_stride = info().sz.x*3, bpp = info().bytepp();

	unsigned char* aTempBuffer = (unsigned char*)_alloca(row_stride);
	for (int y = 0; y < info().sz.y; y++){
		uint8* src = body() + info().pitch*y;
		uint8* dest = aTempBuffer;

		for (int x = info().sz.x; x > 0; x--, src += bpp){
			*dest++ = src[2];
			*dest++ = src[1];
			*dest++ = src[0];
		}

		jpeg_write_scanlines(&cinfo, &aTempBuffer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	fclose(fp);

	return true;
}


#pragma warning (push)
#pragma warning (disable : 4731)

template<typename CORE> void bitmap_t<CORE>::swap_byte(void *target) const
{
    if (info().bytepp() != 3 && info().bytepp() != 4)
    {
        DEBUG_BREAK(); //ERROR_S(L"Unsupported bitpp to convert");
    }


    const int szx = info().sz.x;
    const int szy = info().sz.y;
    const int bpp = info().bytepp();
    const uint8 *bo = body();

    _asm
    {
        // dest
        mov     edi, target

        // source
        //mov     esi, this
        mov     ecx, szx //[esi + m_Size]
        mov     ebx, bpp // [esi + info().bytepp()]
        mov     eax, szy // [esi + m_Size + 4]
        mul     ecx
        mov     esi, bo//[esi + m_Data]
        mov     ecx, eax

        cmp     ebx, 3
        jz      bpp3

        test    ecx, 3
        jz      skipmicroloop4

microloop4:

        mov     eax, [esi]
        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        add     edi, 4
        and     ebx, 0x00FF00FF
        add     esi, 4
        or      eax, ebx
        mov     [edi - 4], eax

        dec     ecx
        test    ecx, 3
        jnz     microloop4

skipmicroloop4:
        shr     ecx, 2
        jz      end

loop1:
        mov     eax, [esi]
        mov     edx, [esi + 4]

        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        add     edi, 16
        and     ebx, 0x00FF00FF
        or      eax, ebx
        mov     ebx, edx
        and     edx, 0xFF00FF00
        ror     ebx, 16
        add     esi, 16
        and     ebx, 0x00FF00FF
        or      edx, ebx

        mov     [edi - 16], eax
        mov     [edi - 12], edx

        mov     eax, [esi - 8]
        mov     edx, [esi - 4]

        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        and     ebx, 0x00FF00FF
        or      eax, ebx
        mov     ebx, edx
        and     edx, 0xFF00FF00
        ror     ebx, 16
        and     ebx, 0x00FF00FF
        or      edx, ebx

        mov     [edi - 8], eax
        mov     [edi - 4], edx

        dec     ecx
        jnz     loop1

        jmp     end
bpp3:

        push    ecx
        shr     ecx, 2
        jnz     rulezz
        pop     ecx
        jmp     microloop3
rulezz:
        push    ebp
loop2:
        // converts every 4 pixels (3 uint32's)

        mov     eax, [esi]
        mov     edx, [esi + 4]
        mov     ebx, eax
        and     eax, 0x0000FF00     // XXXXG0XX
        ror     ebx, 16
        mov     ebp, ebx
        and     ebx, 0x00FF00FF     // XXR0XXG0
        or      eax, ebx
        mov     ebx, edx
        and     ebx, 0x0000FF00     // XXXXR1XX
        and     ebp, 0x0000FF00     // XXXXR0XX
        shl     ebx, 16
        or      eax, ebx
        mov     ebx, edx            // store B2
        mov     [edi], eax
        and     edx, 0xFF0000FF     // G2XXXXG1
        shr     ebx, 16
        or      edx, ebp
        mov     eax, [esi + 8]
        and     ebx, 0x000000FF
        mov     ebp, eax
        and     eax, 0x00FF0000
        ror     ebp, 16
        or      eax, ebx
        mov     ebx, ebp
        and     ebp, 0x00FF0000     // XXR2XXXX
        or      edx, ebp
        mov     [edi + 4], edx
        and     ebx, 0xFF00FF00
        or      eax, ebx
        mov     [edi + 8], eax

        add     esi, 12
        add     edi, 12

        dec     ecx
        jnz     loop2


        pop     ebp
        pop     ecx
        and     ecx, 3
        jz      end

microloop3:
        mov     eax, [esi]
        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        add     edi, 3
        and     ebx, 0x00FF00FF
        add     esi, 3
        or      eax, ebx
        mov     [edi - 3], eax

        dec     ecx
        jnz     microloop3

end:

    };

}

/*

template<typename CORE> void bitmap_t<CORE>::save_as_DDS(const char * filename, int compression, int additional_squish_flags) const
{
    buf_c buf;
    save_as_DDS(buf, compression, additional_squish_flags);
    buf.save_to_file( filename );
}

template<typename CORE> void bitmap_t<CORE>::save_as_DDS(buf_c & buf, int compression, int additional_squish_flags) const
{
    if (info().bytepp() != 3 && info().bytepp() != 4)
    {
        DEBUG_BREAK(); //ERROR(L"Unsupported bitpp to convert");
    }

    int sz = info().sz.x * info().sz.y * info().bytepp();
	int squishc = squish::kDxt5;
	if (compression)
	{
		switch (compression)
		{
		default:
			sux("wrong DDS compression bitpp, using DXT1");
		case D3DFMT_DXT1:
			squishc = squish::kDxt1;
			break;
		case D3DFMT_DXT3:
			squishc = squish::kDxt3;
			break;
		case D3DFMT_DXT5:
			squishc = squish::kDxt5;
			break;
		}
		sz = squish::GetStorageRequirements(info().sz.x, info().sz.y, squishc);
	}

    buf.clear();
    uint32 *dds = (uint32 *)buf.expand(sz + sizeof(DDSURFACEDESC2) + sizeof(uint32));

    *dds = 0x20534444; // "DDS "

    DDSURFACEDESC2 * ddsp = (DDSURFACEDESC2 *)(dds + 1);
    memset(ddsp, 0, sizeof(DDSURFACEDESC2));
    
    ddsp->dwSize = sizeof(DDSURFACEDESC2);
    ddsp->dwWidth = info().sz.x;
    ddsp->dwHeight = info().sz.y;
    ddsp->dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_LINEARSIZE;

    ddsp->dwLinearSize = sz;
    ddsp->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsp->ddpfPixelFormat.dwFlags = (compression == 0 ? DDPF_RGB : DDPF_FOURCC) | ((info().bytepp() == 4)?DDPF_ALPHAPIXELS:0);
    ddsp->ddpfPixelFormat.dwRGBBitCount = info().bitpp;
    ddsp->ddpfPixelFormat.dwRBitMask = 0x00FF0000;
    ddsp->ddpfPixelFormat.dwGBitMask = 0x0000FF00;
    ddsp->ddpfPixelFormat.dwBBitMask = 0x000000FF;
    ddsp->ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
	ddsp->ddpfPixelFormat.dwFourCC = compression;

    ddsp->ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    if (info().sz.x < 4)
    {
		sux("!-!");
        int cnt = info().sz.x * info().sz.y;
        BYTE *des = (BYTE *)(ddsp + 1);
        const uint8 *sou = body();
        if (info().bytepp() == 3)
        {
            while (cnt-- > 0)
            {
                //*des = *(sou + 2);
                //*(des+1) = *(sou + 1);
                //*(des+2) = *(sou + 0);
                *des = *(sou + 0);
                *(des+1) = *(sou + 1);
                *(des+2) = *(sou + 2);
                *(des+3) = 0xFF;

                sou += 3;
                des += 4;
            }
        } else
        {
            while (cnt-- > 0)
            {
                //*des = *(sou + 2);
                //*(des+1) = *(sou + 1);
                //*(des+2) = *(sou + 0);
                //*(des+3) = *(sou + 3);
                *des = *(sou + 0);
                *(des+1) = *(sou + 1);
                *(des+2) = *(sou + 2);
                *(des+3) = *(sou + 3);

                sou += 4;
                des += 4;
            }

        }

    } else
    {
		if (compression == 0)
			memcpy( ddsp + 1, body(), info().sz.x * info().sz.y * info().bytepp() );
		else
		{
			squish::u8 *buf = (squish::u8*)MM_ALLOC(slmemcat::squish_buff, info().sz.x * info().sz.y * info().bytepp());
			swap_byte(buf);
			squish::CompressImage(buf, info().sz.x, info().sz.y, ddsp + 1, squishc | additional_squish_flags);
			MM_FREE(slmemcat::squish_buff, buf);
		}
        //swap_byte( ddsp + 1 );

    }
    //memcpy(des,sou,sz);
}
#pragma warning (pop)
*/

#pragma pack(push)
#pragma pack(1)
struct TGA_Header
{
	uint8 IDLength;
	uint8 ColorMapType;
	uint8 ImageType;
	uint16 ColorMapStart;
	uint16 ColorMapLength;
	uint8 ColorMapDepth;
	uint16 XOffset;
	uint16 YOffset;
	uint16 Width;
	uint16 Height;
	uint8 Depth;
	uint8 ImageDescriptor;
};
#pragma pack(pop)

void bgr_to_rgb(int n_pixels, uint8 *data, int components)
{
	for (;n_pixels > 0; data+=components, n_pixels--) SWAP(data[0], data[2]);
}

template<typename CORE> bool bitmap_t<CORE>::load_from_TGA(const void * buf, int buflen)
{
	clear();

	// Ќеобходимые константы дл€ загрузки TGA
#define TARGA_TRUECOLOR_IMAGE		2
#define TARGA_BW_IMAGE				3
#define TARGA_TRUECOLOR_RLE_IMAGE	10
#define TARGA_BW_RLE_IMAGE			11

	TGA_Header * header = (TGA_Header*)buf;
	if ((size_t)buflen < sizeof(TGA_Header) || (header->ImageType != TARGA_TRUECOLOR_IMAGE && header->ImageType != TARGA_BW_IMAGE)) return false;
	uint8 *pixels = (uint8*)(header + 1) + header->IDLength, *tdata;

	ivec2 sz( header->Width, header->Height );

	switch (header->Depth)
	{
	case 8:  create_grayscale(sz); break;
	case 24: create_RGB(sz);  break;
	case 32: create_RGBA(sz); break;
	default: return false;
	}

	int inc;
	if (header->ImageDescriptor & 0x20)//top-down orientation
	{
		tdata = body();
		inc = info().pitch;
	}
	else//bottom-up orientation
	{
		tdata = body()+(sz.y-1)*info().pitch;
		inc = -(int)info().pitch;
	}

	for (int y=sz.y; y>0; tdata+=inc, pixels+=info().pitch, y--)
		memcpy(tdata, pixels, info().pitch);

	//if (header->Depth > 8) bgr_to_rgb(lenx*leny, body(), info().bytepp());//RGB -> BGR

	return true;
}

template<typename CORE> bool bitmap_t<CORE>::load_from_BMP(const void * buf, int buflen)
{
	clear();

	struct Header
	{
		BITMAPFILEHEADER fH;
		BITMAPINFOHEADER iH;
	} *header = (Header*)buf;

	if (header->fH.bfType != MAKEWORD('B', 'M')) return false;
	if (header->iH.biCompression != 0) return false;

	ivec2 sz( header->iH.biWidth, header->iH.biHeight );

	switch (header->iH.biBitCount)
	{
	case 24: create_RGB (sz); break;
	case 32: create_RGBA(sz); break;
	default: return false;
	}

	uint8 *pixels = (uint8*)buf + header->fH.bfOffBits, *tdata = body()+(sz.y-1)*info().pitch;
	int bmpPitch = (info().pitch + 3) & ~3;

	for (int y=sz.y; y>0; tdata-=info().pitch, pixels+=bmpPitch, y--)
		memcpy(tdata, pixels, info().pitch);

	return true;
}

template<typename CORE> bool bitmap_t<CORE>::load_from_DDS(void * buf_to, int buf_to_len, const void * buf, int buflen)
{
	ivec2 sz;
    uint8 bitpp;

	size_t id = FileDDS_ReadStart_Buf(buf,buflen,sz,bitpp);
	if(id==0) return false;

    ASSERT( bitpp == 32 );
    ASSERT( buf_to_len >= int(sz.x * sz.y * 4) );

    return FileDDS_Read(id,buf_to,sz.x*4) != 0;
}


template<typename CORE> bool bitmap_t<CORE>::load_from_DDS(const void * buf, int buflen)
{
	clear();

	ivec2 sz;
    uint8 bitpp;

	size_t id = FileDDS_ReadStart_Buf(buf,buflen,sz,bitpp);
	if(id==0) return false;

	if(bitpp==8)
    {
		create_grayscale(sz);
	} else if(bitpp==24) {
		create_RGB(sz);
	} else if(bitpp==32) {
		create_RGBA(sz);
	} else
        DEBUG_BREAK(); // unsupported

    if(!FileDDS_Read(id,body(),info().pitch))
    {
		clear();
		return false;
	}
	return true;

}

template<typename CORE> bool bitmap_t<CORE>::load_from_DDS(const buf_c & buf)
{
    if (buf.size() == 0) return false;
    return load_from_DDS( buf.data(), buf.size() );
}
template<typename CORE> bool bitmap_t<CORE>::load_from_DDS(const wsptr &filename)
{
    if (blob_c b = g_fileop->load( filename ))
        return load_from_DDS( b.data(), b.size() );
    return false;
}

template<typename CORE> bool bitmap_t<CORE>::load_from_PNG(const void * buf, int buflen)
{
	clear();

	ivec2 sz;
    uint8 bitpp;

    __try
    {
	    size_t id=FilePNG_ReadStart_Buf(buf,buflen,sz,bitpp);

       
        if(id==0) return false;

	    if(bitpp==8) {
		    create_grayscale(sz);
	    } else if(bitpp==24) {
		    create_RGB(sz);
	    } else if(bitpp==32) {
		    create_RGBA(sz);
	    } else
            DEBUG_BREAK();; // unsupported

        if(!FilePNG_Read(id,body(),info().pitch))
        {
            clear();
            return false;
        }
    }
    __except ( EXCEPTION_EXECUTE_HANDLER )
    {
        clear();
        return false;
    }
    //if (info().bytepp() == 3 || info().bytepp() == 4)
    //{
    //    swap_byte( body() );
    //}
	return true;
}

template<typename CORE> bool bitmap_t<CORE>::load_from_PNG(const buf_c & buf)
{
    if (buf.size() == 0) return false;
    return load_from_PNG( buf.data(), buf.size() );
}
template<typename CORE> bool bitmap_t<CORE>::load_from_PNG(const wsptr &filename)
{
    if (blob_c b = g_fileop->load(filename))
        return load_from_PNG( b.data(), b.size() );
    return false;
}

template<typename CORE> bool bitmap_t<CORE>::load_from_file(const void * buf, int buflen)
{
	if (buflen == 0) return false;
	return load_from_DDS(buf, buflen) || load_from_PNG(buf, buflen) || load_from_BMP(buf, buflen) || load_from_TGA(buf, buflen);
}
template<typename CORE> bool bitmap_t<CORE>::load_from_file(const buf_c & buf)
{
	return load_from_file( buf.data(), buf.size() );
}
template<typename CORE> bool bitmap_t<CORE>::load_from_file(const wsptr &filename)
{
	if (blob_c b = g_fileop->load(filename))
		return load_from_file( b.data(), b.size() );
	return false;
}


template<typename CORE> size_t bitmap_t<CORE>::save_as_PNG(const void * body, void * buf, int buflen) const
{
	if(info().sz.x<=0 || info().sz.y<=0) return 0;
	return FilePNG_Write(buf,buflen,body,info().pitch,info().sz,info().bytepp(),0);
}

template<typename CORE> bool bitmap_t<CORE>::save_as_PNG(const wsptr &filename) const
{
    buf_c buf;
    save_as_PNG(buf);
    buf.save_to_file( filename );
    return true;
}


template<typename CORE> bool bitmap_t<CORE>::save_as_PNG(buf_c & buf) const
{
    void *data = body();
    //buf_c bod;

    //if (info().bytepp() == 3 || info().bytepp() == 4)
    //{
    //    uint sz = info().pitch * info().sz.y;
    //    bod.expand( sz );
    //    swap_byte( bod.data() );

    //    data = bod.data();
    //}

    size_t len = info().pitch*info().sz.y+100;
	buf.set_size(sz_cast<uint>(len), false);
	len=save_as_PNG(data, buf.data(),buf.capacity());
	if(len>buf.capacity()) {
		buf.set_size(sz_cast<uint>(len), false);
		len=save_as_PNG(data, buf.data(),sz_cast<uint>(len));
    } else buf.set_size(sz_cast<uint>(len));
	return len>0;
}


template<typename CORE> void bitmap_t<CORE>::load_from_HWND(HWND hwnd)
{
    RECT r;
    GetClientRect( hwnd, &r );
    ivec2 sz(r.right-r.left, r.bottom-r.top);

    int lPitch=((4*sz.x-1)/4+1)*4;

    BITMAPINFO bi;
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sz.x;
    bi.bmiHeader.biHeight = -sz.y;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = 0;
    bi.bmiHeader.biSizeImage = lPitch * sz.y;
    bi.bmiHeader.biXPelsPerMeter = 0;
    bi.bmiHeader.biYPelsPerMeter = 0;
    bi.bmiHeader.biClrImportant = 0;
    bi.bmiHeader.biClrUsed = 0;

    HDC dc = GetDC( hwnd );
    
    uint8 *bits;
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP memBM = CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void **)&bits,0,0);
    ReleaseDC(hwnd, dc);

    if (memBM == nullptr)
    {
        return;
    }

    HBITMAP tempBM = (HBITMAP)SelectObject(memDC,memBM);

    BitBlt(memDC,0,0,info().sz.x,info().sz.y,dc,0,0,SRCCOPY);

    if (info().sz != sz || info().bytepp() != 4) create_RGBA( sz );

    img_helper_copy( body(), bits, info(), imgdesc_s( sz, 32, lPitch ) );

    SelectObject(memDC,tempBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

void    drawable_bitmap_c::create(const ivec2 &sz)
{
    clear();

    core.m_info.sz = sz;
    core.m_info.bitpp = 32;

    DEVMODEW devmode;
    ZeroMemory(&devmode, sizeof(DEVMODE));
    devmode.dmSize = sizeof(DEVMODE);

    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &devmode);

    devmode.dmBitsPerPel = 32;
    devmode.dmPelsWidth = sz.x;
    devmode.dmPelsHeight = sz.y;

    HDC tdc = CreateDCW(L"DISPLAY", nullptr, nullptr, &devmode);

    m_memDC = CreateCompatibleDC(tdc);
    if (m_memDC == 0)
        DEBUG_BREAK();;

    //m_memBitmap = CreateCompatibleBitmap(tdc, w, h);

    BITMAPV4HEADER bmi;

    int ll = sz.x * 4;
    core.m_info.pitch = (ll + 3) & (~3);

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bV4Size = sizeof(bmi);
    bmi.bV4Width = sz.x;
    bmi.bV4Height = -sz.y;
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = 32;
    bmi.bV4V4Compression = BI_RGB;
    bmi.bV4SizeImage = 0;

    {
#ifndef _FINAL
        disable_fp_exceptions_c __x;
#endif
        m_memBitmap = CreateDIBSection(tdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&core.m_body, 0, 0);

    }
    ASSERT(m_memBitmap);
    CHECK(SelectObject(m_memDC, m_memBitmap));
    DeleteDC(tdc);
}

bool    drawable_bitmap_c::is_alphablend(const irect &r) const
{
    if (!m_memDC || !m_memBitmap) return false;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_memDC, m_memBitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return false;

    if (b.bmi.bmiHeader.bV4BitCount == 32 && core.m_body)
    {
        uint32 ll = uint32(b.bmi.bmiHeader.bV4Width * 4);
        const uint8 * src = core() + (info().pitch*r.lt.y); //(BYTE *)(m_pDIBits)+info().pitch*uint32(b.bmi.bmiHeader.bV4Height - 1 - r.lt.y);
        src += r.lt.x * 4;

        int hh = tmin(r.height(), b.bmi.bmiHeader.bV4Height - r.lt.y);
        ll = 4 * tmin(r.width(), b.bmi.bmiHeader.bV4Width - r.lt.x);
        for (int y = 0; y < hh; y++)
        {
            for (uint i = 0; i < ll; i += 4)
            {
                uint32 s = *(uint32 *)(src + i);
                if ((s & 0xFF000000) != 0xFF000000) return true;
            }
            src += info().pitch;
        }
    }

    return false;
}

bool    drawable_bitmap_c::create_from_bitmap(const bitmap_c &bmp, bool flipy, bool premultiply, bool detect_alpha_pixels)
{
    return create_from_bitmap(bmp, ivec2(0, 0), bmp.info().sz, flipy, premultiply, detect_alpha_pixels);
}

bool drawable_bitmap_c::create_from_bitmap(const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy, bool premultiply, bool detect_alpha_pixels)
{
    clear();

    BITMAPV4HEADER bmi;

    ASSERT((p.x + sz.x) <= bmp.info().sz.x && (p.y + sz.y) <= bmp.info().sz.y);

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bV4Size = sizeof(bmi);
    bmi.bV4Width = sz.x;
    bmi.bV4Height = flipy ? sz.y : -sz.y;
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = bmp.info().bitpp;
    if (bmp.info().bytepp() >= 3)
    {
        bmi.bV4V4Compression = BI_RGB;
        bmi.bV4SizeImage = 0;
        bmi.bV4AlphaMask = 0xFF000000;
    }
    else
    {
        bmi.bV4V4Compression = BI_BITFIELDS;
        bmi.bV4RedMask = 0x0000f800;
        bmi.bV4GreenMask = 0x000007e0;
        bmi.bV4BlueMask = 0x0000001f;
        bmi.bV4AlphaMask = 0;
        bmi.bV4SizeImage = sz.x * sz.y * bmp.info().bytepp();
    }

    //HDC tdc=GetDC(0);

    DEVMODEW devmode;
    ZeroMemory(&devmode, sizeof(DEVMODE));
    devmode.dmSize = sizeof(DEVMODE);

    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &devmode);

    devmode.dmBitsPerPel = bmp.info().bitpp;
    devmode.dmPelsWidth = sz.x;
    devmode.dmPelsHeight = sz.y;

    HDC tdc = CreateDCW(L"DISPLAY", nullptr, nullptr, &devmode);

    {
#ifndef _FINAL
        disable_fp_exceptions_c __x;
#endif

        m_memBitmap = CreateDIBSection(tdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&core.m_body, 0, 0);
    }

    bool alphablend = false;

    if (m_memBitmap == nullptr) DEBUG_BREAK();;

    //if((bmp->info().bytepp()==3 || bmp->info().bytepp()==4)) bmp->swap_byte(ivec2(0,0),bmp->size(),0,2);

    core.m_info.sz = sz;
    core.m_info.bitpp = bmp.info().bitpp;

    int ll = sz.x * bmp.info().bytepp();
    core.m_info.pitch = (ll + 3) & (~3);
    uint8 * bdes = core(); //+lls*uint32(sz.y-1);
    const uint8 * bsou = bmp.body() + p.x * bmp.info().bytepp() + bmp.info().pitch * p.y;

    if (premultiply)
    {
        for (int y = 0; y < sz.y; ++y)
        {
            for (int x = 0; x < sz.x; ++x)
            {
                uint32 ocolor;
                uint32 color = ((uint32 *)bsou)[x];
                uint8 alpha = uint8(color >> 24);
                if (alpha == 255)
                {
                    ocolor = color;
                }
                else if (alpha == 0)
                {
                    ocolor = 0;
                    alphablend = true;
                }
                else
                {

                    uint8 R = as_byte(color >> 16);
                    uint8 G = as_byte(color >> 8);
                    uint8 B = as_byte(color);

                    float A = float(alpha) / 255.0f;

#define CCGOOD( C ) CLAMP<uint8>( (uint)lround(float(C) * A) )

                    ocolor = CCGOOD(B) | (CCGOOD(G) << 8) | (CCGOOD(R) << 16) | (alpha << 24);
#undef CCGOOD

                    alphablend = true;
                }

                ((uint32 *)bdes)[x] = ocolor;
            }

            bsou = bsou + bmp.info().pitch;
            bdes = bdes + info().pitch;

        }

    }
    else if (detect_alpha_pixels)
    {
        for (int y = 0; y < sz.y; ++y)
        {
            if (alphablend)
            {
                if (info().pitch == bmp.info().pitch && ll == info().pitch)
                {
                    memcpy(bdes, bsou, ll * (sz.y - y));
                }
                else
                {
                    for (; y < sz.y; ++y)
                    {
                        memcpy(bdes, bsou, ll);
                        bsou = bsou + bmp.info().pitch;
                        bdes = bdes + info().pitch;
                    }
                }

                break;
            }

            for (int i = 0; i < ll; i += 4)
            {
                uint32 color = *(uint32 *)(bsou + i);
                *(uint32 *)(bdes + i) = color;
                uint8 alpha = uint8(color >> 24);
                if (alpha < 255)
                    alphablend = true;
            }

            bsou = bsou + bmp.info().pitch;
            bdes = bdes + info().pitch;
        }

    }
    else
    {
        if (info().pitch == bmp.info().pitch && ll == info().pitch)
        {
            memcpy(bdes, bsou, ll * sz.y);
        }
        else
        {
            for (int y = 0; y < sz.y; ++y)
            {
                memcpy(bdes, bsou, ll);
                bsou = bsou + bmp.info().pitch;
                bdes = bdes + info().pitch;
            }
        }
    }

    //if((bmp->info().bytepp()==3 || bmp->info().bytepp()==4)) bmp->swap_byte(ivec2(0,0),bmp->size(),0,2);

    m_memDC = CreateCompatibleDC(tdc);
    if (m_memDC == nullptr) DEBUG_BREAK();;
    if (SelectObject(m_memDC, m_memBitmap) == nullptr) DEBUG_BREAK();;

    //ReleaseDC(0,tdc);
    DeleteDC(tdc);

    return alphablend;
}

void    drawable_bitmap_c::save_to_bitmap(bitmap_c &bmp, const ivec2 & pos_from_dc)
{
    if (!m_memDC || !m_memBitmap) return;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_memDC, m_memBitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;

    if (b.bmi.bmiHeader.bV4BitCount == 32 && core())
    {
        uint32 ll = uint32(b.bmi.bmiHeader.bV4Width * 4);
        const uint8 * src = core()+info().pitch*uint32(b.bmi.bmiHeader.bV4Height - 1 - pos_from_dc.y);
        src += pos_from_dc.x * 4;
        //bmp->create_RGBA(b.bmi.bmiHeader.bV4Width,b.bmi.bmiHeader.bV4Height);
        uint8 * dst = bmp.body();
        int hh = tmin(bmp.info().sz.y, b.bmi.bmiHeader.bV4Height - pos_from_dc.y);
        ll = 4 * tmin(bmp.info().sz.x, b.bmi.bmiHeader.bV4Width - pos_from_dc.x);
        for (int y = 0; y < hh; y++)
        {
            //memcpy(dst,src,ll);

            for (uint i = 0; i < ll; i += 4)
            {
                uint32 s = *(uint32 *)(src + i);
                s |= 0xFF000000;
                *(uint32 *)(dst + i) = s;
            }


            src -= info().pitch;
            dst += bmp.info().pitch;
        }
    }

}

void drawable_bitmap_c::save_to_bitmap(bitmap_c &bmp, bool save16as32)
{
    if (!m_memDC || !m_memBitmap) return;


    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_memDC, m_memBitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;

    if (b.bmi.bmiHeader.bV4BitCount == 32 && core())
    {
        uint32 ll = uint32(b.bmi.bmiHeader.bV4Width * 4);
        const uint8 * src = core()+info().pitch*uint32(b.bmi.bmiHeader.bV4Height - 1);
        bmp.create_RGBA(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
        uint8 * dst = bmp.body();
        for (int y = 0; y < bmp.info().sz.y; y++)
        {
            memcpy(dst, src, ll);
            src -= info().pitch;
            dst += bmp.info().pitch;
        }
        return;
    }



    if (b.bmi.bmiHeader.bV4BitCount != 16 && b.bmi.bmiHeader.bV4BitCount != 24 && b.bmi.bmiHeader.bV4BitCount != 32) return;

    if (save16as32 && b.bmi.bmiHeader.bV4BitCount == 16)
    {
        b.bmi.bmiHeader.bV4BitCount = 32;
    }

    if (b.bmi.bmiHeader.bV4BitCount == 16) bmp.create_16(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
    else if (b.bmi.bmiHeader.bV4BitCount == 24) bmp.create_RGB(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
    else if (b.bmi.bmiHeader.bV4BitCount == 32) bmp.create_RGBA(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
    bmp.fill(0);

    if (GetDIBits(m_memDC, m_memBitmap, 0, b.bmi.bmiHeader.bV4Height, bmp.body(), (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;

    bmp.fill_alpha(255);

    //bmp->flip_y();
    //if(b.bmi.bmiHeader.bV4BitCount==24 || b.bmi.bmiHeader.bV4BitCount==32) bmp->swap_byte(ivec2(0,0),bmp->size(),0,2);

}

void drawable_bitmap_c::draw(HDC dc, aint xx, aint yy, int alpha) const
{
    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_memDC, m_memBitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;
    if (alpha > 0)
    {
        BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        AlphaBlend(dc, xx, yy, b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height, m_memDC, 0, 0, b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height, blendPixelFunction);
    } else if (alpha < 0)
    {
        BitBlt(dc, xx, yy, b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height, m_memDC, 0, 0, SRCCOPY);
    }
}

void drawable_bitmap_c::draw(HDC dc, int xx, int yy, const irect &r, int alpha) const
{
    if (alpha > 0)
    {
        BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        AlphaBlend(dc, xx, yy, r.width(), r.height(), m_memDC, r.lt.x, r.lt.y, r.width(), r.height(), blendPixelFunction);
    } else if (alpha < 0)
    {
        BitBlt(dc, xx, yy, r.width(), r.height(), m_memDC, r.lt.x, r.lt.y, SRCCOPY);
    }
}


template class bitmap_t<bmpcore_normal_s>;
template class bitmap_t<bmpcore_exbody_s>;




} // namespace ts

