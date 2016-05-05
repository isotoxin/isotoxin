#include "toolset.h"

namespace ts 
{

    zip_container_c::zip_folder_entry_c * zip_container_c::zip_folder_entry_c::get_folder( const wsptr &fnn )
    {
        for (zip_folder_entry_c *fe : m_folders)
            if (fe->name().equals(fnn)) return fe;
        return nullptr;
    }

    zip_container_c::zip_file_entry_c * zip_container_c::zip_folder_entry_c::get_file( const wsptr &fnn )
    {
        for (zip_file_entry_c *f : m_files)
            if (f->name().equals(fnn)) return f;
        return nullptr;
    }

    bool   zip_container_c::zip_folder_entry_c::file_exists(const wsptr &fnn)
    {
        for (zip_file_entry_c *f : m_files)
            if (f->name().equals(fnn)) return true;
        return false;
    }

    bool    zip_container_c::zip_folder_entry_c::iterate_files(const wsptr &path_orig, ITERATE_FILES_CALLBACK ef, container_c *pf)
    {
        tmp_wstr_c path(path_orig);
        if (!path.is_empty() && !__is_slash(path.get_last_char())) path.append_char(NATIVE_SLASH);

        for (zip_file_entry_c *f : m_files)
            if (!ef(path, f->name(), pf)) return false;

        for (zip_folder_entry_c *fe : m_folders)
            if (!fe->iterate_files(path + fe->name(), ef, pf)) return false;
        return true;
    }


    bool    zip_container_c::zip_folder_entry_c::iterate_files(const wsptr &path_orig, const wsptr &path, ITERATE_FILES_CALLBACK ef, container_c *pf)
    {
        tmp_wstr_c pa( path ), cf;
        ASSERT(pa.find_pos(ENEMY_SLASH) < 0);
        if (pa.get_char(0) == NATIVE_SLASH) pa.cut(0, 1);
        int pad = pa.find_pos(NATIVE_SLASH);
        if ( pad >= 0 )
        {
            cf = pa.substr(0,pad);
            pa.cut(0,pad+1);
        } else
        {
            cf = pa;
            pa.clear();
        }

        if (!cf.is_empty())
        {
            // find folder
            for (zip_folder_entry_c *fe : m_folders)
                if (fe->name().equals( cf ))
                    if (!fe->iterate_files( path_orig, pa, ef, pf ))
						return false;
        } else
        {
            for (zip_file_entry_c *f : m_files)
                if (!ef( path_orig, f->name(), pf ))
					return false;

        }

		return true;
    }

    bool zip_container_c::zip_folder_entry_c::iterate_folders(const wsptr &path_orig, const wsptr &path, ITERATE_FILES_CALLBACK ef, container_c *pf)
    {
        tmp_wstr_c pa(path), cf;
        ASSERT(pa.find_pos(ENEMY_SLASH) < 0);
        if (pa.get_char(0) == NATIVE_SLASH) pa.cut(0, 1);
        int pad = pa.find_pos(NATIVE_SLASH);
        if (pad >= 0)
        {
            cf = pa.substr(0, pad);
            pa.cut(0, pad + 1);
        }
        else
        {
            cf = pa;
            pa.clear();
        }

        if (!cf.is_empty())
        {
            // find folder
            for (zip_folder_entry_c *fe : m_folders)
                if (fe->name().equals(cf))
                    if (!fe->iterate_folders(path_orig, pa, ef, pf))
                        return false;
        }
        else
        {
            for (zip_folder_entry_c *f : m_folders)
                if (!ef(path_orig, f->name(), pf))
                    return false;

        }

        return true;
    }

    zip_container_c::zip_folder_entry_c * zip_container_c::zip_folder_entry_c::add_folder( const wsptr &fnn )
    {
        ASSERT( get_folder(fnn) == nullptr );
        zip_folder_entry_c *nf = TSNEW( zip_folder_entry_c, fnn );
        m_folders.add( nf );
        return nf;
    }

