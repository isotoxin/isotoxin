#include "toolset.h"
#include "internal/platform.h"


namespace ts
{
__declspec( thread ) DWORD Time::thread_current_time = 0;

Time Time::current()
{
    return Time( thread_current_time ? thread_current_time : timeGetTime() );
}

Time Time::update_thread_time() // update current thread time and return current time value
{
    if ( ( thread_current_time = timeGetTime() ) == 0 ) thread_current_time = 1; // 0 means direct call to timeGetTime(), so set 1
    return Time( thread_current_time );
}


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
    //RECURSIVE_ALERT(); recursive calls allowed. sys_idle can be called from timeup

    tmp_array_del_t<timer_subscriber_entry_s, 32> processing;

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
                processing.add(m_items.get_remove_fast(i));
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

    for (timer_subscriber_entry_s *e : processing)
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

    while (processing.size()) makefree(processing.get_last_remove());
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


int generate_time_string( wchar *s, int capacity, const wstr_c& tmpl, const tm& tt )
{
    /*
        d		Day of the month as digits without leading zeros for single-digit days.
        dd		Day of the month as digits with leading zeros for single-digit days.
        ddd		Abbreviated day of the week as specified by a LOCALE_SABBREVDAYNAME* value, for example, "Mon" in English (United States).
        		Windows Vista and later: If a short version of the day of the week is required, your application should use the LOCALE_SSHORTESTDAYNAME* constants.
        dddd	Day of the week as specified by a LOCALE_SDAYNAME* value.


        M		Month as digits without leading zeros for single-digit months.
        MM		Month as digits with leading zeros for single-digit months.
        MMM		Abbreviated month as specified by a LOCALE_SABBREVMONTHNAME* value, for example, "Nov" in English (United States).
        MMMM	Month as specified by a LOCALE_SMONTHNAME* value, for example, "November" for English (United States), and "Noviembre" for Spanish (Spain).

        y		Year represented only by the last digit.
        yy		Year represented only by the last two digits. A leading zero is added for single-digit years.
        yyyy	Year represented by a full four or five digits, depending on the calendar used. Thai Buddhist and Korean calendars have five-digit years.
                The "yyyy" pattern shows five digits for these two calendars, and four digits for all other supported calendars. Calendars that have single-digit or two-digit years,
                such as for the Japanese Emperor era, are represented differently. A single-digit year is represented with a leading zero, for example, "03".
                A two-digit year is represented with two digits, for example, "13". No additional leading zeros are displayed.
        yyyyy	Behaves identically to "yyyy".
    */

#ifdef _WIN32
    SYSTEMTIME st;
    st.wYear = ( ts::uint16 )( tt.tm_year + 1900 );
    st.wMonth = ( ts::uint16 )( tt.tm_mon + 1 );
    st.wDayOfWeek = ( ts::uint16 )tt.tm_wday;
    st.wDay = ( ts::uint16 )tt.tm_mday;
    st.wHour = ( ts::uint16 )tt.tm_hour;
    st.wMinute = ( ts::uint16 )tt.tm_min;
    st.wSecond = ( ts::uint16 )tt.tm_sec;
    st.wMilliseconds = 0;

    return GetDateFormatW( LOCALE_USER_DEFAULT, 0, &st, tmpl.cstr(), s, capacity ) - 1;
#endif // _WIN32

}

} // namespace ts