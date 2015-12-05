#pragma once
/*
  mainrect
*/

class mainrect_c;
template<> struct MAKE_ROOT<mainrect_c> : public _PROOT(mainrect_c)
{
    ts::irect rect;
    MAKE_ROOT(const ts::irect &rect) : _PROOT(mainrect_c)(), rect(rect) { init(false); }
    ~MAKE_ROOT() {}
};


class mainrect_c : public gui_control_c
{
    ts::irect rrect = ts::irect(0);
    ts::irect mrect = ts::irect(0);
    ts::wstr_c name;
    int checktick = 0;
    /*virtual*/ void created() override;
    void onclosesave();
    bool saverectpos(RID,GUIPARAM);

    GM_RECEIVER( mainrect_c, ISOGM_APPRISE );
    GM_RECEIVER( mainrect_c, GM_HEARTBEAT );
    
public:
	mainrect_c(MAKE_ROOT<mainrect_c> &data);
	~mainrect_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    
        //sqhandler_i
	/*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

