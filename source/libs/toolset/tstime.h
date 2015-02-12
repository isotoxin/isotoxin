#pragma once

namespace ts
{
class Time
{
    DWORD value;
    static __declspec(thread) DWORD threadCurrentTime;

    Time();
    explicit Time(DWORD value) : value(value) {}
    void operator-(DWORD) const;//чтобы отловить все места с "t - timeGetTime()"

public:

    DWORD raw() const { return value; }

    static Time current()
    {
        return Time(threadCurrentTime ? threadCurrentTime : timeGetTime());
    }
    static Time undefined()
    {
        return Time(0);
    }
    static Time past()//время, которое гарантированно в прошлом
    {
        return current() - 500000000;
    }
    static Time updateForThread()//обновляет время для текущего потока и возвращает текущее время
    {
        if ((threadCurrentTime = timeGetTime()) == 0) threadCurrentTime = 1;//0 используется как спец. признак того, что для данного потока время не обновляется и нужно всегда использовать timeGetTime()
        return Time(threadCurrentTime);
    }

    Time operator+(int     delta) const { return Time(value + delta); }
    Time operator-(int     delta) const { return Time(value - delta); }
    int  operator-(const Time &t) const { return value - t.value; }
    bool operator==(const Time &t) const { return value == t.value; }
    bool operator< (const Time &t) const { return int(value - t.value) < 0; }
    bool operator> (const Time &t) const { return int(value - t.value) > 0; }
    bool operator<=(const Time &t) const { return int(value - t.value) <= 0; }
    bool operator>=(const Time &t) const { return int(value - t.value) >= 0; }
};


class timer_subscriber_c
{
    friend class timerprocessor_c;
    DECLARE_EYELET(timer_subscriber_c);


public:

    virtual ~timer_subscriber_c() {}

    virtual  void   added(void) {}
    virtual  void   unconnected(void) {}
    virtual  void   timeup(void * par) = 0;


};

struct timer_subscriber_entry_s
{
    MOVABLE(false);

    void *                      param;
    double                      ttl;
    iweak_ptr<timer_subscriber_c>  hook;

    timer_subscriber_entry_s(timer_subscriber_c *s, double _ttl, void *p) :hook(s), ttl(_ttl), param(p) {}
    ~timer_subscriber_entry_s()
    {
        if (hook)
            hook->unconnected();
    }
};

class timerprocessor_c
{

    array_del_t<timer_subscriber_entry_s, 32> m_items;
    array_del_t<timer_subscriber_entry_s, 32> m_items_process;
    array_del_t<timer_subscriber_entry_s, 32> m_items_free;

    timer_subscriber_entry_s *getfree( timer_subscriber_c *s, double _ttl, void *p )
    {
        if (m_items_free.size())
        {
            timer_subscriber_entry_s * itm = m_items_free.get_last_remove();
            itm->param = p;
            itm->ttl = _ttl;
            itm->hook = s;
            return itm;
        }
        return TSNEW( timer_subscriber_entry_s, s, _ttl, p );
    }

    void makefree( timer_subscriber_entry_s *itm )
    {
        m_items_free.add(itm);
    }

public:

    timerprocessor_c() {}
    ~timerprocessor_c() {}

    bool    takt(double dt); // return true if empty
    void    add(timer_subscriber_c *t, double ttl, void * par, bool delete_same = true);
    void    del(timer_subscriber_c *t, void * par);
    void    del(timer_subscriber_c *t);
    void    its_time(timer_subscriber_c *t, void * par);
    bool    remain(timer_subscriber_c *t, void * par, double& ttl);
    bool    present(timer_subscriber_c *t) const;
    bool    present(timer_subscriber_c *t, void * par) const;
    void    do_all(void);
    void    clear(void);

    template <typename RCV> void iterate(RCV &r)
    {
        int cnt = m_items.size();
        for (int i = 0; i < cnt; ++i)
        {
            timer_subscriber_entry_s *e = m_items.get(i);
            if (e->hook.object() == NULL) continue;
            if (!r(e->hook.object(), e->ttl, e->param)) break;
        }
    }

};


class message_poster_c
{
    struct data_s
    {
        HWND w;
        int msg;
        WPARAM wp;
        LPARAM lp;

        bool working;
        bool stoping;
        bool pause;

        data_s(void) :working(false), stoping(false), pause(false) {}
    };

    cs_lock_t<data_s> m_data;
    int m_pausems;

    static DWORD WINAPI worker(LPVOID lpParameter);
public:
    message_poster_c(int pausems = 50);
    ~message_poster_c();

    void start(HWND w, int msg, WPARAM wp, LPARAM lp);
    void stop(void);
    void pause(bool p);
};

class frame_time_c
{
    float fixedFrameTime;
    int currentFrame;//номер текущего кадра (увеличивается на каждый takt())
    double frameTime_;
    float  frameTimef_;

public:

    frame_time_c();

    void takt();
    void setFrameTime(float time) { frameTimef_ = time; }
    float frame_time() const { return frameTimef_; }

};

template<int period_ms> struct time_reducer_s
{
    float m_current_period;

    time_reducer_s(float initial_period = (double(period_ms)*(1.0 / 1000.0))) { m_current_period = initial_period; }

    static int  get_period(void) { return period_ms; }

    void reset(void)
    {
        float initial_period = (float)(double(period_ms)*(1.0 / 1000.0));
        m_current_period = initial_period;
    }

    bool takt(float dt)
    {
        m_current_period -= dt;
        return m_current_period < 0;
    }

    bool it_is_time_looped(void)
    {
        bool r = m_current_period < 0;
        if (r) m_current_period += (double(period_ms)*(1.0 / 1000.0));
        return r;
    }

    bool it_is_time_ones(void)
    {
        bool r = m_current_period < 0;
        if (r)
        {
            do {
                m_current_period += (float)(double(period_ms)*(1.0 / 1000.0));
            } while (m_current_period < 0);
        }
        return r;
    }

};

} // namespace ts