    zip_container_c::zip_file_entry_c * zip_container_c::zip_folder_entry_c::add_file( const wsptr &fnn )
    {
        ASSERT( get_file(fnn) == nullptr );
        zip_file_entry_c *nf = TSNEW( zip_file_entry_c, fnn );
        m_files.add( nf );
        return nf;
    }

    zip_container_c::zip_container_c(const wsptr &name, int prior, uint id):container_c(name, prior, id), 
        m_unz(nullptr),
        m_root( wstr_c() ), 
        m_find_cache_file_entry(nullptr)
    {
    }

    zip_container_c::zip_file_entry_c::~zip_file_entry_c()
    {
    }

    bool    zip_container_c::zip_file_entry_c::read( unzFile unz, buf_wrapper_s &b )
    {
        unzGoToFilePos64(unz, &m_ffs);

        if (unzOpenCurrentFile(unz) == UNZ_OK)
        {
            aint n = unzReadCurrentFile(unz, b.alloc(m_full_unp_size), (int)m_full_unp_size);
            if (unzCloseCurrentFile(unz) == UNZ_OK && n == m_full_unp_size)
                return true;
        }
		return false;
    }

    zip_container_c::~zip_container_c()
    {
        close();
    }

    zip_container_c::zip_file_entry_c * zip_container_c::add_path( const wsptr &path0 )
    {
        wstr_c path1( path0 );
        fix_path(path1, FNO_LOWERCASEAUTO | FNO_NORMALIZE);
        const wchar *path = path1.cstr();

        zip_folder_entry_c *cur = &m_root;

        for (;;)
        {
            int ind = CHARz_find<wchar>( path, NATIVE_SLASH );
            if ( ind >= 0 )
            {
                zip_folder_entry_c *f = cur->get_folder(wsptr(path, ind));
                if (f)
                {
                    cur = f;
                } else
                {
                    cur = cur->add_folder( wsptr(path, ind) );
                }
                path += ind + 1;
                continue;
            }

            // file!
            zip_file_entry_c *fe = cur->add_file( path );
            return fe;
        }
        UNREACHABLE();
    }


    zip_container_c::zip_folder_entry_c * zip_container_c::get_folder( const wsptr &fnn )
    {
        return m_root.get_folder( fnn );
    }


