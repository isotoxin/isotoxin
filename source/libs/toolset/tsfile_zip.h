#ifndef _INCLUDE_TSFILE_ZIP
#define _INCLUDE_TSFILE_ZIP

#include "zlib/src/contrib/minizip/unzip.h"

namespace ts
{
    class zip_container_c : public container_c
    {

        unzFile         m_unz;
        unz_global_info m_ginf;

        class zip_entry_c
        {
            wstr_c m_name;
        public:
            zip_entry_c( const wsptr &name ): m_name( name ) {}
            virtual ~zip_entry_c() {}

            const wstr_c & name(void) const {return m_name;}

        };

        class zip_file_entry_c : public zip_entry_c
        {
        public:
            unz64_file_pos  m_ffs;
            aint            m_full_unp_size;

        public:
            zip_file_entry_c( const wsptr &name ): zip_entry_c( name ) {}
            virtual ~zip_file_entry_c();

            bool  read( unzFile unz, buf_wrapper_s &b );
            size_t  size() const {return m_full_unp_size; }
        };

        class zip_folder_entry_c : public zip_entry_c
        {
            array_del_t<zip_folder_entry_c, 16> m_folders;
            array_del_t<zip_file_entry_c, 32> m_files;

        public:
            zip_folder_entry_c( const wsptr &name ): zip_entry_c( name ) {}
            virtual ~zip_folder_entry_c() {}

            zip_folder_entry_c * get_folder( const wsptr &fnn );
            zip_folder_entry_c * add_folder( const wsptr &fnn );
            zip_file_entry_c * add_file( const wsptr &fnn );
            zip_file_entry_c * get_file( const wsptr &fnn );

            bool               file_exists(const wsptr &fnn);
            bool               iterate_files(const wsptr &path_orig, const wsptr &path0, ITERATE_FILES_CALLBACK ef, container_c *pf);
            bool               iterate_files(const wsptr &path_orig, ITERATE_FILES_CALLBACK ef, container_c *pf);
            bool               iterate_folders(const wsptr &path_orig, const wsptr &path0, ITERATE_FILES_CALLBACK ef, container_c *pf);

        };

        zip_folder_entry_c m_root;

        wstr_c              m_find_cache_file_name;
        zip_file_entry_c   *m_find_cache_file_entry;

    private:

        zip_file_entry_c * add_path( const wsptr &path );
        zip_folder_entry_c * get_folder( const wsptr &fnn );


    public:

        zip_container_c(const wsptr &name, int prior, uint id);
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

        static bool Detect( HANDLE file, LPFILETIME pFileTime ); 

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

#endif