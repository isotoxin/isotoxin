#pragma once

class rsvg_animation_c
{
    uint duration = 1000;
    float invdur = 1.0f;
    int repcount = -1;
    ts::pointers_t<rsvg_animation_c, 0> start_on_end; // these animations will be started on current animation end
    ts::tbuf0_t<uint> start;
    int curc = -1;
    uint startt = 0;
    bool active = false;
protected:
    rsvg_node_c *patient;
    virtual void activate(uint st, bool f);
public:
    rsvg_animation_c(rsvg_node_c *patient):patient(patient) {}
    virtual ~rsvg_animation_c() {}

    static void build(rsvg_load_context_s & ctx, rsvg_node_c *rsvg, ts::rapidxml::xml_node<char> *n);

    void setup(rsvg_load_context_s & ctx, const ts::asptr&beg, const ts::asptr&dur, const ts::asptr&repc);
    virtual void set_from_to(const ts::asptr&from, const ts::asptr&to) = 0;
    void on_end_start(rsvg_animation_c *a) { start_on_end.add(a); }

    void tick(uint t0, uint t1);
    virtual void lerp(float t) = 0;
};

class rsvg_animation_transform_c : public rsvg_animation_c
{
protected:
    cairo_matrix_t m;
    /*virtual*/ void activate(uint st, bool f) override;
public:
    rsvg_animation_transform_c(rsvg_node_c *patient):rsvg_animation_c(patient)
    {
        cairo_matrix_init_identity(&m);
    }
    virtual ~rsvg_animation_transform_c() {}


    const cairo_matrix_t *get_tansform() const { return &m; }
};

class rsvg_animation_transform_rotate_c : public rsvg_animation_transform_c
{
    ts::vec3 dfrom = ts::vec3(0);
    ts::vec3 dto = ts::vec3(0);
public:
    rsvg_animation_transform_rotate_c(rsvg_node_c *patient):rsvg_animation_transform_c(patient) {}
    virtual ~rsvg_animation_transform_rotate_c() {}

    /*virtual*/ void set_from_to(const ts::asptr&from, const ts::asptr&to) override;
    /*virtual*/ void lerp(float t) override;
};
