#pragma once

#include "toolset/toolset.h"

#define REMOVE_CODE_REMINDER(bn) ASSERT( application_c::appbuild() < bn, "Remove code reminder" )

#ifdef _WIN32
#define PLGHOSTNAME CONSTWSTR("plghost.exe")
#endif // _WIN32
#ifdef _NIX
#define PLGHOSTNAME CONSTWSTR("plghost")
#endif


template<typename STR> struct wraptranslate;
template<> struct wraptranslate<ts::wsptr> : public ts::wsptr
{
    wraptranslate(const ts::wsptr &t):ts::wsptr(t) {}
    wraptranslate<ts::wstr_c> operator / (const ts::wsptr&r) const;
};
template<> struct wraptranslate<ts::wstr_c> : public ts::wstr_c
{
    wraptranslate( const ts::wstr_c & t ):ts::wstr_c(t) {}
    wraptranslate &operator / (const ts::wsptr&r)
    {
        int ins = this->find_pos('$');
        if (CHECK(ins >=0))
            this->replace(ins,1, r);
        return *this;
    }
};
inline wraptranslate<ts::wstr_c> wraptranslate<ts::wsptr>::operator / (const ts::wsptr&r) const
{
    return wraptranslate<ts::wstr_c>(*this) / r;
}


#ifndef _FINAL
INLINE const wraptranslate<ts::wsptr> __translation(const ts::wsptr &txt)
{
    WARNING("Translation not found! Use rasp loc. (%s)", ts::to_str(txt).cstr());
    return wraptranslate<ts::wsptr>(txt);
}
#endif
const wraptranslate<ts::wsptr> __translation(const ts::wsptr &txt, int tag);

#define TTT( txt, ... ) __translation(CONSTWSTR(txt),__VA_ARGS__)

INLINE time_t now()
{
    time_t t;
    _time64(&t);
    return t;
}

enum mpd_e
{
    MPD_UNAME = SETBIT(0),
    MPD_USTATUS = SETBIT(1),
    MPD_NAME = SETBIT(2),
    MPD_MODULE = SETBIT(3),
    MPD_ID = SETBIT(4),
    MPD_STATE = SETBIT(5),
};

ts::wstr_c make_proto_desc(const ts::wstr_c&idname, int mask);

template<typename TCHARACTER> ts::str_t<TCHARACTER> maketag_mark(ts::TSCOLOR c)
{
    ts::str_t<TCHARACTER> s(CONSTSTR(TCHARACTER, "<mark=#"));
    s.append_as_hex(ts::RED(c))
        .append_as_hex(ts::GREEN(c))
        .append_as_hex(ts::BLUE(c))
        .append_as_hex(ts::ALPHA(c))
        .append_char('>');
    return s;
}


template<typename TCHARACTER> ts::str_t<TCHARACTER> maketag_color( ts::TSCOLOR c )
{
    ts::str_t<TCHARACTER> s( CONSTSTR(TCHARACTER,"<color=#") );
    s.append_as_hex(ts::RED(c)).append_as_hex(ts::GREEN(c)).append_as_hex(ts::BLUE(c));
    if (ts::ALPHA(c) != 255) s.append_as_hex(ts::ALPHA(c));
    s.append_char('>');
    return s;
}

template<typename TCHARACTER> ts::str_t<TCHARACTER> colorize( const ts::sptr<TCHARACTER> &t, ts::TSCOLOR c)
{
    return maketag_color<TCHARACTER>(c).append(t).append(CONSTSTR(TCHARACTER,"</color>"));
}


