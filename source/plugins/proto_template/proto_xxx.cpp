#include "stdafx.h"

#pragma USELIB("plgcommon")

#pragma comment(lib, "Dnsapi.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

#include "appver.inl"

static host_functions_s *hf;
static contact_state_e self_state; // cleared in handshake


void __stdcall offline()
{
}

void __stdcall online()
{
}

void __stdcall call(int id, const call_info_s *ci)
{
}


void __stdcall stop_call(int id)
{
}

void __stdcall accept_call(int id)
{
}

void __stdcall stream_options(int id, const stream_options_s * so)
{
}

int __stdcall send_av(int id, const call_info_s * ci)
{
    return SEND_AV_OK;
}

void __stdcall tick(int *sleep_time_ms)
{
}

void __stdcall goodbye()
{
}

void __stdcall set_name(const char*utf8name)
{
}

void __stdcall set_statusmsg(const char*utf8status)
{
}

void __stdcall set_ostate(int ostate)
{
}
void __stdcall set_gender(int /*gender*/)
{
}
void __stdcall get_avatar(int id)
{
}
void __stdcall set_avatar(const void*data, int isz)
{
}
void __stdcall set_config(const void*data, int isz)
{
}
void __stdcall init_done()
{
}
void __stdcall save_config(void * param)
{
}
int __stdcall resend_request(int id, const char* invite_message_utf8)
{
    return CR_FUNCTION_NOT_FOUND;
}
int __stdcall add_contact(const char* public_id, const char* invite_message_utf8)
{

    return CR_INVALID_PUB_ID;

}
void __stdcall del_contact(int id)
{
}

void __stdcall send_message(int id, const message_s *msg)
{
}

void __stdcall del_message( u64 utag )
{
}

void __stdcall accept(int id)
{
}

void __stdcall reject(int id)
{
}

void __stdcall configurable(const char *field, const char *val)
{
}

void __stdcall file_send(int cid, const file_send_info_s *finfo)
{
}

void __stdcall file_accept(u64 utag, u64 offset)
{
}

void __stdcall file_control(u64 utag, file_control_e fctl)
{
}
bool __stdcall file_portion(u64 utag, const file_portion_s *portion)
{
    return false;

}

void __stdcall add_groupchat(const char *groupaname, bool persistent)
{
}
void __stdcall ren_groupchat(int gid, const char *groupaname)
{
}
void __stdcall join_groupchat(int gid, int cid)
{
}
void __stdcall typing(int cid)
{
}
void __stdcall export_data()
{
}
void __stdcall logging_flags(unsigned int f)
{
}

proto_functions_s funcs =
{
#define FUNC0( rt, fn ) &fn,
#define FUNC1( rt, fn, p0 ) &fn,
#define FUNC2( rt, fn, p0, p1 ) &fn,
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0
};

proto_functions_s* __stdcall api_handshake(host_functions_s *hf_)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    hf = hf_;

    self_state = CS_OFFLINE;

    return &funcs;
}


void __stdcall api_getinfo( proto_info_s *info )
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
        "",
        "uin",
        "f=png,jpg,gif" NL "s=8192" NL "a=32" NL "b=96",
        nullptr
    };

    info->strings = strings;

    info->priority = 500;
    info->indicator = 1000;
    info->features = PF_UNAUTHORIZED_CHAT | PF_UNAUTHORIZED_CONTACT | PF_AVATARS | PF_NEW_REQUIRES_LOGIN | PF_OFFLINE_MESSAGING | PF_AUTH_NICKNAME | PF_SEND_FILE; // | PF_IMPORT | PF_EXPORT | PF_AUDIO_CALLS | PF_VIDEO_CALLS | PF_GROUP_CHAT | PF_INVITE_NAME;
    info->connection_features = CF_PROXY_SUPPORT_HTTPS | CF_PROXY_SUPPORT_SOCKS5; // | CF_IPv6_OPTION | CF_UDP_OPTION | CF_SERVER_OPTION;

}


