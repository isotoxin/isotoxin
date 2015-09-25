#include "toolset.h"

// based on https://github.com/libyal/liblnk (worst code ever, imho)

namespace ts
{
    enum LIBLNK_DATA_FLAGS
    {
        LIBLNK_DATA_FLAG_HAS_LINK_TARGET_IDENTIFIER = 0x00000001UL,
        LIBLNK_DATA_FLAG_HAS_LOCATION_INFORMATION = 0x00000002UL,
        LIBLNK_DATA_FLAG_HAS_DESCRIPTION_STRING = 0x00000004UL,
        LIBLNK_DATA_FLAG_HAS_RELATIVE_PATH_STRING = 0x00000008UL,
        LIBLNK_DATA_FLAG_HAS_WORKING_DIRECTORY_STRING = 0x00000010UL,
        LIBLNK_DATA_FLAG_HAS_COMMAND_LINE_ARGUMENTS_STRING = 0x00000020UL,
        LIBLNK_DATA_FLAG_HAS_ICON_LOCATION_STRING = 0x00000040UL,
        LIBLNK_DATA_FLAG_IS_UNICODE = 0x00000080UL,
        LIBLNK_DATA_FLAG_FORCE_NO_LOCATION_INFORMATION = 0x00000100UL,
        LIBLNK_DATA_FLAG_HAS_ENVIRONMENT_VARIABLES_LOCATION_BLOCK = 0x00000200UL,
        LIBLNK_DATA_FLAG_RUN_IN_SEPARATE_PROCESS = 0x00000400UL,

        LIBLNK_DATA_FLAG_HAS_DARWIN_IDENTIFIER = 0x00001000UL,
        LIBLNK_DATA_FLAG_RUN_AS_USER = 0x00002000UL,
        LIBLNK_DATA_FLAG_HAS_ICON_LOCATION_BLOCK = 0x00004000UL,
        LIBLNK_DATA_FLAG_NO_PIDL_ALIAS = 0x00008000UL,

        LIBLNK_DATA_FLAG_RUN_WITH_SHIM_LAYER = 0x00020000UL,
        LIBLNK_DATA_FLAG_NO_DISTRIBUTED_LINK_TRACKING_DATA_BLOCK = 0x00040000UL,
        LIBLNK_DATA_FLAG_HAS_METADATA_PROPERTY_STORE_DATA_BLOCK = 0x00080000UL
    };

#pragma pack(push, 1)
struct lnk_file_header_s
{
	/* The header size
	 * Consists of 4 bytes
	 */
	uint32 header_size;

	/* Class identifier
	 * Consists of 16 bytes
	 * Contains the little-endian GUID: {00021401-0000-0000-00c0-000000000046}
	 */
	uint8_t class_identifier[ 16 ];

	/* Data flags
	 * Consists of 4 bytes
	 */
	uint32 data_flags;

	/* File attribute flags
	 * Consists of 4 bytes
	 */
	uint8_t file_attribute_flags[ 4 ];

	/* Creation date and time
	 * Consists of 8 bytes
	 * Contains a filetime
	 */
	uint8_t creation_time[ 8 ];

	/* Last access date and time
	 * Consists of 8 bytes
	 * Contains a filetime
	 */
	uint8_t access_time[ 8 ];

	/* Last modification date and time
	 * Consists of 8 bytes
	 * Contains a filetime
	 */
	uint8_t modification_time[ 8 ];

	/* The size of the file
	 * Consists of 4 bytes
	 */
	uint8_t file_size[ 4 ];

	/* The icon index value
	 * Consists of 4 bytes
	 */
	uint8_t icon_index[ 4 ];

	/* The ShowWindow value
	 * Consists of 4 bytes
	 */
	uint8_t show_window_value[ 4 ];

	/* The hot key value
	 * Consists of 2 bytes
	 */
	uint8_t hot_key_value[ 2 ];

