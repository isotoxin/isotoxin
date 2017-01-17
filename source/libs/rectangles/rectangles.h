/*
    Rectangles UI module
    (C) 2015 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once

#define _ALLOW_RTCc_IN_STL

#pragma warning (push)
#pragma warning (disable:4820)
#pragma warning (disable:4100)
#include <memory>
#include "fastdelegate/FastDelegate.h"
#pragma warning (pop)

#include <src/cairo.h>
#include "toolset/toolset.h"
#include "rsvg/rsvg.h"

#define MWHEELPIXELS 20

#include "menu.h"
#include "rtools.h"
#include "gmesages.h"
#include "theme.h"
#include "guirect.h"
#include "rectengine.h"
#include "gui.h"
#include "dialog.h"

enum mem_type_rectangles_e
{
    MEMT_GMES = MEMT_LAST + 1,
    MEMT_GUI_COMMON,
    MEMT_EVTSYSTEM,
    MEMT_TEXTRECT,
    MEMT_TOOLTIP,
    MEMT_TEXTFIELD,
    MEMT_CHREPOS,
    MEMT_CHSORT,
    //MEMT_REDRAW,
    MEMT_SETFOCUS,
    MEMT_DYNAMIC_TEXTURE,
    MEMT_MENU,

    MEMT_RECTANGLES_LAST
};

ts::irect correct_rect_by_maxrect(const ts::irect & oldmaxrect, const ts::irect & newmaxrect, const ts::irect & rect, bool force_correct);