template<typename TCHARACTER> ts::str_t<TCHARACTER> maketag_outline(ts::TSCOLOR c)
{
    ts::str_t<TCHARACTER> s(CONSTSTR(TCHARACTER, "<outline=#"));
    s.append_as_hex(ts::RED(c)).append_as_hex(ts::GREEN(c)).append_as_hex(ts::BLUE(c));
    if (ts::ALPHA(c) != 255) s.append_as_hex(ts::ALPHA(c));
    s.append_char('>');
    return s;
}
template<typename TCHARACTER> ts::str_t<TCHARACTER> maketag_shadow(ts::TSCOLOR c, int len = 2)
{
    ts::str_t<TCHARACTER> s(CONSTSTR(TCHARACTER, "<shadow="));
    s.append_as_int(len).append(CONSTSTR(TCHARACTER, ";#"))
        .append_as_hex(ts::RED(c))
        .append_as_hex(ts::GREEN(c))
        .append_as_hex(ts::BLUE(c))
        .append_as_hex(ts::ALPHA(c))
        .append_char('>');
    return s;
}

INLINE ts::wstr_c text_seconds(int seconds)
{
    int m = seconds / 60;
    int s = seconds - (m * 60);

    ts::wstr_c t( (m < 10) ? CONSTWSTR(" (0") : CONSTWSTR(" (") );

    t.append_as_uint(m).append_char(':');
    if (s < 10) t.append(CONSTWSTR("0"));
    t.append_as_uint(s);
    t.append_char(')');

    return t;
}

template<typename SCORE> void text_set_date(ts::str_t<ts::wchar, SCORE> & tstr, const ts::wstr_c &fmt, const tm &tt)
{
    if (tstr.get_capacity() > 32)
        tstr.set_length(tstr.get_capacity() - 1);
    else
        tstr.set_length(64);
    tstr.set_length( ts::generate_time_string(tstr.str(), tstr.get_capacity() - 1, fmt, tt) );
    
}

template <typename TCH> bool text_find_link(const ts::sptr<TCH> &message, int from, ts::ivec2 & rslt);

void text_convert_from_bbcode(ts::str_c &text_utf8);
void text_convert_to_bbcode(ts::str_c &text_utf8);
void text_close_bbcode(ts::str_c &text_utf8);
void text_convert_char_tags(ts::str_c &text_utf8);
void text_adapt_user_input(ts::str_c &text_utf8); // before print
void text_prepare_for_edit(ts::str_c &text_utf8);
INLINE ts::wstr_c enquote(const ts::wstr_c &x)
{
    return ts::wstr_c(CONSTWSTR("\""), x).append(CONSTWSTR("\""));
}

bool text_find_inds( const ts::wstr_c &t, ts::tmp_tbuf_t<ts::ivec2> &marks, const ts::wstrings_c &fsplit );
INLINE ts::pwstr_c text_non_letters() { return ts::pwstr_c(CONSTWSTR("?!:;*&^%$#@()<>[]{}=+.,~|/\r\n\t\\\"—«»„“”0123456789 _")); }

void render_pixel_text( ts::bitmap_c&tgt, const ts::irect& r, const ts::wsptr& t, ts::TSCOLOR bgcol, ts::TSCOLOR col );
void gen_identicon( ts::bitmap_c&tgt, const unsigned char *random_bytes /* at least 16 bytes, md5 will be ok */ );

typedef ts::sstr_t<4> SLANGID;
typedef ts::array_inplace_t<SLANGID, 0> SLANGIDS;

SLANGID detect_language();
menu_c list_langs( SLANGID curlng, MENUHANDLER h );

bool new_version();
bool new_version( const ts::asptr &current, const ts::asptr &newver, bool same_version = false );

bool file_mask_match( const ts::wsptr &filename, const ts::wsptr &masks );

class sound_capture_handler_c
{
protected:

    s3::Format capturefmt;
    bool capture = false;

    sound_capture_handler_c();
    ~sound_capture_handler_c();

public:
    s3::Format &getfmt() { return capturefmt; }
    bool is_capture() const { return capture; };
    virtual void datahandler(const void *data, int size) = 0;
    virtual const s3::Format *formats( ts::aint &count) { count = 0; return nullptr; };

    void start_capture();
    void stop_capture();

};

struct parsed_command_line_s
{
    ts::swstr_t<MAX_PATH_LENGTH + 16> alternative_config_path;
    bool checkinstance = true;
    bool minimize = false;
    bool readonlymode = false;
};


