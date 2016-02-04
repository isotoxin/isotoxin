#include "stdafx.h"

#pragma USELIB("plgcommon")

#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "opus.lib")

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "Iphlpapi.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

#include "appver.inl"

void __stdcall get_info(proto_info_s *info)
{
    if (info->protocol_name) strncpy_s( info->protocol_name, info->protocol_name_buflen, "lan", _TRUNCATE );
    if (info->description) strncpy_s( info->description, info->description_buflen, "Lan protocol", _TRUNCATE );
    if (info->description_with_tags) strncpy_s( info->description_with_tags, info->description_with_tags_buflen, "<b>Lan</b> protocol", _TRUNCATE );
    if (info->version) strncpy_s( info->version, info->version_buflen, SS(PLUGINVER), _TRUNCATE );
    info->priority = 1000;
    info->features = PF_AVATARS | PF_INVITE_NAME | PF_UNAUTHORIZED_CHAT | PF_AUDIO_CALLS | PF_SEND_FILE | PF_PURE_NEW;
    info->connection_features = 0;

    info->audio_fmt.sample_rate = AUDIO_SAMPLERATE;
    info->audio_fmt.channels = AUDIO_CHANNELS;
    info->audio_fmt.bits = AUDIO_BITS;

    info->icon = "M 35 10 L 35 40 L 47.5 40 L 47.5 47.5 L 5 47.5 L 5 52.5 L 22.5 52.5 L 22.5 60 L 10 60 L 10 90 L 40 90 L 40 60 L 27.5 60 L 27.5 52.5 L 72.5 52.5 L 72.5 60 L 60 60 L 60 90 L 90 90 L 90 60 L 77.5 60 L 77.5 52.5 L 95 52.5 L 95 47.5 L 52.5 47.5 L 52.5 40 L 65 40 L 65 10 L 35 10 z ";
    info->icon_buflen = strlen(info->icon);

}

#define FUNC0( rt, fn ) rt __stdcall fn() { return lan_engine::get()->fn(); }
#define FUNC1( rt, fn, p0 ) rt __stdcall fn(p0 pp0) { return lan_engine::get()->fn(pp0); }
#define FUNC2( rt, fn, p0, p1 ) rt __stdcall fn(p0 pp0, p1 pp1) { return lan_engine::get()->fn(pp0, pp1); }
PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0

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

proto_functions_s* __stdcall handshake( host_functions_s *hf )
{
    if (ASSERT(lan_engine::get(false) == nullptr))
    {
        if (sodium_init() == -1)
        {
        }

        lan_engine::create( hf );
    }

    return &funcs;
}



