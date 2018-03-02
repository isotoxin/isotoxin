#include "toolset.h"

namespace ts
{

    struct zippp : zlib_filefunc_def
    {
        static THREADLOCAL zippp * first;
        zippp *next = nullptr;
        zip_container_c *zc;
        unzFile unz = nullptr;
        void *stream = nullptr;
        uint8 rbuf[65536];
        uint64    rbufshift = (uint64)-1;
        uint64    rcurpos = 0; // current virtual pos
        uint64    rfcurpos = 0; // current pos in real file
        uint64    fsz = 0;
        aint rbufsize = 0;

        zippp(zip_container_c *owner, bool prefetch) :zc(owner)
        {
            zopen_file = fopen_file_func;
            zread_file = fread_file_func;
            zwrite_file = nullptr; /*fwrite_file_func*/;
            ztell_file = ftell_file_func;
            zseek_file = fseek_file_func;
            zclose_file = fclose_file_func;
            zerror_file = ferror_file_func;
            opaque = this;

            stream = f_open(zc->fn());
            if (stream)
            {
                // load last 64k of file
                fsz = f_size(stream);
                if (prefetch)
                {
                    // preload last 64k of zip file - need for toc load
                    rbufsize = (aint)tmin(fsz, 65536);
                    rbufshift = fsz - rbufsize;
                    f_set_pos(stream, rbufshift);
                    f_read(stream, rbuf, rbufsize);
                    rfcurpos = fsz;
                }
                ASSERT(rfcurpos == f_get_pos(stream));
            }
            rcurpos = 0;
        }

        ~zippp()
        {
            //ASSERT(unz == nullptr && stream == nullptr);
        }

        static zippp * get(zip_container_c *c, bool prefetch)
        {
            for (zippp *a = zippp::first; a; a = a->next)
            {
                if (a->zc == c)
                    return a;
            }
            zippp *n = TSNEW(zippp, c, prefetch);
            if (n->stream == nullptr)
            {
                TSDEL(n);
                return nullptr;
            }

            n->unz = unzOpen2(nullptr, n);
            if (n->unz == nullptr)
            {
                TSDEL(n);
                return nullptr;
            }

            n->next = zippp::first;
            zippp::first = n;

            return n;
        }

        bool closezip()
        {
            bool o = false;
            if (unz)
            {
                unzClose(unz);
                unz = nullptr;
                o = true;
            }

            zc->finish_thread();
            return o;
        }


        static bool close(zip_container_c *c)
        {
            for (zippp *a = zippp::first; a; a = a->next)
            {
                if (a->zc == c)
                    return a->closezip();
            }

            return false;
        }

        static voidpf ZCALLBACK fopen_file_func(voidpf opaque, const char * filename, int mode)
        {
            zippp *z = (zippp *)opaque;

            //zip_container_c *zc = (zip_container_c *)(((uint8 *)z) - offsetof(zip_container_c, data));
            ASSERT(z->stream != nullptr);

            return z->stream;
        }

        static uLong ZCALLBACK fread_file_func(voidpf opaque, voidpf stream, void* buf, uLong size)
        {
            zippp *z = (zippp *)opaque;

            size_t inb = 0;
            if (z->rbufsize && z->rcurpos >= z->rbufshift)
            {
                size_t s = (size_t)(z->rcurpos - z->rbufshift);
                size_t e = s + size;

                if (s < (uint64)z->rbufsize)
                {
                    if (e <= (uint64)z->rbufsize)
                    {
                        z->rcurpos += size;
                        memcpy(buf, z->rbuf + s, size);
                        return size;
                    }

                    inb = (size_t)z->rbufsize - s;
                    size_t ost = e - (size_t)z->rbufsize;
                    ASSERT(inb + ost == size);

                    memcpy(buf, z->rbuf + s, inb);
                    z->rcurpos += inb;
                    size = (uLong)ost;
                    buf = (char *)buf + inb;
                }
            }

            if (z->rcurpos != z->rfcurpos)
            {
                f_set_pos(stream, z->rcurpos);
                z->rfcurpos = z->rcurpos;
            }

            if (size >= 32768)
            {
                uLong r = (uLong)f_read(stream, buf, size);
                z->rfcurpos += size;
                z->rcurpos = z->rfcurpos;
                ASSERT(z->rfcurpos == f_get_pos(stream));
                return r;
            }

            z->rbufsize = 65536;
            uLong rsz = (uLong)f_read(stream, z->rbuf, 65536);
            uLong nsz = tmin(rsz, size);
            if (rsz < 65536)
                z->rbufsize = rsz;
            z->rbufshift = z->rfcurpos;
            z->rfcurpos += rsz;
            z->rcurpos += nsz;
            memcpy(buf, z->rbuf, nsz);
            ASSERT(z->rfcurpos == f_get_pos(stream));
            return (uLong)(nsz + inb);
        }
        static long ZCALLBACK ftell_file_func(voidpf opaque, voidpf stream)
        {
            zippp *z = (zippp *)opaque;
            return (long)z->rcurpos;
        }
        static long ZCALLBACK fseek_file_func(voidpf opaque, voidpf stream, uLong offset, int origin)
        {
            zippp *z = (zippp *)opaque;

            uint64 o = offset;
            switch (origin)
            {
            case ZLIB_FILEFUNC_SEEK_END:
                o += z->fsz;
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                o += z->rcurpos;
                break;
            }

            z->rcurpos = o;
            return 0;
        }
        static int ZCALLBACK fclose_file_func(voidpf opaque, voidpf stream)
        {
            f_close(stream);
            return 0;
        }
        static int ZCALLBACK ferror_file_func(voidpf opaque, voidpf stream)
        {
            return 0;
        }


    };