	/* Reserved
	 * Consists of 10 bytes
	 * a 2 byte value
	 * a 4 byte value
	 * a 4 byte value
	 */
	uint8_t reserved[ 10 ];
};

struct lnk_location_information_s
{
	/* The size of the location information header
	 * Consists of 4 bytes
	 */
	uint32 header_size;

	/* The location flags
	 * Consists of 4 bytes
	 */
	uint32 location_flags;

	/* The offset of the volume information
	 * Consists of 4 bytes
	 */
	uint32 volume_information_offset;

	/* The offset of the local path
	 * Consists of 4 bytes
	 */
	uint32 local_path_offset;

	/* The offset of the network share information
	 * Consists of 4 bytes
	 */
	uint32 network_share_information_offset;

	/* The offset of the common path
	 * Consists of 4 bytes
	 */
	uint32 common_path_offset;

	/* The following values are only available if the header size > 28
	 */

	/* The offset of the unicode local path
	 * Consists of 4 bytes
	 */
	uint32 unicode_local_path_offset;

	/* The following values are only available if the header size > 32
	 */

	/* The offset of the unicode common path
	 * Consists of 4 bytes
	 */
	uint32 unicode_common_path_offset;
};

#if 0
struct lnk_volume_information_s
{
	/* The size of the volume information
	 * Consists of 4 bytes
	 */
	uint32 size;

	/* The drive type
	 * Consists of 4 bytes
	 */
	uint8_t drive_type[ 4 ];

	/* The drive serial number
	 * Consists of 4 bytes
	 */
	uint8_t drive_serial_number[ 4 ];

	/* The offset of the volume label
	 * Consists of 4 bytes
	 */
	uint32 volume_label_offset;

	/* The following values are only available if the volume label offset > 16
	 */

	/* The offset of the unicode volume label
	 * Consists of 4 bytes
	 */
	uint32 unicode_volume_label_offset;
};

struct lnk_network_share_information_s
{
	/* The size of the network share information
	 * Consists of 4 bytes
	 */
	uint32 size;

	/* The network share type
	 * Consists of 4 bytes
	 */
	uint8_t network_share_type[ 4 ];

	/* The offset of the network share name
	 * Consists of 4 bytes
	 */
	uint32 network_share_name_offset;

	/* The offset of the device name
	 * Consists of 4 bytes
	 */
	uint32 device_name_offset;

	/* The network provide type
	 * Consists of 4 bytes
	 */
	uint8_t network_provider_type[ 4 ];

	/* The following values are only available if the network share name offset > 20
	 */

	/* The offset of the unicode network share name
	 * Consists of 4 bytes
	 */
	uint32 unicode_network_share_name_offset;

