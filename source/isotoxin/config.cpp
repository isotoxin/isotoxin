#include "isotoxin.h"

ts::static_setup<config_c,1000> cfg;

config_base_c::~config_base_c()
{
}

void config_base_c::close()
{
    onclose();
    gui->delete_event(DELEGATE(this, save_dirty));
    save_dirty(RID(), as_param(1));
    closed = true;
    if (db)
    {
        db->close();
        db = nullptr;
    }
}

ts::uint32 config_base_c::gm_handler(gmsg<ISOGM_ON_EXIT> &)
{
    close();
    return 0;
}

void config_base_c::prepare_conf_table( ts::sqlitedb_c *db )
{
    if (ASSERT(db)/* && !db->is_table_exist(CONSTASTR("conf"))*/)
    {
        ts::column_desc_s cfgcols[2];
        cfgcols[0].name_ = CONSTASTR("name");
        cfgcols[0].options.set( ts::column_desc_s::f_primary );
        cfgcols[0].type_ = ts::data_type_e::t_str;

        cfgcols[1].name_ = CONSTASTR("value");
        cfgcols[1].type_ = ts::data_type_e::t_str;

        db->update_table_struct(CONSTASTR("conf"), ARRAY_WRAPPER(cfgcols), true);
    }

}
bool config_base_c::save_dirty(RID, GUIPARAM save_all_now)
{
    MEMT( MEMT_CONFIG );

    ts::db_transaction_c __transaction(db);

    bool some_data_still_not_saved = save();
    for(;save_all_now && some_data_still_not_saved; some_data_still_not_saved = save()) ;
    if (some_data_still_not_saved) DEFERRED_UNIQUE_CALL( 1.0, DELEGATE(this, save_dirty), nullptr );
    return true;
}
bool config_base_c::save()
{
    ts::db_transaction_c __transaction(db);

    if (db && dirty.size())
    {
        ts::str_c vn = dirty.last();
        ts::data_pair_s d[2];
        d[0].name = CONSTASTR("name");
        d[0].text = vn;
        d[0].type_ = ts::data_type_e::t_str;

        d[1].name = CONSTASTR("value");
        d[1].text = values[vn];
        d[1].type_ = ts::data_type_e::t_str;

        db->insert(CONSTASTR("conf"), ARRAY_WRAPPER(d));

        dirty.truncate(dirty.size() - 1);
        while (dirty.remove_fast(vn));
    }
    return dirty.size() != 0;
}

bool config_base_c::param( const ts::asptr& pn, const ts::asptr& vl )
{
    if (closed) return false;
    bool added = false;
    auto &v = values.add_get_item(pn, added);
    if (added || !v.value.equals(vl))
    {
        v.value = vl;
        dirty.add(v.key);
        changed();
        return true;
    }
    return false;
}

void config_base_c::changed(bool save_all_now)
{
    DEFERRED_UNIQUE_CALL(1.0, DELEGATE(this, save_dirty), save_all_now ? 1 : 0);
}

extern parsed_command_line_s g_commandline;
bool find_config(ts::wstr_c &path)
{
    bool local_write_protected = false;
    ts::wstr_c exepath = ts::get_exe_full_name();
    if (g_commandline.alternative_config_path.is_empty())
    {
        if (ts::check_write_access(ts::fn_get_path(exepath)))
        {
            // ignore write protected path
            // never never never store config in program files
            path = ts::fn_change_name_ext(exepath, CONSTWSTR("config.db"));
            if (ts::is_file_exists(path)) return true;

#ifdef _DEBUG
            if (exepath.find_pos(CONSTWSTR("Program Files")) < 0)
                return false;
#endif
        }
        else
            local_write_protected = g_app != nullptr;

        path = ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%APPDATA%\\Isotoxin\\config.db")), FNO_FULLPATH | FNO_PARSENENV);
    } else
        path = ts::fn_fix_path(ts::fn_join(g_commandline.alternative_config_path, CONSTWSTR("config.db")), FNO_FULLPATH | FNO_PARSENENV);

    bool prsnt = ts::is_file_exists(path);
    if (!prsnt)
    {
        if (local_write_protected)
        {
            path = ts::fn_change_name_ext(exepath, CONSTWSTR("config.db"));
            if (ts::is_file_exists(path))
            {
                g_app->F_READONLY_MODE = true;
                if (ts::is_admin_mode())
                {
                    // super oops!
                    // write protected folder under elevated rights?
                    // readonly mode
                } else if (elevate())
                {
                    path.clear();
                    ts::master().sys_exit(0);
                    return false;
                } else
                {
                    ts::sys_sleep(500);
                }

                return true;
            }
        } 

        path.clear();
    }

    return prsnt;
}


void config_c::load( const ts::wstr_c &path_override )
{
    ASSERT(g_app);

    bool found_cfg = path_override.is_empty() ? find_config(path) : ts::is_file_exists(path_override);
    if (!path_override.is_empty())
        path = path_override;

    if (!found_cfg) 
    {
        if (!path_override.is_empty())
        {
            ERROR("Config MUST PRESENT");
            ts::master().sys_exit(0);
            return;
        }
        // no config :(
        // aha! 1st run!
        // show dialog
        g_app->load_theme(CONSTWSTR("def"));

        redraw_collector_s dch;
        SUMMON_DIALOG<dialog_firstrun_c>();

    } else
    {
        db = ts::sqlitedb_c::connect(path, nullptr, g_app->F_READONLY_MODE);
    }

    if (db)
    {
        if ( !db->read_table( CONSTASTR( "conf" ), get_cfg_reader() ) )
            prepare_conf_table( db );

        int bld = build();

        if (bld < 425)
        {
            REMOVE_CODE_REMINDER( 525 );

            ts::db_transaction_c __transaction(db);

            ts::data_pair_s idp;
            idp.name = CONSTASTR("name");
            idp.type_ = ts::data_type_e::t_str;

            idp.text = CONSTASTR("proxy");
            db->update( CONSTASTR("conf"), ts::array_wrapper_c<const ts::data_pair_s>(&idp, 1), CONSTASTR("name=\"autoupdate_proxy\""));

            idp.text = CONSTASTR("proxy_addr");
            db->update(CONSTASTR("conf"), ts::array_wrapper_c<const ts::data_pair_s>(&idp, 1), CONSTASTR("name=\"autoupdate_proxy_addr\""));

        }

        build(application_c::appbuild());

        gui->disable_special_border( ( misc_flags() & MISCF_DISABLEBORDER ) != 0 );
        g_app->F_SPLIT_UI = 0 != ( cfg().misc_flags() & MISCF_SPLIT_UI );

    }
}

bool config_base_c::cfg_reader( int row, ts::SQLITE_DATAGETTER getta )
{
    ts::data_value_s v;
    if (CHECK( ts::data_type_e::t_str == getta(0, v) ))
    {
        ts::str_c &vv = values[v.text];
        if (CHECK( ts::data_type_e::t_str == getta(1, v) ))
            vv = v.text;
    }
    return true;
}
