#pragma once

#define _ALLOW_RTCc_IN_STL

#pragma warning (disable:4091) // 'typedef ' : ignored on left of '' when no variable is declared

#define APPNAME "Isotoxin"
#define APPNAME_CAPTION maketag_color<ts::wchar>(GET_THEME_VALUE(appname_color)).append(CONSTWSTR("<b>")).append(CONSTWSTR(APPNAME)).append(CONSTWSTR("</b></color>"))

#define USERAGENT "curl"

#define RESAMPLER_SPEEXFA 1
#define RESAMPLER_SRC 2

#define _RESAMPLER RESAMPLER_SPEEXFA

#if _RESAMPLER == RESAMPLER_SPEEXFA
#include "filter_audio/other/speex_resampler.h"
#elif _RESAMPLER == RESAMPLER_SRC
#include "libresample/src/samplerate.h"
#endif
#include "filter_audio/filter_audio.h"

#include "ipc/ipc.h"
#include "rectangles/rectangles.h"

#pragma push_macro("delete")
#pragma push_macro("FLAG")
#pragma push_macro("near")
#undef delete
#undef FLAG
#undef near
#include "s3/s3.h"
#include "hunspell/hunspell_io.h"
#include "hunspell/src/hunspell.hxx"
#pragma pop_macro("delete")
#pragma pop_macro("FLAG")
#pragma pop_macro("near")

#define STRTYPE(TCHARACTER) ts::pstr_t<TCHARACTER>
#define MAKESTRTYPE(TCHARACTER, s, l) STRTYPE(TCHARACTER)( ts::sptr<TCHARACTER>((s),(l)) )
#include "../plugins/plgcommon/plghost_interface.h"
#undef MAKESTRTYPE
#undef STRTYPE
#include "../plugins/plgcommon/common_types.h"

#if defined (_M_AMD64) || defined (WIN64) || defined (__LP64__)
#define LIBSUFFIX "64.lib"
#else
#define LIBSUFFIX ".lib"
#endif

#ifdef _FINAL
#define USELIB(ln) comment(lib, #ln LIBSUFFIX)
#elif defined _DEBUG_OPTIMIZED
#define USELIB(ln) comment(lib, #ln "do" LIBSUFFIX)
#else
#define USELIB(ln) comment(lib, #ln "d" LIBSUFFIX)
#endif

/*
INLINE ipcr & operator>>(ipcr &r, ts::astrings_c &s)
{
    int cnt = r.get<int>();
    for(int i=0;i<cnt;++i)
        s.add( r.getastr() );
    return r;
}
*/

#include "tools.h"
#include "mediasystem.h"
#include "videoprocessing.h"

#include "contacts.h"
#include "config.h"
#include "activeprotocol.h"
#include "profile.h"

// gui
#include "filterbar.h"
#include "contactlist.h"
#include "fullscreenvideo.h"
#include "conversation.h"
#include "images.h"
#include "emoticons.h"
#include "isodialog.h"
#include "mainrect.h"
#include "firstrun.h"
#include "addcontact.h"
#include "metacontact.h"
#include "contactprops.h"
#include "settings.h"
#include "simpledlg.h"
#include "avaselector.h"
#include "smileselector.h"
#include "prepareimage.h"
#include "colors.h"

#include "res/resource.h"
#include "application.h"

