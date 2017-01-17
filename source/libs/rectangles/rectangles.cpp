#include "rectangles.h"

ts::irect correct_rect_by_maxrect(const ts::irect & oldmaxrect, const ts::irect & newmaxrect, const ts::irect & rect, bool force_correct)
{
    if (force_correct || newmaxrect != oldmaxrect)
    {
        ts::ivec2 newc = rect.center() - oldmaxrect.center() + newmaxrect.center();
        ts::irect newr = ts::irect::from_center_and_size(newc, rect.size());
        newr.movein(newmaxrect);
        return newr;
    }
    return rect;
}
