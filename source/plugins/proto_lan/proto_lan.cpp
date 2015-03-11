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

#define SS2(x) #x
#define SS(x) SS2(x)

void __stdcall get_info(proto_info_s *info)
{
    if (info->protocol_name) strncpy_s( info->protocol_name, info->protocol_name_buflen, "lan", _TRUNCATE );
    if (info->description) strncpy_s( info->description, info->description_buflen, "Local Area Network protocol " SS(PLUGINVER), _TRUNCATE );
    info->max_avatar_size = 0;
    info->priority = 1000;
    info->features = PF_INVITE_NAME | PF_UNAUTHORIZED_CHAT | PF_AUDIO_CALLS;
    info->proxy_support = 0;
    info->max_friend_request_bytes = SIZE_MAX_FRIEND_REQUEST_BYTES;

    info->audio_fmt.sample_rate = AUDIO_SAMPLERATE;
    info->audio_fmt.channels = AUDIO_CHANNELS;
    info->audio_fmt.bits = AUDIO_BITS;

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



