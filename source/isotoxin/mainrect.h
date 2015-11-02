#pragma once
/*
  mainrect
*/

class mainrect_c : public gui_control_c
{
    ts::wstr_c name;
    /*virtual*/ void created() override;
    void onclosesave();

    GM_RECEIVER( mainrect_c, ISOGM_APPRISE );
    
public:
	mainrect_c(initial_rect_data_s &data);
	~mainrect_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    
        //sqhandler_i
	/*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

