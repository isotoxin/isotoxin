#pragma once

namespace ts
{
class Time
{
    MOVABLE( true );

    uint32 value;
    static THREADLOCAL uint32 thread_current_time;

    Time();
    explicit Time( uint32 value) : value(value) {}
    void operator-( uint32 ) const; // avoid "t - timeGetTime()"

public:

    uint32 raw() const { return value; }

    static Time current();
    static Time undefined()
    {
        return Time(0);
    }
    static Time past() // time that is guaranteed is in past
    {
        return current() - 500000000;
    }
    static Time update_thread_time(); // update current thread time and return current time value

    Time &operator+=(int delta) { value += delta; return *this; }
    Time operator+(int     delta) const { return Time(value + delta); }
    Time operator-(int     delta) const { return Time(value - delta); }
    int  operator-(const Time &t) const { return value - t.value; }
    bool operator==(const Time &t) const { return value == t.value; }
    bool operator< (const Time &t) const { return int(value - t.value) < 0; }
    bool operator> (const Time &t) const { return int(value - t.value) > 0; }
    bool operator<=(const Time &t) const { return int(value - t.value) <= 0; }
    bool operator>=(const Time &t) const { return int(value - t.value) >= 0; }
};

int generate_time_string( wchar *s, int capacity, const wstr_c& tmpl, const tm& t );

class timer_subscriber_c
{
    friend class timerprocessor_c;
    DECLARE_EYELET(timer_subscriber_c);


public:

    virtual ~timer_subscriber_c() {}

    virtual  void   added() {}
    virtual  void   unconnected() {}
    virtual  void   timeup(void * par) = 0;


};

struct timer_subscriber_entry_s
{
    MOVABLE(false);

    double ttl;
    iweak_ptr<timer_subscriber_c> hook;
    void * param;

    timer_subscriber_entry_s(timer_subscriber_c *s, double _ttl, void *p ) :hook(s), ttl(_ttl), param(p) {}
    ~timer_subscriber_entry_s()
    {
        if (hook)
            hook->unconnected();
    }
};

class timerprocessor_c
{
    MOVABLE( false );

    enum
    {
        GRANULA = 16
    };

    pointers_t<timer_subscriber_entry_s, GRANULA> m_items;
    struct_buf_t<timer_subscriber_entry_s, GRANULA> m_pool;

public:

    timerprocessor_c() {}
    ~timerprocessor_c()
    {
        clear();
    }

    void cleanup();

    float   takt(double dt); // return next time event
    void    add(timer_subscriber_c *t, double ttl, void * par, bool delete_same = true);
    void    del(timer_subscriber_c *t, void * par);
    void    del(timer_subscriber_c *t);
    void    its_time(timer_subscriber_c *t, void * par);
    bool    remain(timer_subscriber_c *t, void * par, double& ttl);
    bool    present(timer_subscriber_c *t) const;
    bool    present(timer_subscriber_c *t, void * par) const;
    void    do_all();
    void    clear();

    template <typename RCV> void iterate(RCV &r)
    {
        int cnt = m_items.size();
        for (int i = 0; i < cnt; ++i)
        {
            timer_subscriber_entry_s *e = m_items.get(i);
            if (e->hook.expired()) continue;
            if (!r(e->hook.get(), e->ttl, e->param)) break;
        }
    }

};

class frame_time_c
{
    float m_fixed_frame_time;
    int m_current_frame_num; // frame num (increased every takt())
    double m_frametime_d;
    float  m_frametime_f;

public:

    frame_time_c();

    void takt();
    void set_frame_time(float time) { m_frametime_f = time; }
    float frame_time() const { return m_frametime_f; }

};

template<int period_ms> struct time_reducer_s
{
    Time m_next_reaction;

    time_reducer_s(int initial_period = period_ms):m_next_reaction( Time::current() + initial_period ) {}

    static int  get_period() { return period_ms; }

    void reset()
    {
        m_next_reaction = Time::current() + period_ms;
    }

    bool it_is_time_looped()
    {
        Time ct = Time::current();
        if ((ct - m_next_reaction) < 0) return false;
        m_next_reaction += period_ms;
        return true;
    }

    bool it_is_time_ones()
    {
        Time ct = Time::current();
        if ((ct - m_next_reaction) < 0) return false;
        m_next_reaction = ct + period_ms;
        return true;
    }

};

template<int dummynum> INLINE bool __tmbreak( int ms )
{
    static Time prevtimestamp = Time::undefined() + dummynum;
    static bool initialized = false;
    bool r = false;
    Time   timestamp = Time::current();
    if ( initialized )
    {
        if ( ( timestamp - prevtimestamp ) < ms )
        {
            r = true;
        }
        prevtimestamp = timestamp;
    }
    else
    {
        prevtimestamp = timestamp;
        initialized = true;
    }
    return r;
};


} // namespace ts