    zippp * zippp::first = nullptr;

    zip_container_c::~zip_container_c()
    {
        close();
    }

    void zip_container_c::finish_thread()
    {
        zippp *prv = nullptr;
        for(zippp *a = zippp::first; a; a=a->next)
        {
            if (a->zc == this)
            {
                if (prv)
                {
                    prv->next = a->next;
                }
                else
                    zippp::first = a->next;
                TSDEL( a );
                break;
            }
            prv = a;
        }
    }

    bool    zip_container_c::zip_entry_s::read(unzFile unz, buf_wrapper_s &b)
    {
        unzGoToFilePos64(unz, &ffs);

        if (unzOpenCurrentFile(unz) == UNZ_OK)
        {
            auint n = unzReadCurrentFile(unz, b.alloc(full_unp_size), (int)full_unp_size);
            if (unzCloseCurrentFile(unz) == UNZ_OK && n == full_unp_size)
                return true;
        }
        return false;
    }

    struct crallocator : public dynamic_allocator_s
    {
        block_buf_c< 512 * 1024 > texts;
        virtual void *balloc(size_t sz)
        {
            return texts.alloc((uint)sz);
        }
        virtual void bfree(void *p)
        {
        }
    };


    bool    zip_container_c::open()
    {
        zippp *z = zippp::get(this, true);
        if (z == nullptr)
            return false;


        unz_global_info ginf;
        int err = unzGetGlobalInfo (z->unz,&ginf);
        if (err!=UNZ_OK)
        {
            z->closezip();
            return false;
        }

        ts::shared_ptr<crallocator> a = TSNEW(crallocator);

        entries.clear();
        entries.reserve(ginf.number_entry);

        sstr_t<-1032> filename_inzip( 1024, false );
        for (int i=0;i<(int)ginf.number_entry;++i)
        {
            filename_inzip.set_length( 1024 );
            unz_file_info file_info;
            err = unzGetCurrentFileInfo(z->unz,&file_info,filename_inzip.str(),(uLong)filename_inzip.get_capacity(),nullptr,0,nullptr,0);
            if (err!=UNZ_OK)
            {
                entries.clear();
                z->closezip();
                return false;
            }
            filename_inzip.set_length();
            if (file_info.compression_method!=0 && file_info.compression_method!=Z_DEFLATED) goto next;
            if (file_info.size_filename == 0 || __is_slash(filename_inzip.get_last_char())) goto next;

            {
#ifdef _WIN32
                filename_inzip.case_down_ansi();
#endif // _WIN32
                filename_inzip.replace_all(ENEMY_SLASH, NATIVE_SLASH);

                zip_entry_s &e = entries.add();
                e.name = ts::refstring_t<char>::build(filename_inzip, a);
                e.full_unp_size = file_info.uncompressed_size;
                unzGetFilePos64(z->unz, &e.ffs );

                int lsl = filename_inzip.find_last_pos(NATIVE_SLASH);
                if (lsl > 0)
                {
                    filename_inzip.set_length(lsl + 1);
                    for (int j = 0, c = paths.size(); j < c; ++j)
                    {
                        ts::str_c &p = paths.get(j);
                        if (filename_inzip.begins(p) || p.begins(filename_inzip))
                        {
                            if (filename_inzip.get_length() > p.get_length())
                                p = filename_inzip;
                            filename_inzip.clear();
                            break;
                        }
                    }
                    if (!filename_inzip.is_empty())
                        paths.add(filename_inzip);
                }

            }
next:
            if ((i+1)<(int)ginf.number_entry)
            {
                err = unzGoToNextFile(z->unz);
                if (err!=UNZ_OK)
                {
                    entries.clear();
                    z->closezip();
                    return false;
                }
            }
        }

        entries.asort();
        paths.asort();

        return true;
    }
    bool    zip_container_c::close()
    {
        return zippp::close(this);
    }

    bool   zip_container_c::read(const asptr &filename, buf_wrapper_s &b)
    {
        aint index = -1;
        if (entries.find_sorted(index, filename))
        {
            zippp *z = zippp::get(this, false);
            if (z == nullptr)
                return false;
            return entries.get(index).read(z->unz, b);
        }

        return false;
    }

