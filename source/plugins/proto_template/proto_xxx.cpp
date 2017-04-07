#include "stdafx.h"

#ifdef _WIN32
#pragma USELIB("plgcommon")

#pragma comment(lib, "Dnsapi.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif

/*
#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif
*/

#include "../appver.inl"

class xxx
{
    host_functions_s *hf;
    contact_state_e self_state;

public:

#define FUNC1( rt, fn, p0 ) rt fn(p0);
#define FUNC2( rt, fn, p0, p1 ) rt fn(p0, p1);
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1

    void on_handshake(host_functions_s *hf_)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif // _WIN32

        hf = hf_;
        self_state = CS_OFFLINE;
    }

};

static xxx cl;



void xxx::call(contact_id_s id, const call_prm_s *ci)
{
}

void xxx::stream_options(contact_id_s id, const stream_options_s * so)
{
}

int xxx::send_av(contact_id_s id, const call_prm_s * ci)
{
    return SEND_AV_OK;
}

void xxx::tick(int *sleep_time_ms)
{
}

void xxx::set_name(const char*utf8name)
{
}

void xxx::set_statusmsg(const char*utf8status)
{
}

void xxx::set_ostate(int ostate)
{
}
void xxx::set_gender(int gender) // unused
{
}
void xxx::set_avatar(const void*data, int isz)
{
}
void xxx::set_config(const void*data, int isz)
{
}
void xxx::save_config(void * param)
{
    savebuffer b;
    
    //save your protocol state to buffer
    //save_state(b);

    hf->on_save(b.data(), static_cast<int>(b.size()), param);
}

void xxx::signal(contact_id_s cid, signal_e s)
{
    switch (s)
    {
    case APPS_INIT_DONE:
        break;
    case APPS_ONLINE:
        break;
    case APPS_OFFLINE:
        break;
    case APPS_GOODBYE:

#ifdef _WIN32
        WSACleanup();
#endif // _WIN32

        break;


    case CONS_ACCEPT_INVITE:
        break;
    case CONS_REJECT_INVITE:
        break;
    case CONS_ACCEPT_CALL:
        break;
    case CONS_STOP_CALL:
        break;
    case CONS_DELETE:
        break;
    case CONS_TYPING:
        break;

    case REQS_DETAILS:
        break;
    case REQS_AVATAR:
        break;
    case REQS_EXPORT_DATA:
        break;
    }
}

int xxx::resend_request(contact_id_s id, const char* invite_message_utf8)
{
    return CR_FUNCTION_NOT_FOUND;
}

void xxx::contact(const contact_data_s * icd)
{
}

int xxx::add_contact(const char* public_id, const char* invite_message_utf8)
{
    return CR_INVALID_PUB_ID;

}
void xxx::send_message(contact_id_s id, const message_s *msg)
{
}

void xxx::del_message( u64 utag )
{
}

void xxx::file_send(contact_id_s cid, const file_send_info_prm_s *finfo)
{
}

void xxx::file_accept(u64 utag, u64 offset)
{
}

void xxx::file_control(u64 utag, file_control_e fctl)
{
}
bool xxx::file_portion(u64 utag, const file_portion_prm_s *portion)
{
    return false;

}

void xxx::folder_share_ctl(u64 utag, const folder_share_control_e ctl)
{

}

void xxx::folder_share_toc(contact_id_s cid, const folder_share_prm_s *sfd)
{
}

void  xxx::folder_share_query(u64, const folder_share_query_prm_s *prm)
{

}


void xxx::create_conference(const char *groupaname, const char *options)
{
}
void xxx::ren_conference(contact_id_s gid, const char *groupaname)
{
}
void xxx::join_conference(contact_id_s gid, contact_id_s cid)
{
}
void xxx::del_conference(const char *conference_id)
{
}
void xxx::enter_conference(const char *conference_id)
{
}
void xxx::leave_conference(contact_id_s gid, int keep_leave)
{
}
void xxx::logging_flags(unsigned int f)
{
}
void xxx::proto_file(i32, const file_portion_prm_s *)
{
}


#define FUNC1( rt, fn, p0 ) rt PROTOCALL static_##fn(p0 pp0) { return cl.fn(pp0); }
#define FUNC2( rt, fn, p0, p1 ) rt PROTOCALL static_##fn(p0 pp0, p1 pp1) { return cl.fn(pp0, pp1); }
PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1

proto_functions_s funcs =
{
#define FUNC1( rt, fn, p0 ) &static_##fn,
#define FUNC2( rt, fn, p0, p1 ) &static_##fn,
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
};

proto_functions_s* PROTOCALL api_handshake(host_functions_s *hf_)
{
    cl.on_handshake(hf_);
    return &funcs;
}


void PROTOCALL api_getinfo( proto_info_s *info )
{

#define  NL "\n"

    static const char *strings[ _is_count_ ] =
    {
        "xxx", 						// three letters of proto
        "Super protocol",           // Name of protocol
        "<b>Super</b> protocol",	// Name of protocol with tags
        SS( PLUGINVER ),			// version of protocol
        "superprotocol.com",		// homesite of protocol
        "",
        "m 4.9325858,22.946568 c 0.5487349,21.4588 18.1769072,43.713547 39.3746742,56.129555 -4.846477,3.759294 -10.312893,6.622086 -16.264974,8.120824 -0.0076,0.429798 -0.0075,0.890126 -0.014,1.28315 7.580261,-1.02812 15.451876,-3.501307 21.728625,-6.455133 7.249049,3.523305 15.796858,6.304913 22.897411,6.959039 0.0048,-0.425597 -0.0072,-0.87415 -0.01398,-1.283087 C 66.365608,86.12091 60.638252,83.014858 55.600825,78.950147 76.72364,66.739459 94.126528,44.562328 94.679221,22.948901 85.860244,26.806022 75.627343,29.821243 67.342306,32.266499 l -0.0024,0.0024 c 0.04002,0.681554 0.06294,1.367504 0.06294,2.055287 0,12.771327 -6.49847,28.367452 -17.28445,39.519312 C 39.609264,62.728786 33.279549,47.415326 33.279549,34.82808 c 0,-0.688814 0.0212,-1.371406 0.06066,-2.052951 l 0,-0.0024 C 23.585914,30.325955 13.251707,26.176417 4.9325858,22.946568 Z",
        "",
        "",
        "uin",
        "f=png,jpg,gif" NL "s=8192" NL "a=32" NL "b=96",
        "", //CONFA_OPT_TYPE "=t/a",
        "", //"%APPDATA%\\tox", WINDOWS_ONLY // path to import
        "", //"%APPDATA%\\tox\\isotoxin_tox_save.tox", WINDOWS_ONLY // file
        nullptr
    };

    info->strings = strings;

    info->priority = 500;
    info->indicator = 1000;
    info->features = PF_UNAUTHORIZED_CHAT | PF_UNAUTHORIZED_CONTACT | PF_AVATARS | PF_NEW_REQUIRES_LOGIN | PF_OFFLINE_MESSAGING | PF_AUTH_NICKNAME | PF_SEND_FILE; // | PF_IMPORT | PF_EXPORT | PF_AUDIO_CALLS | PF_VIDEO_CALLS | PF_GROUP_CHAT | PF_INVITE_NAME;
    info->connection_features = CF_PROXY_SUPPORT_HTTPS | CF_PROXY_SUPPORT_SOCKS5 | CF_enc_only | CF_trust_only; // | CF_IPv6_OPTION | CF_UDP_OPTION | CF_SERVER_OPTION;

}