// isotoxin gmsgs


enum isogmsg_e
{
    ISOGM_SEND_MESSAGE = GM_COUNT,  // signal to send message
    ISOGM_MESSAGE,                  // message itself // MULTIPASS!!!
    ISOGM_PROFILE_TABLE_LOADED,
    ISOGM_PROFILE_TABLE_SAVED,
    ISOGM_UPDATE_CONTACT,           // new or update contact - signal from protocol
    ISOGM_V_UPDATE_CONTACT,         // visual update (add to list, or update caption)
    ISOGM_SELECT_CONTACT,           // nullptr means total contacts unload
    ISOGM_CMD_RESULT,
    ISOGM_PROTO_LOADED,
    ISOGM_INCOMING_MESSAGE,
    ISOGM_DELIVERED,
    ISOGM_FILE,
    ISOGM_CHANGED_SETTINGS,
    ISOGM_NOTICE,
    ISOGM_NOTICE_PRESENT,
    ISOGM_SUMMON_POST,              // summon post_s into conversation
    ISOGM_CALL_STOPED,
    ISOGM_METACREATE,
    ISOGM_APPRISE,
    ISOGM_NEWVERSION,
    ISOGM_DOWNLOADPROGRESS,
    ISOGM_AVATAR,
    ISOGM_DO_POSTEFFECT,
    ISOGM_TYPING,
    ISOGM_REFRESH_SEARCH_RESULT,
    ISOGM_VIDEO_TICK,
    ISOGM_CAMERA_TICK,
    ISOGM_PEER_STREAM_OPTIONS,
    ISOGM_EXPORT_PROTO_DATA,
    ISOGM_UPDATE_MESSAGE_NOTIFICATION,
    ISOGM_SUMMON_NOPROFILE_UI,
    ISOGM_GRABDESKTOPEVENT,

    ISOGM_ON_EXIT,

    ISOGM_COUNT,
};

template<> struct gmsg<ISOGM_NEWVERSION> : public gmsgbase
{
    enum error_e
    {
        E_OK,
        E_OK_FORCE,
        E_NETWORK,
        E_DISK,
    };

    gmsg( ts::asptr ver, error_e en, bool version64 ) :gmsgbase(ISOGM_NEWVERSION), ver(ver), error_num(en), version64( version64 ){}
    ts::sstr_t<-16> ver;
    error_e error_num = E_OK;
    bool version64 = false;
    bool is_ok() const
    {
        return E_OK == error_num || E_OK_FORCE == error_num;
    }
};

class incoming_msg_panel_c;
template<> struct gmsg<ISOGM_UPDATE_MESSAGE_NOTIFICATION> : public gmsgbase
{
    gmsg() :gmsgbase( ISOGM_UPDATE_MESSAGE_NOTIFICATION ) {}
    ts::array_safe_t< incoming_msg_panel_c, 1 > panels;
};

template<> struct gmsg<ISOGM_EXPORT_PROTO_DATA> : public gmsgbase
{
    gmsg(int id) :gmsgbase(ISOGM_EXPORT_PROTO_DATA), protoid(id) {}
    int protoid;
    ts::blob_c buf;
};

template<> struct gmsg<ISOGM_DOWNLOADPROGRESS> : public gmsgbase
{
    gmsg(int id, int d, int t) :gmsgbase(ISOGM_DOWNLOADPROGRESS), id(id), downloaded(d), total(t) {}
    int id; // -1 - downloading new version
    int downloaded;
    int total;
};

enum profile_table_e;
template<> struct gmsg<ISOGM_PROFILE_TABLE_LOADED> : public gmsgbase
{
    gmsg(profile_table_e tabi) :gmsgbase(ISOGM_PROFILE_TABLE_LOADED),tabi(tabi) {}
    profile_table_e tabi;
};
template<> struct gmsg<ISOGM_PROFILE_TABLE_SAVED> : public gmsgbase
{
    gmsg(profile_table_e tabi) :gmsgbase(ISOGM_PROFILE_TABLE_SAVED), tabi(tabi) {}
    profile_table_e tabi;
};

