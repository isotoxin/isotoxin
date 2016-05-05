#pragma once

struct rsvg_load_context_s
{
    ts::ivec2 size = ts::ivec2(0);
    rsvg_stroke_and_fill_s deffillstroke;
    ts::hashmap_t< ts::str_c, ts::shared_ptr<rsvg_filters_group_c> > filters;
    ts::hashmap_t< ts::str_c, rsvg_animation_c * > anims;
    ts::astrings_c targets; // per filter results
    allinims_t allanims;

    struct junk_on_end_s
    {
        MOVABLE( true );
        rsvg_animation_c *a;
        ts::str_c id;
    };
    ts::array_inplace_t<junk_on_end_s, 2> joe;

    ~rsvg_load_context_s();
};
