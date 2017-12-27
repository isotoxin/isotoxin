#pragma once

#include "zlib/src/contrib/minizip/unzip.h"

namespace ts
{
    class zip_container_c : public container_c
    {
        friend struct zippp;

#ifdef MODE64
        uint8 data[65664];
#else
        uint8 data[65624];
#endif

        struct zip_entry_s : movable_flag<true>
        {
            ts::shared_ptr< ts::refstring_t<char> > name;
            unz64_file_pos  ffs;
            uint            full_unp_size;

            int operator()(const zip_entry_s &e) const
            {
                return str_c::compare(name->cstr(), e.name->cstr());
            }
            int operator()(const str_c &s) const
            {
                return str_c::compare(name->cstr(), s);
            }

            bool  read(unzFile unz, buf_wrapper_s &b);
        };

        ts::array_inplace_t<zip_entry_s, 1> entries;
        ts::astrings_c paths;

        void build_z();


    public:

        zip_container_c() :container_c(ts::wstr_c(), 0, 0) { build_z(); } //, m_root(wstr_c()), m_find_cache_file_entry(nullptr) {}
        zip_container_c(const wstr_c &name, int prior, uint id) :container_c(name, prior, id) { build_z(); }
        virtual ~zip_container_c();

        /*virtual*/ bool    open() override;
        /*virtual*/ bool    close() override;

        /*virtual*/ bool    read(const wsptr &filename, buf_wrapper_s &b) override;
        /*virtual*/ size_t  size(const wsptr &filename) override;

        /*virtual*/ bool    path_exists(const wsptr &path) override;
        /*virtual*/ bool    file_exists(const wsptr &path) override;
        /*virtual*/ bool    iterate_folders(const wsptr &path, ITERATE_FILES_CALLBACK ef) override;
        /*virtual*/ bool    iterate_files(const wsptr &path, ITERATE_FILES_CALLBACK ef) override;
        /*virtual*/ bool    iterate_files(ITERATE_FILES_CALLBACK ef) override;

        static bool detect( blob_c & );

    };

    /*
    class zip_container_c
    {
    private:
        zipFile m_zf;

    public:
        zip_container_c();
        ~zip_container_c();
        bool CreateContainer(const wsptr &ZipFileName, bool bOverWrite = true);
        bool AddFileData(const wsptr &FileName, const void *pdata, DWORD dwSize, int CompressionLevel = 0);
        bool CloseContainer();
    };
    */


} // namespace ts