template<> struct gmsg<ISOGM_DELIVERED> : public gmsgbase
{
    gmsg(uint64 utag) :gmsgbase(ISOGM_DELIVERED), utag(utag) {}
    uint64 utag;
};

template<> struct gmsg<ISOGM_CMD_RESULT> : public gmsgbase
{
    gmsg(int networkid, commands_e cmd, cmd_result_e rslt) :gmsgbase(ISOGM_CMD_RESULT), networkid(networkid), cmd(cmd), rslt(rslt) {}
    int networkid;
    commands_e cmd;
    cmd_result_e rslt;
};


template<> struct gmsg<ISOGM_PROTO_LOADED> : public gmsgbase
{
    gmsg(int id) :gmsgbase(ISOGM_PROTO_LOADED), id(id)
    {
    }
    int id;
};

struct post_s;
class contact_c;
class contact_root_c;
template<> struct gmsg<ISOGM_SUMMON_POST> : public gmsgbase
{
    gmsg(const post_s &post, contact_root_c *historian) :gmsgbase(ISOGM_SUMMON_POST), post(post), historian(historian), replace_post(false)
    {
    }
    gmsg(const post_s &post, bool replace_post) :gmsgbase(ISOGM_SUMMON_POST), post(post), replace_post(replace_post)
    {
    }
    contact_root_c *historian = nullptr;
    rectengine_c *created = nullptr;
    const post_s &post;
    
    uint64 prev_found = 0;
    uint64 next_found = 0;

    bool replace_post = false;
    bool found_item = false;
    bool down = true; // false - insert new post at begin

    void operator =(gmsg &) UNUSED;
};

template<> struct gmsg<ISOGM_DO_POSTEFFECT> : public gmsgbase
{
    gmsg(ts::uint32 bits) :gmsgbase(ISOGM_DO_POSTEFFECT), bits(bits) {}
    ts::uint32 bits;

};

template<> struct gmsg<ISOGM_VIDEO_TICK> : public gmsgbase
{
    gmsg(const ts::ivec2 &sz) :gmsgbase(ISOGM_VIDEO_TICK), videosize(sz) {}
    ts::ivec2 videosize;
};

template<> struct gmsg<ISOGM_CAMERA_TICK> : public gmsgbase
{
    gmsg(contact_root_c *collocutor) :gmsgbase(ISOGM_CAMERA_TICK), collocutor(collocutor) {}
    contact_root_c *collocutor;
};

enum settingsparam_e
{
    // profile
    PP_USERNAME,
    PP_USERSTATUSMSG,
    PP_GENDER,
    PP_ONLINESTATUS,
    PP_AVATAR,
    PP_PROFILEOPTIONS,
    PP_NETWORKNAME,
    PP_EMOJISET,
    PP_ACTIVEPROTO_SORT,
    PP_FONTSCALE,
    PP_VIDEO_ENCODING_SETTINGS,

    // config
    CFG_MICVOLUME,
    CFG_TALKVOLUME,
    CFG_DSPFLAGS,
    CFG_LANGUAGE,
    CFG_DEBUG,
};
template<> struct gmsg<ISOGM_CHANGED_SETTINGS> : public gmsgbase
{
    gmsg(int protoid, settingsparam_e sp, const ts::str_c &s) :gmsgbase(ISOGM_CHANGED_SETTINGS), protoid(protoid), sp(sp), s(s) {}
    gmsg(int protoid, settingsparam_e sp, int bits = 0) :gmsgbase(ISOGM_CHANGED_SETTINGS), protoid(protoid), sp(sp), bits(bits) {}
    int protoid;
    settingsparam_e sp;
    ts::str_c s;
    int bits = 0;
};
//

bool check_profile_name(const ts::wstr_c &, bool );

bool check_netaddr( const ts::asptr & );