	/* The offset of the unicode device name
	 * Consists of 4 bytes
	 */
	uint32 unicode_device_name_offset;
};
#endif

#pragma pack(pop)

template<typename T> T readt(const T *d)
{
    return d ? (*d) : 0;
}

bool lnk_s::read()
{
    aint file_offset = 0;

    auto readblock = [&](aint size)-> const uint8 *
    {
        if (!size) return nullptr;
        if ( (file_offset + size) > datasize ) return nullptr;
        const uint8 *r = data + file_offset;
        file_offset += size;
        return r;
    };

#define READSTRUCT( s ) (const s *)readblock(sizeof(s))
#define READT( t ) readt<t>((const t *)readblock(sizeof(t)))

    const lnk_file_header_s *header = READSTRUCT( lnk_file_header_s );

    auto readstr = [&]( wstr_c * out )->bool
    {
        uint16 sz = READT(uint16);

        if ((header->data_flags & LIBLNK_DATA_FLAG_IS_UNICODE) != 0)
        {
            if (sz * 2 + file_offset > datasize)
                return false;

            const wchar *s = (const wchar *)readblock( sz * 2 );
            if (out) out->set( wsptr(s,sz) );

        } else
        {
            if (sz + file_offset > datasize)
                return false;

            const char *s = (const char *)readblock( sz );
            if (out) out->set(to_wstr(asptr(s,sz)));
        }

        return true;
    };


    if( !header || header->header_size != 0x4c )
        return false;

    const uint8 *tgtid = nullptr;
    if ((header->data_flags & LIBLNK_DATA_FLAG_HAS_LINK_TARGET_IDENTIFIER) != 0)
    {
        uint16 sz = READT(uint16);
        tgtid = readblock(sz);
    }
    if ((header->data_flags & LIBLNK_DATA_FLAG_HAS_LOCATION_INFORMATION) != 0)
    {
        aint location_information_size = READT(uint32);

	    if( location_information_size <= 4 )
            return false;

	    location_information_size -= 4;

	    const uint8 * location_information_data = readblock(location_information_size);

        const lnk_location_information_s *li = (lnk_location_information_s *)location_information_data;

	    aint location_information_header_size = li->header_size;
        //aint volume_information_offset = li->volume_information_offset;
        aint local_path_offset = li->local_path_offset;
        //aint network_share_information_offset = li->network_share_information_offset;
        //aint common_path_offset = li->common_path_offset;

	    if( ( location_information_header_size != 28 ) && ( location_information_header_size != 32 ) && ( location_information_header_size != 36 ) )
            return false;

        aint unicode_local_path_offset = 0;
	    if( location_information_header_size > 28 )
            unicode_local_path_offset = li->unicode_local_path_offset;

        aint unicode_common_path_offset = 0;
	    if( location_information_header_size > 32 )
	        unicode_common_path_offset = li->unicode_common_path_offset;

#if 0
	    // Volume information path
	    if( ( ( li->location_flags & 0x00000001UL ) != 0 ) && ( volume_information_offset > 0 ) )
	    {
		    if( volume_information_offset < location_information_header_size )
                return false;

		    volume_information_offset -= 4;

		    if( location_information_size < 4 )
                return false;

		    if( volume_information_offset > ( location_information_size - 4 ) )
                return false;

            const uint8 *location_information_value_data = location_information_data + volume_information_offset;
            const lnk_volume_information_s *vi = (const lnk_volume_information_s *)location_information_value_data;
            aint location_information_value_size = vi->size;


		    if( location_information_value_size > ( location_information_size - volume_information_offset ) )
                return false;

            if( location_information_value_size < 16 )
                return false;

            aint volume_label_offset = vi->volume_label_offset;
            aint unicode_volume_label_offset = 0;

		    if( volume_label_offset > 16 )
		        unicode_volume_label_offset = vi->unicode_volume_label_offset;

            const char *volume_label = "";

		    if( volume_label_offset > 0 )
		    {
			    if( volume_label_offset > location_information_value_size )
                    return false;

                location_information_value_data = location_information_value_data + volume_label_offset;
                volume_label = (const char *)location_information_value_data;

                int volume_label_max_len = location_information_value_size - volume_label_offset;
		    }

            const wchar *unicode_volume_label = L"";

		    if( unicode_volume_label_offset > 0 )
		    {
			    if( unicode_volume_label_offset > location_information_value_size )
                    return false;

                const uint8 *location_information_unicode_value_data = location_information_value_data + unicode_volume_label_offset;
                unicode_volume_label = (const wchar *)location_information_unicode_value_data;
		
		    }
	    }
#endif

        // Local path
	    if( ( li->location_flags & 0x00000001UL ) != 0 )
	    {
            const char * ansi_local_path = "";
		    if( local_path_offset > 0 )
		    {
			    if( local_path_offset < location_information_header_size )
                    return false;

                local_path_offset -= 4;

			    if( local_path_offset > location_information_size )
                    return false;

                ansi_local_path = (const char *)(location_information_data + local_path_offset);
		    }

            if (unicode_local_path_offset > 0)
		    {
			    if( unicode_local_path_offset < location_information_header_size )
                    return false;

                unicode_local_path_offset -= 4;

			    if( unicode_local_path_offset > location_information_size )
                    return false;

                const wchar *unicode_local_path = (const wchar *)(location_information_data + unicode_local_path_offset);
                local_path.set( wsptr(unicode_local_path) );


		    } else if( local_path_offset > 0 )
		    {
                local_path.set( to_wstr(ansi_local_path) );
		    }
	    }

#if 0
	    // Network share information
	    if( ( ( li->location_flags & 0x00000002UL ) != 0 ) && ( network_share_information_offset > 0 ) )
	    {
		    if( network_share_information_offset < location_information_header_size )
                return false;

            network_share_information_offset -= 4;

		    if( location_information_size < 4 )
                return false;

            if( network_share_information_offset > ( location_information_size - 4 ) )
                return false;

            const uint8 *location_information_value_data = location_information_data + network_share_information_offset;

            const lnk_network_share_information_s *shi = (const lnk_network_share_information_s *)location_information_value_data;
            aint location_information_value_size = shi->size;

		    if( location_information_value_size > ( location_information_size - network_share_information_offset ) )
                return false;

            if( location_information_value_size < 16 )
                return false;

            aint network_share_name_offset = shi->network_share_name_offset;
            aint device_name_offset = shi->device_name_offset;
            aint unicode_network_share_name_offset = 0;
            aint unicode_device_name_offset = 0;

		    if( network_share_name_offset > 20 )
		    {
                unicode_network_share_name_offset = shi->unicode_network_share_name_offset;
                unicode_device_name_offset = shi->unicode_device_name_offset;
		    }

            const char *network_share = "";

		    if( network_share_name_offset > 0 )
		    {
			    if( network_share_name_offset > location_information_value_size )
                    return false;

                network_share = (const char *)(location_information_value_data + network_share_name_offset);
		    }

		    if( unicode_network_share_name_offset > 0 )
		    {
			    if( unicode_network_share_name_offset > location_information_value_size )
                    return false;

                const wchar *unicode_network_share = (const wchar *)(location_information_value_data + unicode_network_share_name_offset);

			    
		    } else if( network_share_name_offset > 0 )
		    {
		    }

            const char *device_name = "";

            if (device_name_offset > 0)
		    {
			    if( device_name_offset > location_information_value_size )
                    return false;

                device_name = (const char *)(location_information_value_data + device_name_offset);
		    }
            if (unicode_device_name_offset > 0)
		    {
			    if( unicode_device_name_offset > location_information_value_size )
                    return false;

                const wchar *unicode_device_name = (const wchar *)(location_information_value_data + unicode_device_name_offset);
		    
            } else if( device_name_offset > 0 )
		    {
		    }
	    }
#endif

#if 0
	    // Common path
        const char *common_path = "";
	    if( common_path_offset > 0 )
	    {
		    if( common_path_offset < location_information_header_size )
                return false;

            common_path_offset -= 4;

		    if( common_path_offset > location_information_size )
                return false;

            common_path = (const char *)( location_information_data + common_path_offset );
	    }
	    if( unicode_common_path_offset > 0 )
	    {
		    if( unicode_common_path_offset < location_information_header_size )
                return false;

            unicode_common_path_offset -= 4;

		    if( unicode_common_path_offset > location_information_size )
                return false;

            const wchar *unicode_common_path = (const wchar *)( location_information_data + unicode_common_path_offset );

        } else if( common_path_offset > 0 )
	    {
	    }
#endif
    }
    if ((header->data_flags & LIBLNK_DATA_FLAG_HAS_DESCRIPTION_STRING) != 0)
        if (!readstr(nullptr)) // just skip
            return false;

    if ((header->data_flags & LIBLNK_DATA_FLAG_HAS_RELATIVE_PATH_STRING) != 0)
        if (!readstr(&relative_path))
            return false;

    if ((header->data_flags & LIBLNK_DATA_FLAG_HAS_WORKING_DIRECTORY_STRING) != 0)
        if (!readstr(&working_directory))
            return false;

    if ((header->data_flags & LIBLNK_DATA_FLAG_HAS_COMMAND_LINE_ARGUMENTS_STRING) != 0)
        if (!readstr(&command_line_arguments))
            return false;

    return true;
}



} // namespace ts