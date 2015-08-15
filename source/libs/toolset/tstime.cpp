#include "toolset.h"

namespace ts
{

void    timerprocessor_c::clear()
{
    while(m_items.size()) makefree( m_items.get_last_remove() );
}

void    timerprocessor_c::do_all()
{
    for (timer_subscriber_entry_s *e : m_items)
        if (timer_subscriber_c *t = e->hook.get())
        {
            e->hook.unconnect();
            void * par = e->param;
            t->timeup(par);
            continue;
        }
    clear();

}

float    timerprocessor_c::takt(double dt)
{
    RECURSIVE_ALERT();

    tmp_array_del_t<timer_subscriber_entry_s, 32> m_items_process;

    float nexttime = -1;
    aint cnt = m_items.size();
    for (aint i = 0; i < cnt;)
    {
        timer_subscriber_entry_s * e = m_items.get(i);
        if (timer_subscriber_c *t = e->hook.get())
        {
            e->ttl -= dt;
            if (e->ttl < 0)
            {
                m_items_process.add(m_items.get_remove_fast(i));
                --cnt;
                continue;
            } else
            {
                if (nexttime < 0 || e->ttl < nexttime)
                    nexttime = (float)e->ttl;
            }
        }
        ++i;
    }

    for (timer_subscriber_entry_s *e : m_items_process)
    {
        timer_subscriber_c *t = e->hook.get();
        e->hook.unconnect();
        void * par = e->param;
        if (t) t->timeup(par);
    }

    for (int i = 0; i < m_items.size();)
    {
        timer_subscriber_entry_s * e = m_items.get(i);
        if (e->hook.expired())
        {
            makefree( m_items.get_remove_fast(i) );
            continue;
        }
        ++i;
    }

    while (m_items_process.size()) makefree(m_items_process.get_last_remove());
    return nexttime;
}

void    timerprocessor_c::add(timer_subscriber_c *t, double ttl, void * par, bool delete_same)
{
    if (delete_same) del(t, par);
    timer_subscriber_entry_s *e = getfree(t, ttl, par);
    m_items.add(e);
    t->added();
}

void    timerprocessor_c::del(timer_subscriber_c *t, void * par)
{
    int cnt = m_items.size();
    for (int i = 0; i < cnt; ++i)
    {
        timer_subscriber_entry_s *at = m_items.get(i);
        if (at->param == par && t == at->hook.get())
        {
            makefree(m_items.get_remove_fast(i));
            return;
        }
    }
}

bool    timerprocessor_c::present(timer_subscriber_c *t, void * par) const
{
    for (timer_subscriber_entry_s *at : m_items)
        if (at->param == par && t == at->hook.get())
            return true;
    return false;
}

bool    timerprocessor_c::present(timer_subscriber_c *t) const
{
    for (timer_subscriber_entry_s *at : m_items)
        if (t == at->hook.get())
            return true;
    return false;
}

bool    timerprocessor_c::remain(timer_subscriber_c *t, void * par, double& ttl)
{
    for (timer_subscriber_entry_s *at : m_items)
        if (at->param == par && t == at->hook.get())
        {
            ttl = at->ttl;
            return true;
        }
    return false;
}

void    timerprocessor_c::del(timer_subscriber_c *t)
{
    for (aint i = 0; i < m_items.size();)
    {
        if (t == m_items.get(i)->hook.get())
        {
            makefree( m_items.get_remove_fast(i) );
            continue;
        }
        ++i;
    }
}

void    timerprocessor_c::its_time(timer_subscriber_c *t, void * par)
{
    int cnt = m_items.size();
    for (int i = 0; i < cnt; ++i)
    {
        timer_subscriber_entry_s *at = m_items.get(i);
        if (at->param == par && t == at->hook.get())
        {
            if (at->hook)
                at->hook->timeup(par);
            makefree( m_items.get_remove_fast(i) );
            return;
        }
    }
}

message_poster_c::message_poster_c(int pausems): m_pausems(pausems)
{

}
message_poster_c::~message_poster_c()
{
    stop();
}

DWORD WINAPI message_poster_c::worker(LPVOID lpParameter)
{
    message_poster_c *d = (message_poster_c *)lpParameter;
    d->m_data.lock_write()().working = true;

    for (;;)
    {
        auto r = d->m_data.lock_read();
        if (r().stoping)
        {
            break;
        }
        if (r().pause)
        {
            r.unlock();
            Sleep(0);
            continue;
        }
        //Beep(1000,10);
        //Beep(2000,10);

        HWND _w = r().w;
        int _m = r().msg;
        WPARAM _wp = r().wp;
        LPARAM _lp = r().lp;

        r.unlock();
        d->m_data.lock_write()().pause = true;
        PostMessageW(_w, _m, _wp, _lp);
        Sleep(d->m_pausems);
    }

    d->m_data.lock_write()().working = false;

    return 0;
}



void message_poster_c::start(HWND w, int msg, WPARAM wp, LPARAM lp)
{
    stop();

    auto wr = m_data.lock_write();
    wr().w = w;
    wr().msg = msg;
    wr().wp = wp;
    wr().lp = lp;
    wr().working = false;
    wr().stoping = false;
    wr().pause = false;
    wr.unlock();

    if (HANDLE t = CreateThread(nullptr, 0, worker, this, 0, nullptr))
    {
        CloseHandle(t);
        for (;;)
        {
            Sleep(0);
            if (!m_data.lock_read()().working) continue;
            break;
        }
    }

}
void message_poster_c::stop()
{
    if (!m_data.lock_read()().working) return;

    m_data.lock_write()().stoping = true;
    for (;;)
    {
        Sleep(0);
        if (m_data.lock_read()().working) continue;
        break;
    }
    m_data.lock_write()().stoping = false;
}

void message_poster_c::pause(bool p)
{
    m_data.lock_write()().pause = p;
}

namespace
{
template <class T> __forceinline const T sign(const T &x) { return x > 0 ? T(1) : (x < 0 ? T(-1) : 0); }

template <int M> class valfilter
{
    double dt[M];
    int current;
    double divergence;

public:
    valfilter() { memset(this, 0, sizeof(*this)); }

    double operator()(double indt, double p = 0.1)//p - чем меньше, тем слабее скачки входного dt будут влиять на выходной, но тем больше времени будет погашаться расхождение, вызванное скачком
    {
        dt[current] = indt;

        double outdt = dt[0];
        for (int i = 1; i < M; i++) outdt += dt[i];
        outdt /= M;//считаем среднее (если бы не было коррекции накопленной ошибки, это и было бы возвращенным значением)

        //		divergence += indt - outdt;//считаем величину расхождения

        double correction = min(outdt * p, fabs(divergence)) * sign(divergence);//считаем на сколько нужно скорректировать фильтрованный dt, чтобы за большой промежуток времени суммы реальных и фильтрованных dt не сильно отличались
        dt[current] += correction;//обновляем массив, чтобы потом среднее dt считалось с учетом коррекции - так divergence будет скорее сокращаться
        outdt += correction / M;//пересчитываем среднее ..
        divergence -= correction / M;//.. и величину расхождения на основе обновленного в массиве значения dt[current]

        current = (current + 1) % M;
        return outdt;
    }
};

static double invFreq;

inline double TimeDeltaToSec(UINT64 delta)
{
    return (double)delta * invFreq;
}


INLINE UINT64 GetTime()
{
    LARGE_INTEGER s;
    QueryPerformanceCounter(&s);
    return s.QuadPart;
}

template<class T, class T1, class T2>
inline T clamp(const T& x, const T1& xmin, const T2& xmax)
{
    if (x < xmin)
        return xmin;
    if (x > xmax)
        return xmax;
    return x;
}

}

frame_time_c::frame_time_c()
{
    m_fixed_frame_time = 0;
    m_current_frame_num = 0;
    m_frametime_d = 0;
    m_frametime_f = 0;
    LARGE_INTEGER s;
    QueryPerformanceFrequency(&s);
    invFreq = 1.0 / (double)s.QuadPart;
}

void frame_time_c::takt()
{
    static UINT64 prevTime = GetTime();
    UINT64 curTime = GetTime();
    static valfilter<4> dtf; // 12 выбрано как НОК 2,3 и 4 - провалы fps с такими периодами будут полностью погашены фильтром; также при p = 0.1, от любого скачка fps останется лишь 0.1/12*100%
    double ft = TimeDeltaToSec(curTime - prevTime);
    if (m_fixed_frame_time == 0) m_frametime_d = dtf(clamp(ft, 0.00000001, .25));
    else
    {
        while (ft < m_fixed_frame_time) ft = TimeDeltaToSec((curTime = GetTime()) - prevTime);
        m_frametime_d = m_fixed_frame_time;
    }
    m_frametime_f = (float)m_frametime_d;
    prevTime = curTime;
    m_current_frame_num++;
}

} // namespace ts