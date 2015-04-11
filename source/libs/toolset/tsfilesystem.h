#pragma once
#ifndef _INCLUDE_TSFILESYSTEM
#define _INCLUDE_TSFILESYSTEM

namespace ts
{
	enum compression_method_e
	{
		COMPRESSION_METHOD_NONE = 0,
		COMPRESSION_METHOD_LZO = 1,
		COMPRESSION_METHOD_DEFLATE = 2,
		COMPRESSION_METHOD_LZMA2 = 3,
	};

#define COMPRESSION_METHOD_MASK ((1<<8)-1)//0xFF
#define COMPRESSION_FILTER_MASK  (1<<8)
#define COMPRESSION_FILTER_SHIFT 8


wstr_c  TSCALL get_exe_full_name(void);
void    TSCALL set_work_dir(wstr_c &wd, wstr_c *exename = NULL);
__inline void    set_work_dir(void)
{
	wstr_c wd;
	set_work_dir(wd);
}

template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL simplify_path(const str_t<TCHARACTER> &ipath, bool lower_case=true, bool parse_env = false);

void    TSCALL make_path(const wstr_c &path, bool lower_case = true);

void    TSCALL fill_dirs_and_files( const wstr_c &path, wstrings_c &files, wstrings_c &dirs );
void    TSCALL del_dir(const wstr_c &path);
void    TSCALL copy_dir(const wstr_c &path_from, const wstr_c &path_clone);

bool    TSCALL dir_present(const wstr_c &path, bool path_already_nice = false);

bool TSCALL is_file_exists(const wsptr &fname);
bool TSCALL is_file_exists(const wsptr &iroot, const wsptr &fname);

wstr_c   TSCALL get_load_filename_dialog(const wsptr &iroot, const wsptr &name, const wsptr &filt, const wchar *defext, const wchar *title);
bool    TSCALL get_load_filename_dialog(wstrings_c &files, const wsptr &iroot, const wsptr& name, const wsptr &filt, const wchar *defext, const wchar *title);
wstr_c   TSCALL get_save_directory_dialog(const wsptr &root, const wsptr &title, const wsptr &selectpath = wsptr(), bool nonewfolder = false);
wstr_c   TSCALL get_save_filename_dialog(const wsptr &iroot, const wsptr &name, const wsptr &filt, const wchar *defext, const wchar *title);

bool    TSCALL find_files(const wsptr &wildcard, wstrings_c &files, const DWORD dwFileAttributes, const DWORD dwSkipAttributes = 0, bool full_names = false);

template<typename TCHARACTER> void TSCALL parse_env(str_t<TCHARACTER> &envstring);

template<typename TCHARACTER> str_t<TCHARACTER> TSCALL get_full_path(const str_t<TCHARACTER> &path, bool lower_case = true, bool parse_env = false);

template<typename TCHARACTER> void              TSCALL fn_split( const str_t<TCHARACTER> &full_name, str_t<TCHARACTER> &name, str_t<TCHARACTER> &ext );
template<typename TCHARACTER, class CORE> str_t<TCHARACTER>	TSCALL fn_join(const str_t<TCHARACTER, CORE> &path, const str_t<TCHARACTER, CORE> &name)
{
	if (path.is_empty()) return name;
	if (path.get_last_char() == '\\' || path.get_last_char() == '/')
	{
		if ( name.get_char(0) == '\\' || name.get_char(0) == '/' ) return str_t<TCHARACTER>(path).append(name.as_sptr().skip(1));
		return str_t<TCHARACTER>(path).append(name);
	}
	if ( name.get_char(0) == '\\' || name.get_char(0) == '/' ) return str_t<TCHARACTER>( path ).append( name );
	return str_t<TCHARACTER>( path ).append_char( '\\' ).append( name );

}
template<typename TCHARACTER, class CORE> str_t<TCHARACTER> TSCALL fn_join(const str_t<TCHARACTER, CORE> &path, const sptr<TCHARACTER> &name)
{
	if (path.get_last_char() == '\\' || path.get_last_char() == '/')
	{
		if ( name.s[0] == '\\' || name.s[0] == '/' ) return str_t<TCHARACTER>(path).append(name.skip(1));
		return str_t<TCHARACTER>(path).append(name);
	}
	if ( name.s[0] == '\\' || name.s[0] == '/' ) return str_t<TCHARACTER>( path ).append( name );
	return str_t<TCHARACTER>( path ).append_char( '\\' ).append( name );
}

template<typename TCHARACTER, class CORE> str_t<TCHARACTER> TSCALL fn_join(const str_t<TCHARACTER, CORE> &path, const sptr<TCHARACTER> &path1, const sptr<TCHARACTER> &name)
{
	str_t<TCHARACTER> t = fn_join(path, path1);
	if (t.get_last_char() == '\\' || t.get_last_char() == '/')
	{
		if (name.s[0] == '\\' || name.s[0] == '/') return t.append(name.skip(1));
		return t.append(name);
	}
	if (name.s[0] == '\\' || name.s[0] == '/') return t.append(name);
	return t.append_char( '\\' ).append( name );
}
template<typename TCHARACTER> str_t<TCHARACTER> TSCALL fn_get_name_with_ext(const str_t<TCHARACTER> &name)
{
    int i = name.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
	if (i < 0) i = 0; else ++i;
	return str_t<TCHARACTER>(name.cstr() + i);
}
template<typename TCHARACTER> str_t<TCHARACTER> TSCALL fn_get_short_name(const str_t<TCHARACTER> &name)
{
	return fn_get_name_with_ext(name).case_down();
}

template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_autoquote(const str_t<TCHARACTER> &name);
template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_next_name(const str_t<TCHARACTER> &fullname, bool check = true);
template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_name(const sptr<TCHARACTER> &name);
template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_ext(const str_t<TCHARACTER> &name);
template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_path(const str_t<TCHARACTER> &name); // with ending slash

template<typename TCHARACTER> bool TSCALL fn_mask_match( const sptr<TCHARACTER> &fn, const sptr<TCHARACTER> &mask );

template<typename TCHARACTER> void  TSCALL fn_validizate(str_t<TCHARACTER> &name, const TCHARACTER *ext); // ext must be with dot ('.')

template <class TCHARACTER> bool TSCALL parse_text_file(const wsptr &filename, strings_t<TCHARACTER>& text, bool from_utf8 = false);

template<typename TCHARACTER> str_t<TCHARACTER>   TSCALL fn_change_name(const str_t<TCHARACTER> &full, const sptr<TCHARACTER> &name);
template<typename TCHARACTER> str_t<TCHARACTER>   TSCALL fn_change_ext(const str_t<TCHARACTER> &full, const sptr<TCHARACTER> &ext);

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


template<class TPred, class S_CORE> bool enum_files(const str_t<wchar, S_CORE> &base, TPred &pred, const str_t<wchar, S_CORE> &path = str_t<wchar, S_CORE>(), const wchar * wildcard = L"*.*")
{
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(fn_join<wchar, S_CORE>(base,path,wildcard), &fd);
	if (h == INVALID_HANDLE_VALUE) return true;

	do
	{
		str_t<wchar, S_CORE> sFileName(fd.cFileName);
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