#pragma once

namespace ts
{

irect   TSCALL wnd_get_max_size_fs(const ts::ivec2 &pt);
irect   TSCALL wnd_get_max_size(const ts::ivec2& pt);

irect   TSCALL wnd_get_max_size(const irect &rfrom);
irect   TSCALL wnd_get_max_size_fs(const irect &rfrom);

ivec2   TSCALL wnd_get_center_pos( const ts::ivec2& size );
void    TSCALL wnd_fix_rect(irect &r, int minw, int minh);

int     TSCALL monitor_count();
irect   TSCALL monitor_get_max_size_fs(int monitor);
irect   TSCALL monitor_get_max_size(int monitor);

wstr_c  TSCALL monitor_get_description(int monitor);

wstr_c TSCALL get_clipboard_text();
void TSCALL set_clipboard_text(const wsptr &text);

bitmap_c TSCALL get_clipboard_bitmap();
void TSCALL set_clipboard_bitmap(const bitmap_c &bmp);

void TSCALL open_link(const ts::wstr_c &lnk);
void TSCALL explore_path( const wsptr &path, bool path_only );

ivec2 TSCALL get_cursor_pos();

bool TSCALL is_admin_mode();

str_c gen_machine_unique_string();

}
