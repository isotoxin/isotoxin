#include "_precompiled.h"
#include "rsvg_internals.h"

rsvg_load_context_s::~rsvg_load_context_s()
{
    for (auto &j : joe)
    {
        if (auto *a = anims.find(j.id))
        {
            a->value->on_end_start( j.a );
        }
    }
}

void rsvg_animation_c::build(rsvg_load_context_s & ctx, rsvg_node_c *rsvg, ts::rapidxml::xml_node<char> *n)
{
    ts::pstr_c tagname(n->name());
    if (tagname.equals(CONSTASTR("animateTransform")))
    {
        rsvg_animation_transform_c * atr = nullptr;
        ts::asptr from, to, beg, dur, repc, id;
        for (ts::rapidxml::attribute_iterator<char> ai(n), e; ai != e; ++ai)
        {
            ts::pstr_c attrname(ai->name());
            if (attrname.equals(CONSTASTR("type")))
            {
                if (ts::pstr_c(ai->value()).equals(CONSTASTR("rotate")))
                    atr = TSNEW(rsvg_animation_transform_rotate_c, rsvg);

            }
            else if (attrname.equals(CONSTASTR("from")))
            {
                from = ai->value();
            }
            else if (attrname.equals(CONSTASTR("to")))
            {
                to = ai->value();
            }
            else if (attrname.equals(CONSTASTR("begin")))
            {
                beg = ai->value();
            }
            else if (attrname.equals(CONSTASTR("dur")))
            {
                dur = ai->value();
            }
            else if (attrname.equals(CONSTASTR("repeatCount")))
            {
                repc = ai->value();
            }
            else if (attrname.equals(CONSTASTR("id")))
            {
                id = ai->value();
            }
        }
        if (atr)
        {
            ctx.anims.add(id) = atr;
            atr->set_from_to(from, to);
            atr->setup(ctx, beg, dur, repc);
            ctx.allanims.add(atr);
        }
    }
}

static uint parsetime(const ts::pstr_c &d)
{
    if (d.ends(CONSTASTR("ms")))
        return d.substr(0, d.get_length() - 2).as_uint();
    else if (d.ends(CONSTASTR("s")))
        return d.substr(0, d.get_length() - 1).as_uint() * 1000;
    return d.as_uint();
}

void rsvg_animation_c::setup(rsvg_load_context_s & ctx, const ts::asptr&beg, const ts::asptr&dur, const ts::asptr&repc)
{
    duration = parsetime( ts::pstr_c(dur) );
    invdur = duration ? (1.0f / (float)duration) : 1.0f;

    ts::pstr_c r(repc);
    if (r.equals(CONSTASTR("indefinite")))
        repcount = -1;
    else
        repcount = r.as_uint();

    for (ts::token<char> t(beg, ';'); t; ++t)
    {
        ts::pstr_c bv = t->get_trimmed();
        if (bv.ends(CONSTASTR(".end")))
        {
            auto &j = ctx.joe.add();
            j.a = this;
            j.id = bv.substr(0, bv.get_length() - 4);
        } else
            start.add() = parsetime(ts::pstr_c(bv));
    }
}

void rsvg_animation_c::activate(uint st, bool f)
{
    bool oa = active;

    curc = repcount;
    active = f;
    if (f) startt = st;

    if (!f && oa)
    {
        for (rsvg_animation_c *a : start_on_end)
            a->activate(st, true), a->lerp(0);
    }

}

/*virtual*/ void rsvg_animation_c::tick(uint t0, uint t1)
{
    if (t0 == 0 && t1 == 0)
    {
        lerp(0);
        active = false;
    }

    if (!active)
    {
        for (uint bt : start)
        {
            if (bt <= t1 && t1 <= bt + duration)
            {
                activate(bt, true);
                break;
            }
        }
    }

    if (active)
    {
        if ( t1 >= startt && t1 <= startt + duration)
        {
            lerp( (float)(t1 - startt) * invdur );
            return;
        }

        if (repcount < 0 || (--curc) > 0)
        {
            lerp((float)((t1 - startt) % duration) * invdur);

        } else
        {
            ASSERT( t1 > startt + duration );
            lerp(1.0f);
            activate(startt + duration, false);
        }
    }


}

/*virtual*/ void rsvg_animation_transform_c::activate(uint st, bool f)
{
    patient->set_transform_animation( f ? this : nullptr );
    super::activate(st, f);
}

/*virtual*/ void rsvg_animation_transform_rotate_c::set_from_to(const ts::asptr&from, const ts::asptr&to)
{
    int i = 0;
    for (ts::token<char> t(from, ' '); t; ++t)
        dfrom[i++] = t->as_float();

    i = 0;
    for (ts::token<char> t(to, ' '); t; ++t)
        dto[i++] = t->as_float();

    dfrom.x = deg2rad(dfrom.x);
    dto.x = deg2rad(dto.x);
}

void rsvg_animation_transform_rotate_c::lerp(float t)
{
    ts::vec3 v = ts::LERP( dfrom, dto, t );

    if (v.y == 0 && v.z == 0)
    {
        cairo_matrix_init_rotate(&m, v.x);
    } else
    {
        cairo_matrix_t affine;
        cairo_matrix_init_translate(&m, v.y, v.z);

        cairo_matrix_init_rotate(&affine, v.x);
        cairo_matrix_multiply(&m, &affine, &m);

        cairo_matrix_init_translate(&affine, -v.y, -v.z);
        cairo_matrix_multiply(&m, &affine, &m);
    }
}

