#pragma once

class dialog_firstrun_c : public gui_isodialog_c
{
    bool refresh_current_page( RID, GUIPARAM );
    bool start( RID, GUIPARAM );
    bool prev_page( RID, GUIPARAM );
    bool next_page( RID, GUIPARAM );
    bool noob_or_father( RID, GUIPARAM );
    bool handler_0( RID, GUIPARAM );
    bool handler_1( RID, GUIPARAM );
    bool path_check_0( const ts::wstr_c & t );
    bool path_check_1( const ts::wstr_c & t );
    void select_lang( const ts::str_c& prm );
    int page = 0;
    RID selpath;
    RID profile;

    enum path_choice_e
    {
        PCH_PROGRAMFILES,
        PCH_HERE,
        PCH_CUSTOM,
        PCH_APPDATA,
        PCH_INSTALLPATH,
    };
    SLANGID deflng;
    ts::wstr_c copyto, profilename;
    bool developing = true;
    bool i_am_noob = true;
#ifdef _DEBUG
    path_choice_e choice0 = PCH_HERE;
#else
    path_choice_e choice0 = PCH_PROGRAMFILES;
#endif
    path_choice_e choice1 = PCH_APPDATA;

    void go2page(int page);
    ts::wstr_c path_by_choice(path_choice_e choice);

protected:
    /*virtual*/ void created() override;
    /*virtual*/ void on_close() override { sys_exit(0); }
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
public:
    dialog_firstrun_c(initial_rect_data_s &data);
    ~dialog_firstrun_c();

    /*virtual*/ ts::wstr_c get_name() const override;

    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(510,450); }
    
    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

