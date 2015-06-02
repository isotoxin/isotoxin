#pragma once
#ifndef _INCLUDE_TSFILESYSTEM
#define _INCLUDE_TSFILESYSTEM

#define FNO_LOWERCASEAUTO 1 // lower case name (only case insensitive os, eg: windows)
#define FNO_REMOVECRAP 2    // removes .. and . and double slashes from path
#define FNO_NORMALIZE 4     // replace enemy_slash to native_slash
#define FNO_PARSENENV 8     // parse environment like %USER%, %APPDATA% and other %nnn%
#define FNO_FULLPATH  16    // build full path according to current directory
#define FNO_TRIMLASTSLASH 32
#define FNO_APPENDSLASH 64
#define FNO_SIMPLIFY (FNO_NORMALIZE|FNO_FULLPATH|FNO_LOWERCASEAUTO|FNO_REMOVECRAP)

namespace ts
{

#define NATIVE_SLASH '\\'
#define ENEMY_SLASH '/'

INLINE bool __is_slash(const wchar c)
{
    return c == NATIVE_SLASH || c == ENEMY_SLASH;
}

wstr_c  TSCALL get_exe_full_name();
void    TSCALL set_work_dir(wstr_c &wd, wstr_c *exename = nullptr);
inline void    set_work_dir()
{
	wstr_c wd;
	set_work_dir(wd);
}

void TSCALL parse_env(wstr_c &envstring); // or call fix_path( FNO_PARSEENV )
void  TSCALL fix_path(wstr_c &ipath, int fnoptions);
inline wstr_c  TSCALL fn_fix_path(const wsptr &ipath, int fnoptions)
{
    wstr_c p(ipath);
    fix_path(p, fnoptions);
    return p;
}

void    TSCALL make_path(const wstr_c &path, int fnoptions);

void    TSCALL fill_dirs_and_files( const wstr_c &path, wstrings_c &files, wstrings_c &dirs );
void    TSCALL del_dir(const wstr_c &path);
void    TSCALL copy_dir(const wstr_c &path_from, const wstr_c &path_clone, const wsptr &skip = CONSTWSTR(".svn;.hg;.git"));

bool    TSCALL dir_present(const wstr_c &path);

bool TSCALL is_file_exists(const wsptr &fname);
bool TSCALL is_file_exists(const wsptr &iroot, const wsptr &fname);

wstr_c   TSCALL get_load_filename_dialog(const wsptr &iroot, const wsptr &name, const wsptr &filt, const wchar *defext, const wchar *title);
bool    TSCALL get_load_filename_dialog(wstrings_c &files, const wsptr &iroot, const wsptr& name, const wsptr &filt, const wchar *defext, const wchar *title);
wstr_c   TSCALL get_save_directory_dialog(const wsptr &root, const wsptr &title, const wsptr &selectpath = wsptr(), bool nonewfolder = false);
wstr_c   TSCALL get_save_filename_dialog(const wsptr &iroot, const wsptr &name, const wsptr &filt, const wchar *defext, const wchar *title);

bool    TSCALL find_files(const wsptr &wildcard, wstrings_c &files, const DWORD dwFileAttributes, const DWORD dwSkipAttributes = 0, bool full_names = false);

void TSCALL fn_split( const wsptr &full_name, wstr_c &name, wstr_c &ext );
template<class CORE> wstr_c	TSCALL fn_join(const str_t<wchar, CORE> &path, const str_t<wchar, CORE> &name)
{
	if (path.is_empty()) return name;
	if (path.get_last_char() == NATIVE_SLASH || path.get_last_char() == ENEMY_SLASH)
	{
		if ( name.get_char(0) == NATIVE_SLASH || name.get_char(0) == ENEMY_SLASH ) return wstr_c(path).append(name.as_sptr().skip(1));
		return wstr_c(path).append(name);
	}
	if ( name.get_char(0) == NATIVE_SLASH || name.get_char(0) == ENEMY_SLASH ) return wstr_c( path ).append( name );
	return wstr_c( path ).append_char( NATIVE_SLASH ).append( name );

}
template<class CORE> wstr_c TSCALL fn_join(const str_t<wchar, CORE> &path, const wsptr &name)
{
	if (path.get_last_char() == NATIVE_SLASH || path.get_last_char() == ENEMY_SLASH)
	{
		if ( name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH ) return wstr_c(path).append(name.skip(1));
		return wstr_c(path,name);
	}
	if ( name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH ) return wstr_c( path ).append( name );
	return wstr_c( path ).append_char( NATIVE_SLASH ).append( name );
}

template<class CORE> wstr_c TSCALL fn_join(const str_t<wchar, CORE> &path, const wsptr &path1, const wsptr &name)
{
	wstr_c t = fn_join(path, path1);
	if (t.get_last_char() == NATIVE_SLASH || t.get_last_char() == ENEMY_SLASH)
	{
		if (name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH) return t.append(name.skip(1));
		return t.append(name);
	}
	if (name.s[0] == NATIVE_SLASH || name.s[0] == ENEMY_SLASH) return t.append(name);
	return t.append_char( NATIVE_SLASH ).append( name );
}
INLINE wstr_c TSCALL fn_get_name_with_ext(const wsptr &name)
{
    int i = pwstr_c(name).find_last_pos_of(CONSTWSTR("/\\"));
	if (i < 0) i = 0; else ++i;
	return wstr_c(name.skip(i));
}
//INLINE wstr_c TSCALL fn_get_short_name(const wsptr &name)
//{
//	return fn_get_name_with_ext(name).case_down();
//}

template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_autoquote(const str_t<TCHARACTER> &name);
wstr_c TSCALL fn_get_next_name(const wsptr &fullname, bool check = true);
wstr_c TSCALL fn_get_name(const wsptr &name);
wstr_c TSCALL fn_get_ext(const wsptr &name);
wstr_c TSCALL fn_get_path(const wstr_c &name); // with ending slash

bool TSCALL fn_mask_match( const wsptr &fn, const wsptr &mask );

template<typename TCHARACTER> void  TSCALL fn_validizate(str_t<TCHARACTER> &name, const TCHARACTER *ext); // ext must be with dot ('.')

template <class TCHARACTER> bool TSCALL parse_text_file(const wsptr &filename, strings_t<TCHARACTER>& text, bool from_utf8 = false);

wstr_c TSCALL fn_change_name(const wsptr &full, const wsptr &name);
wstr_c TSCALL fn_change_ext(const wsptr &full, const wsptr &ext);

inline wstr_c fn_change_name_ext(const wstr_c &full, const wsptr &name, const wsptr &ext)
{
    int i = full.find_last_pos_of(CONSTWSTR("/\\")) + 1;
    return wstr_c(wsptr(full.cstr(), i)).append(name).append_char('.').append(ext);
}

inline wstr_c fn_change_name_ext(const wstr_c &full, const wsptr &nameext)
{
    int i = full.find_last_pos_of(CONSTWSTR("/\\")) + 1;
    return wstr_c(wsptr(full.cstr(), i)).append(nameext);
}


template<class RCV, class STRCORE> bool enum_files(const str_t<wchar, STRCORE> &base, RCV &pred, const str_t<wchar, STRCORE> &path = str_t<wchar, STRCORE>(), const wsptr &wildcard = CONSTWSTR("*.*"))
{
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(fn_join<STRCORE>(base,path,wildcard), &fd);
	if (h == INVALID_HANDLE_VALUE) return true;

	do
	{
		str_t<wchar, STRCORE> sFileName(fd.cFileName);
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (sFileName == CONSTWSTR(".") || sFileName == CONSTWSTR("..")) continue;
				if (!enum_files(base, pred, fn_join(path,sFileName), wildcard)) {FindClose(h); return false;}
			}
			else
				if (!pred(base, fn_join(path,sFileName))) return true;
	} while (FindNextFileW(h, &fd));

	FindClose(h);

	return true;
}


} // namespace ts


#endif