void path_expand_env(ts::wstr_c &path, const ts::wstr_c &contactid);

void install_to(const ts::wstr_c &path, bool acquire_admin_if_need);
bool elevate();

enum loctext_e
{
    loc_connection_failed,
    loc_autostart,
    loc_please_create_profile,
    loc_please_authorize,
    loc_yes,
    loc_no,
    loc_exit,
    loc_network,
    loc_nonetwork,
    loc_networks,
    loc_disk_write_error,
    loc_import_from_file,
    loc_loading,
    loc_anyfiles,
    loc_camerabusy,
    loc_initialization,
    loc_copy,
    loc_image,
    loc_images,
    loc_space2takeimage,
    loc_dropimagehere,
    loc_loadimagefromfile,
    loc_pasteimagefromclipboard,
    loc_capturecamera,
    loc_qrcode,

    loc_connection_name,
    loc_module,
    loc_state,
};

ts::wstr_c loc_text(loctext_e);

ts::wstr_c text_sizebytes( ts::aint sz );
ts::wstr_c text_contact_state( ts::TSCOLOR color_online, ts::TSCOLOR color_offline, contact_state_e st, int link = -1 );
ts::wstr_c text_typing(const ts::wstr_c &prev, ts::wstr_c &workbuf, const ts::wsptr &preffix);

void draw_initialization( rectengine_c *e, ts::bitmap_c &pab, const ts::irect&viewrect, ts::TSCOLOR tcol, const ts::wsptr &additiontext );
void draw_chessboard(rectengine_c &e, const ts::irect & r, ts::TSCOLOR c1, ts::TSCOLOR c2);

void add_status_items(menu_c &m);

enum icon_type_e
{
    IT_NORMAL,
    IT_ONLINE,
    IT_OFFLINE,
    IT_AWAY,
    IT_DND,
    IT_UNKNOWN,
    IT_WHITE,
};

const ts::bitmap_c &prepare_proto_icon( const ts::asptr &prototag, const ts::asptr &icond, int imgsize, icon_type_e icot );

struct data_block_s
{
    const void *data;
    ts::aint datasize;
    data_block_s(const void *data, ts::aint datasize) :data(data), datasize(datasize) {}
};

struct ipcw : public ts::tmp_buf_c
{
    ipcw( commands_e cmd )
    {
        tappend<data_header_s>( data_header_s(cmd) );
    }

    template<typename T> ipcw & operator<<(const T &v) { TS_STATIC_CHECK(std::is_pod<T>::value, "T is not pod"); tappend<T>(v); return *this; }
    template<> ipcw & operator<<(const data_block_s &d) { tappend<uint>((uint)d.datasize); append_buf(d.data,d.datasize); return *this; }
    template<> ipcw & operator<<(const ts::asptr &s) { tappend<unsigned short>((unsigned short)s.l); append_buf(s.s,s.l); return *this; }
    template<> ipcw & operator<<(const ts::wsptr &s) { tappend<unsigned short>((unsigned short)s.l); append_buf(s.s,s.l*sizeof(ts::wchar)); return *this; }
    template<> ipcw & operator<<(const ts::str_c &s) { tappend<unsigned short>((unsigned short)s.get_length()); append_buf(s.cstr(), s.get_length()); return *this; }
    template<> ipcw & operator<<(const ts::wstr_c &s) { tappend<unsigned short>((unsigned short)s.get_length()); append_buf(s.cstr(), s.get_length()*sizeof(ts::wchar)); return *this; }
    template<> ipcw & operator<<(const ts::blob_c &b) { tappend<int>((int)b.size()); append_buf(b); return *this; }
    template<> ipcw & operator<<( const ts::tmp_buf_c &b ) { tappend<int>( (int)b.size() ); append_buf( b ); return *this; }
};

struct isotoxin_ipc_s
{
    typedef fastdelegate::FastDelegate< bool( ipcr )  > datahandler_func;
    typedef fastdelegate::FastDelegate< bool () > idlejob_func;

