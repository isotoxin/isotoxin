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

void __stdcall get_info( proto_info_s *info )
{
    if (info->protocol_name) strncpy_s(info->protocol_name, info->protocol_name_buflen, "tgm", _TRUNCATE);
    if (info->description) strncpy_s(info->description, info->description_buflen, "Telegram protocol", _TRUNCATE);
    if (info->description_with_tags) strncpy_s(info->description_with_tags, info->description_with_tags_buflen, "<b>Telegram</b> protocol", _TRUNCATE);
    if (info->version)
    {
        sstr_t<1024> vers( "plugin: " SS(PLUGINVER) ", mtproto: " ); 

        //vers.append_as_uint(TOX_VERSION_MAJOR);
        //vers.append_char('.');
        //vers.append_as_uint(TOX_VERSION_MINOR);
        //vers.append_char('.');
        //vers.append_as_uint(TOX_VERSION_PATCH);
        //vers.append_char(')');

        strncpy_s(info->version, info->version_buflen, vers, _TRUNCATE);
    }

    info->priority = 500;
    info->features = PF_AVATARS | PF_OFFLINE_INDICATOR | PF_PURE_NEW | /*PF_IMPORT | PF_EXPORT | PF_AUDIO_CALLS | PF_VIDEO_CALLS |*/ PF_SEND_FILE | PF_GROUP_CHAT; //PF_INVITE_NAME | PF_UNAUTHORIZED_CHAT;
    info->connection_features = 0; // CF_PROXY_SUPPORT_HTTPS | CF_PROXY_SUPPORT_SOCKS5 | CF_IPv6_OPTION | CF_UDP_OPTION | CF_SERVER_OPTION;
    info->audio_fmt.sample_rate = 48000;
    info->audio_fmt.channels = 1;
    info->audio_fmt.bits = 16;

    info->icon = "m 74.044499,38.828783 c 0,-4.166708 0.06581,-9.385543 -0.02935,-13.551362 -0.0418,-1.794753 -0.238352,-3.62508 -0.663471,-5.366471 C 70.309137,7.460631 58.257256,-0.28224198 45.602382,2.007891 34.399851,4.0347699 26.06021,14.030422 26.06021,25.430393 l 0,13.371709 c -3.678661,-3.2e-4 -7.473489,0 -11.204309,0 -2.466229,0 -4.467316,2.000197 -4.467316,4.467315 l 0,50.583035 c 0,2.466229 2.001087,4.467316 4.467316,4.467316 l 70.661498,0 c 2.468007,0 4.468205,-2.001087 4.468205,-4.467316 l 0,-50.583035 c 0,-2.467118 -2.000198,-4.467315 -4.468205,-4.467315 z m -17.313405,47.37418 -13.140472,0 c -2.065122,0 -3.738031,-1.702258 -3.738031,-3.801176 0.452829,-4.54018 0.52948,-5.74902 1.833885,-7.830921 1.45946,-2.150501 3.42853,-3.548594 5.564801,-4.17738 -2.815752,-1.148179 -4.799051,-3.911458 -4.799051,-7.137211 0,-4.259202 3.450763,-7.709077 7.708187,-7.709077 4.259203,0 7.708188,3.449875 7.708188,7.709077 0,3.307575 -2.084688,6.128663 -5.01339,7.221701 2.074015,0.669697 4.03508,2.057118 5.57903,4.186274 1.608048,2.307856 2.032215,4.874192 2.032215,7.571225 -0.0053,0.05514 0,0.111171 0,0.168091 8.9e-4,2.097139 -1.672019,3.799397 -3.735362,3.799397 M 65.3331,31.049446 c -1.389199,4.100894 -4.019961,7.386234 -6.768121,10.594199 -2.532759,2.638121 -5.259444,5.204412 -7.880726,6.825041 1.583972,-2.735708 2.86111,-5.334453 3.231089,-8.363654 -3.765601,0.891151 -7.316864,0.501606 -10.755176,-1.013884 -6.102871,-2.688572 -9.750186,-8.553981 -9.127625,-15.175356 0.50961,-5.449182 3.605395,-10.490611 8.391106,-13.0271 7.708187,-4.0848867 17.24706,-1.0668343 21.653898,6.200225 2.567617,4.231632 2.810178,9.372258 1.255555,13.960529";
    info->icon_buflen = strlen(info->icon);

    info->vcodecs = nullptr;
    info->vcodecs_buflen = 0;
}

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

void __stdcall file_resume(u64 utag, u64 offset)
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

proto_functions_s* __stdcall handshake(host_functions_s *hf_)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    hf = hf_;

    self_state = CS_OFFLINE;

    return &funcs;
}