    bool    zip_container_c::open()
    {
        if (m_unz) return true;

        struct zippp : zlib_filefunc_def
        {
            ts::wstr_c name;

            static voidpf ZCALLBACK fopen_file_func(voidpf opaque, const char * filename, int mode)
            {
                return f_open( ( (zippp *)opaque )->name );

                /*
                HANDLE file, h;
                DWORD desiredacces = GENERIC_READ;
                DWORD createdispos = OPEN_EXISTING;

                if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)
                {
                    desiredacces = GENERIC_READ;
                    createdispos = OPEN_EXISTING;

                } else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
                {
                    desiredacces = GENERIC_READ | GENERIC_WRITE;
                    createdispos = OPEN_EXISTING;

                } else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
                {
                    desiredacces = GENERIC_READ | GENERIC_WRITE;
                    createdispos = CREATE_ALWAYS;
                }

                h = CreateFileW(((zippp *)opaque)->name, desiredacces, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, createdispos, FILE_ATTRIBUTE_NORMAL, nullptr);
                file = (HANDLE)((aint)h + 1);
                return file;
                */
            }

            static uLong ZCALLBACK fread_file_func(voidpf opaque, voidpf stream, void* buf, uLong size)
            {
                return (uLong)f_read(stream, buf, size);
            }
            static long ZCALLBACK ftell_file_func(voidpf opaque, voidpf stream)
            {
                return (long)f_get_pos(stream);
            }
            static long ZCALLBACK fseek_file_func(voidpf opaque, voidpf stream, uLong offset, int origin)
            {
                uint64 o = offset;
                switch (origin)
                {
                    case ZLIB_FILEFUNC_SEEK_END:
                        o += f_size(stream);
                        break;
                    case ZLIB_FILEFUNC_SEEK_CUR:
                        o += f_get_pos( stream );
                        break;
                }

                return f_set_pos( stream, o ) ? 0 : 1;
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

            zippp(const ts::wstr_c &name):name(name)
            {
                zopen_file = fopen_file_func;
                zread_file = fread_file_func;
                zwrite_file = nullptr; /*fwrite_file_func*/;
                ztell_file = ftell_file_func;
                zseek_file = fseek_file_func;
                zclose_file = fclose_file_func;
                zerror_file = ferror_file_func;
                opaque = this;
            }

        } z( fn() );


        m_unz = unzOpen2( nullptr, &z );
        if (m_unz == nullptr) return false;
        
        int err = unzGetGlobalInfo (m_unz,&m_ginf);
        if (err!=UNZ_OK)
        {
            unzClose(m_unz);
            m_unz = nullptr;
            return false;
        }

        sstr_t<-1032> filename_inzip( 1024, false );
        for (int i=0;i<(int)m_ginf.number_entry;++i)
        {
            filename_inzip.set_length( 1024 );
            unz_file_info file_info;
            err = unzGetCurrentFileInfo(m_unz,&file_info,filename_inzip.str(),(uLong)filename_inzip.get_capacity(),nullptr,0,nullptr,0);
            if (err!=UNZ_OK)
            {
                unzClose(m_unz);
                m_unz = nullptr;
                return false;
            }
            filename_inzip.set_length();
            if (file_info.compression_method!=0 && file_info.compression_method!=Z_DEFLATED) goto next;
            if (file_info.size_filename == 0 || __is_slash(filename_inzip.get_last_char())) goto next;

            {
                zip_file_entry_c *fe = add_path( to_wstr(filename_inzip.as_sptr()) );
                fe->m_full_unp_size = file_info.uncompressed_size;
                unzGetFilePos64( m_unz, &fe->m_ffs );
            }
next:
            if ((i+1)<(int)m_ginf.number_entry)
            {
                err = unzGoToNextFile(m_unz);
                if (err!=UNZ_OK)
                {
                    unzClose(m_unz);
                    m_unz = nullptr;
                    return false;
                }
            }
        }
        return true;
    }
    bool    zip_container_c::close()
    {
        if (!m_unz) return false;
        unzClose(m_unz);
        m_unz = nullptr;
        return true;
    }

    bool   zip_container_c::read(const wsptr &filename, buf_wrapper_s &b)
    {
        ts::wstr_c fn(filename);
        fix_path(fn, FNO_LOWERCASEAUTO | FNO_NORMALIZE);

        if (m_find_cache_file_name.equals(fn))
        {
            return m_find_cache_file_entry->read( m_unz, b );
        } else
        {
            if (file_exists( fn ))
            {
                return m_find_cache_file_entry->read( m_unz, b );
            }
        }
        return false;
    }

    size_t    zip_container_c::size(const wsptr &filename)
    {
        ts::wstr_c fn(filename);
        fix_path(fn, FNO_LOWERCASEAUTO | FNO_NORMALIZE);

        if (m_find_cache_file_name.equals(fn))
        {
            return m_find_cache_file_entry->size();
        }
        else
        {
            if (file_exists(fn))
            {
                return m_find_cache_file_entry->size();
            }
        }
        return 0;
    }

    bool    zip_container_c::path_exists(const wsptr &path)
    {
        return false;
    }

    bool    zip_container_c::file_exists(const wsptr &path0)
    {
        wstr_c path1( path0 );
        fix_path(path1, FNO_LOWERCASEAUTO | FNO_NORMALIZE);
        const wchar *path = path1.cstr();

        zip_folder_entry_c *cur = &m_root;

        for (;;)
        {
            int ind = CHARz_find<wchar>( path, NATIVE_SLASH );
            if ( ind >= 0 )
            {
                zip_folder_entry_c *f = cur->get_folder(wsptr(path, ind));
                if (f)
                {
                    cur = f;
                } else
                {
                    return false;
                }
                path += ind + 1;
                continue;
            }

            // file!

            zip_file_entry_c *fe = cur->get_file( path );
            if (fe)
            {
                m_find_cache_file_entry = fe;
                m_find_cache_file_name = path0;
                return true;
            }

            return false;
        }

        UNREACHABLE();
    }

    bool    zip_container_c::iterate_files(ITERATE_FILES_CALLBACK ef)
    {
        return m_root.iterate_files(CONSTWSTR(""), ef, this);
    }

    bool    zip_container_c::iterate_files(const wsptr &path0, ITERATE_FILES_CALLBACK ef)
    {
        wstr_c path1( path0 );
        fix_path(path1, FNO_LOWERCASEAUTO | FNO_NORMALIZE);

        tmp_wstr_c pp( path0 );
        if ( !__is_slash(pp.get_last_char()) ) pp.append_char(NATIVE_SLASH);

        return m_root.iterate_files(pp, path1,ef,this);
    }

    bool    zip_container_c::iterate_folders(const wsptr &path0, ITERATE_FILES_CALLBACK ef)
    {
        wstr_c path1(path0);
        fix_path(path1, FNO_LOWERCASEAUTO | FNO_NORMALIZE);

        tmp_wstr_c pp(path0);
        if (!__is_slash(pp.get_last_char())) pp.append_char(NATIVE_SLASH);

        return m_root.iterate_folders(pp, path1, ef, this);
    }

    bool zip_container_c::detect( blob_c &b )
    {
        return *b.data16() == 0x4B50;
    }



#if 0
// ====================================== Zip Container ======================================

    zip_container_c::zip_container_c()
    {
        m_zf = nullptr;
    }
    
    zip_container_c::~zip_container_c()
    {
        ASSERT(m_zf == nullptr);
    }
    
    bool zip_container_c::CreateContainer(const wsptr &ZipFileName, bool bOverWrite)
    {
        m_zf = zipOpen(tmp_wstr_c(ZipFileName), bOverWrite ? 0 : 2);
        return (m_zf != nullptr);
    }
    
    bool zip_container_c::AddFileData(const wsptr &fn, const void *data, DWORD dwSize, int CompressionLevel)
    {
        SYSTEMTIME st;
        FILETIME ft;

        ASSERT(fn.s != nullptr);
        ASSERT(m_zf != nullptr);
        ASSERT(data != nullptr);

        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &ft);

        zip_fileinfo zi;
        zi.tmz_date.tm_sec  = st.wSecond;
        zi.tmz_date.tm_min  = st.wMinute;
        zi.tmz_date.tm_hour = st.wHour;
        zi.tmz_date.tm_mday = st.wDay;
        zi.tmz_date.tm_mon  = st.wMonth;
        zi.tmz_date.tm_year = st.wYear;

        FileTimeToDosDateTime(&ft, ((LPWORD)&zi.dosDate) + 1, ((LPWORD)&zi.dosDate) + 0);

        zi.internal_fa  = 0;
        zi.external_fa  = 0;

        int opt_compress_level = CLAMP(CompressionLevel, 0, 9);

        if (ZIP_OK == zipOpenNewFileInZip(m_zf, tmp_wstr_c(fn), &zi, nullptr, 0, nullptr, 0, nullptr /* comment */,
                                          (opt_compress_level != 0) ? Z_DEFLATED : 0, opt_compress_level))
        {
            if (ZIP_OK == zipWriteInFileInZip(m_zf, data, dwSize))
            {
                if (ZIP_OK == zipCloseFileInZip(m_zf))
                {
                    return true;
                }
            }
        }
        return false;
    }
    
    bool zip_container_c::CloseContainer()
    {
        if (m_zf != nullptr)
        {
            int err = zipClose(m_zf, nullptr);
            m_zf = nullptr;
            return (ZIP_OK == err);
        }
        return true;
    }
#endif

} // namespace

