#include "toolset.h"
#include "zlib/src/contrib/minizip/unzip.h"

namespace ts
{
struct zlo : public zlib_filefunc_def,  public arc_file_s
{
    char filename_inzip[512];
    const uint8 *data;
    aint datasize, ptr = 0;
    unz_global_info inf;
    unzFile z;

    unz_file_info file_info;
    unz64_file_pos_s ffs;


    static voidpf ZCALLBACK fopen_file_func( voidpf, const char *, int )
    {
        return (voidpf)1; //-V566
    }

    static uLong ZCALLBACK fread_file_func(voidpf opaque, voidpf stream, void* buf, uLong size)
    {
        zlo *me = (zlo *)opaque;
        aint ost = me->datasize - me->ptr;
        aint minsz = tmin(ost, (aint)size);
        memcpy( buf, me->data + me->ptr, minsz);
        me->ptr += minsz;
        return (uLong)minsz;
    }
    static long ZCALLBACK ftell_file_func(voidpf opaque, voidpf stream)
    {
        zlo *me = (zlo *)opaque;
        return (long)me->ptr;
    }
    static long ZCALLBACK fseek_file_func(voidpf opaque, voidpf stream, uLong offset, int origin)
    {
        zlo *me = (zlo *)opaque;
        switch (origin)
        {
            case ZLIB_FILEFUNC_SEEK_END:
                me->ptr = me->datasize + offset;
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                me->ptr += offset;
                break;
            default:
                me->ptr = offset;
                break;
        }

        return 0;
    }
    static int ZCALLBACK fclose_file_func(voidpf opaque, voidpf stream)
    {
        return EOF;
    }
    static int ZCALLBACK ferror_file_func(voidpf opaque, voidpf stream)
    {
        return 0;
    }


    zlo( const void *data, aint datasize ):data((const uint8 *)data), datasize(datasize)
    {
        zopen_file = fopen_file_func;
        zread_file = fread_file_func;
        zwrite_file = nullptr; /*fwrite_file_func*/;
        ztell_file = ftell_file_func;
        zseek_file = fseek_file_func;
        zclose_file = fclose_file_func;
        zerror_file = ferror_file_func;
        opaque = this;
        z = unzOpen2( nullptr, this );
        if (z)
        {
            int err = unzGetGlobalInfo(z, &inf);
            if (err != UNZ_OK)
            {
                unzClose(z);
                z = nullptr;
                return;
            }
        }

        fn.s = filename_inzip;

    }
    ~zlo()
    {
        if (z) unzClose(z);
    }
    operator bool() const { return z != nullptr; }

    virtual blob_c get() const override
    {
        unzGoToFilePos64(z, &ffs);
        if (unzOpenCurrentFile(z) == UNZ_OK)
        {
            blob_c b(file_info.uncompressed_size, true);
            uLong n = unzReadCurrentFile(z, b.data(), file_info.uncompressed_size);
            if (unzCloseCurrentFile(z) == UNZ_OK && n == file_info.uncompressed_size)
            {
                return b;
            }
        }
        return blob_c();
    }

    bool iterate( ARC_FILE_HANDLER h )
    {
        for (int i = 0; i < (int)inf.number_entry; ++i)
        {
            int err = unzGetCurrentFileInfo(z, &file_info, filename_inzip, sizeof(filename_inzip) - 1, nullptr, 0, nullptr, 0);
            if (err != UNZ_OK)
                return false;
            fn.l = file_info.size_filename;
            if (file_info.compression_method != 0 && file_info.compression_method != Z_DEFLATED) goto next;
            if (file_info.size_filename == 0 || __is_slash(filename_inzip[fn.l - 1])) goto next;
            unzGetFilePos64(z, &ffs);
            if (!h(*this))
                return true;
        next:
            if ((i + 1) < (int)inf.number_entry)
            {
                err = unzGoToNextFile(z);
                if (err != UNZ_OK)
                    return false;
            }
        }
    
        return true;
    }

};


bool zip_open( const void *data, aint datasize, ARC_FILE_HANDLER h )
{
    zlo z(data,datasize);
    if (!z) return false;
    return z.iterate(h);
}

}