    bool   zip_container_c::read(const wsptr &filename, buf_wrapper_s &b)
    {
        ts::str_c fn(to_utf8(filename));
#ifdef _WIN32
        fn.case_down();
#endif // _WIN32
        fn.replace_all(ENEMY_SLASH, NATIVE_SLASH);

        aint index = -1;
        if (entries.find_sorted(index, fn))
        {
            zippp *z = zippp::get(this, false);
            if (z == nullptr)
                return false;
            return entries.get(index).read(z->unz, b);
        }

        return false;
    }

    size_t    zip_container_c::size(const wsptr &filename)
    {
        ts::str_c fn(to_utf8(filename));
#ifdef _WIN32
        fn.case_down();
#endif // _WIN32
        fn.replace_all(ENEMY_SLASH, NATIVE_SLASH);

        aint index = -1;
        if (entries.find_sorted(index, fn))
            return entries.get(index).full_unp_size;

        return 0;
    }

    bool    zip_container_c::path_exists(const wsptr &path)
    {
        return false;
    }

    bool    zip_container_c::file_exists(const wsptr &filename)
    {
        ts::str_c fn(to_utf8(filename));
#ifdef _WIN32
        fn.case_down();
#endif // _WIN32
        fn.replace_all(ENEMY_SLASH, NATIVE_SLASH);

        aint index = -1;
        return entries.find_sorted(index, fn);
    }

    bool    zip_container_c::iterate_files(ITERATE_FILES_CALLBACK ef)
    {
        wstr_c fn;
        for (zip_entry_s & e : entries)
        {
            fn = from_utf8(e.name->cstr());
            int fni = fn.find_last_pos_of(CONSTWSTR("/\\"));
            if (!ef(fni >=0 ? wsptr(fn.cstr(),fni) : wsptr(), fn.substr(fni+1).as_sptr(),this))
                return false;
        }

        return true;
    }

    bool    zip_container_c::iterate_files(const wsptr &path0, ITERATE_FILES_CALLBACK ef)
    {
        str_c path1( to_utf8(path0) );
#ifdef _WIN32
        path1.case_down();
#endif // _WIN32
        path1.replace_all(ENEMY_SLASH, NATIVE_SLASH);

        if ( !__is_slash(path1.get_last_char()) ) path1.append_char(NATIVE_SLASH);

        aint index = -1;
        if (!entries.find_sorted(index, path1))
            if (index < 0)
                return true;


        wstr_c fn;
        for (aint c = entries.size();index < c; ++index)
        {
            zip_entry_s & e = entries.get(index);
            if (!pstr_c(e.name->cstr()).begins(path1))
                break;

            fn = from_utf8(e.name->cstr());
            int fni = fn.find_last_pos(NATIVE_SLASH);
            if (!ef(fni >= 0 ? wsptr(fn.cstr(), fni) : wsptr(), fn.substr(fni + 1).as_sptr(), this))
                return false;
        }
        return true;
    }

    bool    zip_container_c::iterate_folders(const wsptr &path0, ITERATE_FILES_CALLBACK ef)
    {
        str_c path1(to_utf8(path0));
#ifdef _WIN32
        path1.case_down();
#endif // _WIN32
        path1.replace_all(ENEMY_SLASH, NATIVE_SLASH);

        if (!__is_slash(path1.get_last_char())) path1.append_char(NATIVE_SLASH);

        wstr_c fn;
        str_c prev;

        if (path1.get_length() == 1 && path1.get_char(0) == NATIVE_SLASH)
        {
            for (aint index = 0, c = paths.size(); index < c; ++index)
            {
                const str_c &pe = paths.get(index);

                int ss = pe.find_pos(NATIVE_SLASH);
                if (ss >= prev.get_length() && pe.substr(0, ss).equals(prev.substr(0, ss)))
                    continue;

                fn = from_utf8(pe.substr(0, ss));
                prev = pe;
                if (!ef(wsptr(), fn.as_sptr(), this))
                    return false;
            }
            return true;
        }

        aint index = -1;
        if (!paths.find_sorted(index, path1))
            if (index < 0)
                return true;

        for (aint c = paths.size(); index < c; ++index)
        {
            const str_c &pe = paths.get(index);
            if (pe.get_length() == path1.get_length())
                continue;

            if (!pe.begins(path1))
                break;

            int z = path1.get_length();
            int ss = pe.find_pos(z, NATIVE_SLASH);

            if (ss >= prev.get_length() && pe.substr(0, ss).equals(prev.substr(0, ss)))
                continue;

            fn = from_utf8(pe.substr(0,ss));
            prev = pe;
            ASSERT(z-1 == fn.find_last_pos(NATIVE_SLASH));
            if (!ef(wsptr(fn.cstr(), z-1), fn.substr(z).as_sptr(), this))
                return false;
        }
        return true;
    }

    bool zip_container_c::detect( blob_c &b )
    {
        return *b.data16() == 0x4B50;
    }


} // namespace

