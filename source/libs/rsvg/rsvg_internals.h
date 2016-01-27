#pragma once

struct rsvg_load_context_s
{
    ts::ivec2 size = ts::ivec2(0);
    rsvg_stroke_and_fill_s deffillstroke;
    ts::hashmap_t< ts::str_c, ts::shared_ptr<rsvg_filters_group_c> > filters;
    ts::astrings_c targets; // per filter results
};
