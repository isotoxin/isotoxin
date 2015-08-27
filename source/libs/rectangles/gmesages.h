#pragma once

enum gmsg_e
{
    GM_KILLPOPUPMENU_LEVEL,
    GM_POPUPMENU_DIED,
    GM_TOOLTIP_PRESENT,
    GM_KILL_TOOLTIPS,
    GM_GROUP_SIGNAL,
    GM_DIALOG_PRESENT,
    GM_CLOSE_DIALOG,
    GM_ROOT_FOCUS,
    GM_HEARTBEAT,
    GM_UI_EVENT,
    GM_DROPFILES,
    GM_DRAGNDROP, // drag'n'drop object drag or drop


    GM_COUNT
};

#define GM_RECEIVER( parent, ev ) struct UNIQIDLINE( ev##clazz ) : public gm_receiver_c \
{ \
    UNIQIDLINE( ev##clazz )():gm_receiver_c(ev) {} \
    ~ UNIQIDLINE( ev##clazz )() { unsubscribe(ev); } \
    virtual ts::uint32 event_happens( gmsgbase & __param ) override \
    { \
        parent *p = (parent *)( ((::byte *)this) - offsetof( parent, UNIQIDLINE( m_##ev ) ) ); \
        return p->gm_handler( (gmsg<ev> &)__param ); \
    } \
} UNIQIDLINE( m_##ev ); \
	ts::uint32 gm_handler( gmsg<ev> & p )


#define GMRBIT_ABORT     (1<<0)
#define GMRBIT_ACCEPTED  (1<<1)
#define GMRBIT_FAIL      (1<<2)
#define GMRBIT_CALLAGAIN (1<<3)

struct gmsgbase { int m; int pass = 0; gmsgbase(int m) :m(m) {} virtual ~gmsgbase() {} ts::flags32_s send(); void send_to_main_thread(); };
template<int mid> struct gmsg : public gmsgbase { gmsg() :gmsgbase(mid) {} };
template<> struct gmsg<GM_KILLPOPUPMENU_LEVEL> : public gmsgbase
{
    gmsg(int level) :gmsgbase(GM_KILLPOPUPMENU_LEVEL), level(level) {}
    int level;
};
template<> struct gmsg<GM_POPUPMENU_DIED> : public gmsgbase
{
    gmsg(int level) :gmsgbase(GM_POPUPMENU_DIED), level(level) {}
    int level;
};

template<> struct gmsg<GM_TOOLTIP_PRESENT> : public gmsgbase
{
    gmsg(RID rid) :gmsgbase(GM_TOOLTIP_PRESENT), rid(rid) {}
    RID rid;
};

template<> struct gmsg<GM_ROOT_FOCUS> : public gmsgbase
{
    gmsg(RID rootrid) :gmsgbase(GM_ROOT_FOCUS),rootrid(rootrid) {}
    RID rootrid;
    RID activefocus;
};

template<> struct gmsg<GM_GROUP_SIGNAL> : public gmsgbase
{
    gmsg(RID sender, int tag) :gmsgbase(GM_GROUP_SIGNAL), sender(sender), tag(tag) {}
    RID sender;
    int tag;
    ts::uint32 mask = 0;
};

template<> struct gmsg<GM_DIALOG_PRESENT> : public gmsgbase
{
    gmsg(int udt) :gmsgbase(GM_DIALOG_PRESENT), unique_tag(udt) {}
    int unique_tag;
};

template<> struct gmsg<GM_CLOSE_DIALOG> : public gmsgbase
{
    gmsg(int udt) :gmsgbase(GM_CLOSE_DIALOG), unique_tag(udt) {}
    int unique_tag;
};

template<> struct gmsg<GM_DROPFILES> : public gmsgbase
{
    gmsg(RID root, const ts::wstr_c&fn, const ts::ivec2 &p) :gmsgbase(GM_DROPFILES), root(root), p(p), fn(fn) {}
    RID root;
    ts::ivec2 p;
    ts::wstr_c fn;
};

class gm_receiver_c
{
    gm_receiver_c *_gmer_next;
    gm_receiver_c *_gmer_prev;

public:

    static ts::uint32 notify_receivers(int ev, gmsgbase &par);


    gm_receiver_c(int ev);
    virtual void unsubscribe(int ev); // must be called by inherit class
    virtual ts::uint32 event_happens(gmsgbase & param) = 0;

};

template<gmsg_e mid> struct gm_redirect_s : public gm_receiver_c
{
    typedef fastdelegate::FastDelegate<bool (gmsg<mid> &)> HANDLER;
    HANDLER handler;
    gm_redirect_s(HANDLER h) :gm_receiver_c(mid), handler(h) {}
    ~gm_redirect_s() { unsubscribe(mid);}
    /*virtual*/ ts::uint32 event_happens(gmsgbase & param) override
    {
        return handler((gmsg<mid> &)param) ? GMRBIT_ACCEPTED : 0;
    }
};