    ts::process_handle_s plughost;
    ipc::ipc_junction_s junct;
    ts::str_c tag;
    int version = 0;
    bool ipc_ok = false;
    isotoxin_ipc_s(const ts::str_c &tag, datahandler_func);
    ~isotoxin_ipc_s();

    datahandler_func datahandler;

    void wait_loop( idlejob_func idlefunc );
    void something_happens() { junct.idlejob(); }

    static ipc::ipc_result_e handshake_func( void *, void *, int );
    static ipc::ipc_result_e processor_func( void *, void *, int );
    static ipc::ipc_result_e wait_func( void * );
    void send( const ipcw &data )
    {
        junct.send(data.data(), (int)data.size());
    }

};

// leeches

struct leech_fill_parent_s : public autoparam_i
{
    leech_fill_parent_s() {}
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_fill_parent_rect_s : public autoparam_i
{
    ts::irect mrect;
    leech_fill_parent_rect_s(const ts::irect &mrect):mrect(mrect) {}
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_left_s : public autoparam_i
{
    int width;
    leech_dock_left_s(int width) :width(width) {}
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_top_s : public autoparam_i
{
    int height;
    leech_dock_top_s(int height) :height(height) {}
    void update_ctl_pos();
    /*virtual*/ bool i_leeched(guirect_c &to) override { if (__super::i_leeched(to)) { update_ctl_pos(); return true;} return false; };
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_bottom_center_s : public autoparam_i
{
    int width;
    int height;
    int x_space;
    int y_space;
    int index;
    int num;
    leech_dock_bottom_center_s(int width, int height, int x_space = 0, int y_space = 0, int index = 0, int num = 1) :width(width), height(height), x_space(x_space), y_space(y_space), index(index), num(num){}
    void update_ctl_pos();
    /*virtual*/ bool i_leeched( guirect_c &to ) override { if (__super::i_leeched(to)) { update_ctl_pos(); return true;} return false; };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_top_center_s : public autoparam_i
{
    int width;
    int height;
    int x_space;
    int y_space;
    int index;
    int num;
    leech_dock_top_center_s(int width, int height, int x_space = 0, int y_space = 0, int index = 0, int num = 1) :width(width), height(height), x_space(x_space), y_space(y_space), index(index), num(num){}
    void update_ctl_pos();
    /*virtual*/ bool i_leeched(guirect_c &to) override { if (__super::i_leeched(to)) { update_ctl_pos(); return true; } return false; };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_right_center_s : public autoparam_i
{
    int width;
    int height;
    int x_space;
    int y_space;
    int index;
    int num;
    leech_dock_right_center_s(int width, int height, int x_space = 0, int y_space = 0, int index = 0, int num = 1) :width(width), height(height), x_space(x_space), y_space(y_space), index(index), num(num) {}
    void update_ctl_pos();
    /*virtual*/ bool i_leeched( guirect_c &to ) override { if (__super::i_leeched(to)) { update_ctl_pos(); return true;} return false; };
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_bottom_right_s : public autoparam_i
{
    int width;
    int height;
    int x_space;
    int y_space;
    leech_dock_bottom_right_s(int width, int height, int x_space = 0, int y_space = 0) :width(width), height(height), x_space(x_space), y_space(y_space) {}
    void update_ctl_pos();
    /*virtual*/ bool i_leeched(guirect_c &to) override { if (__super::i_leeched(to)) { update_ctl_pos(); return true;} return false; };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_dock_top_right_s : public autoparam_i
{
    int width;
    int height;
    int x_space;
    int y_space;
    leech_dock_top_right_s(int width, int height, int x_space = 0, int y_space = 0) :width(width), height(height), x_space(x_space), y_space(y_space) {}
    void update_ctl_pos();
    /*virtual*/ bool i_leeched(guirect_c &to) override { if (__super::i_leeched(to)) { update_ctl_pos(); return true; } return false; };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_at_right : public autoparam_i
{
    int space;
    guirect_c::sptr_t of;
    leech_at_right(guirect_c *of, int space) :of(of), space(space)
    {
        of->leech(this);
    }
    /*virtual*/ bool i_leeched(guirect_c &to) override
    {
        if (&to != of) 
            if (!__super::i_leeched(to)) return false;

        if (&to == owner)
        {
            evt_data_s d;
            sq_evt(SQ_RECT_CHANGED, of->getrid(), d);
        }
        return true;
    };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_at_left_s : public autoparam_i
{
    int space;
    guirect_c::sptr_t of;
    leech_at_left_s(guirect_c *of, int space) :of(of), space(space)
    {
        of->leech(this);
    }
    /*virtual*/ bool i_leeched(guirect_c &to) override
    {
        if (&to != of) 
            if (!__super::i_leeched(to))
                return false;
        if (&to == owner)
        {
            evt_data_s d;
            sq_evt(SQ_RECT_CHANGED, of->getrid(), d);
        }
        return true;
    };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};


struct leech_save_proportions_s : public autoparam_i
{
    static const int divider = 10000;
    ts::str_c cfgname;
    ts::str_c defaultv;
    leech_save_proportions_s(const ts::asptr& cfgname, const ts::asptr& defvals) :cfgname(cfgname), defaultv(defvals)
    {
    }
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

struct leech_save_size_s : public autoparam_i
{
    ts::str_c cfgname;
    ts::ivec2 defaultsz;
    leech_save_size_s(const ts::asptr& cfgname, const ts::ivec2& defsz) :cfgname(cfgname), defaultsz(defsz)
    {
    }
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};



struct protocol_description_s
{
    MOVABLE( true );

    ts::astrings_c strings;
    int connection_features = 0;
    int features = 0;
    protocol_description_s() {}
    protocol_description_s( const protocol_description_s& o ) :strings( o.strings ), connection_features( o.connection_features ), features( o.features ) {}
    protocol_description_s &operator=(const protocol_description_s& o)
    {
        strings = o.strings;
        connection_features = o.connection_features;
        features = o.features;
        return *this;
    }

    const ts::str_c &getstr( info_string_e s ) const
    {
        if ( s < strings.size() )
            return strings.get( s );
        return ts::make_dummy<ts::str_c>( true );
    }

    ts::wstr_c idname() const
    {
        ts::wstr_c n( from_utf8( getstr(IS_IDNAME) ) );
        if ( n.is_empty() )
            n = CONSTWSTR( "ID" );
        return n;
    }

};

struct available_protocols_s : public ts::array_inplace_t<protocol_description_s,0>
{
    DECLARE_EYELET( available_protocols_s );

public:
    const protocol_description_s *find(const ts::str_c& tag) const
    {
        for (const protocol_description_s& p : *this)
            if (p.getstr(IS_PROTO_TAG) == tag)
                return &p;
        return nullptr;
    }

    void load();
    virtual void done(bool fail) {};

    available_protocols_s() {}
    available_protocols_s(const available_protocols_s&) UNUSED;
    available_protocols_s &operator=(const available_protocols_s &o) UNUSED;
    available_protocols_s &operator=(available_protocols_s &&o)
    {
        ts::array_inplace_t<protocol_description_s,0> *me = this;
        ts::array_inplace_t<protocol_description_s,0> *oth = &o;
        *me = std::move( *oth );
        return *this;
    }

};

enum crypto_constants_e
{
    CC_SALT_SIZE = 16,
    CC_HASH_SIZE = 32,
};

enum db_check_e
{
    DBC_IO_ERROR, // not found ot other IO error
    DBC_NOT_DB,
    DBC_DB,
    DBC_DB_ENCRTPTED,
};

db_check_e check_db(const ts::wstr_c &fn, ts::uint8 *salt /* CC_SALT_SIZE bytes buffer */ );

void gen_salt( ts::uint8 *buf, int blen );
void gen_passwdhash(ts::uint8 *passwhash, const ts::wstr_c &passwd);


