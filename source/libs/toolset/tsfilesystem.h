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
#define FNO_MAKECORRECTNAME 128 // process only name - replace all incorrect filename symbols to _ (underscore)
#define FNO_SIMPLIFY (FNO_NORMALIZE|FNO_FULLPATH|FNO_LOWERCASEAUTO|FNO_REMOVECRAP)
#define FNO_SIMPLIFY_NOLOWERCASE (FNO_NORMALIZE|FNO_FULLPATH|FNO_REMOVECRAP)

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

bool    TSCALL make_path(const wstr_c &path, int fnoptions);

void    TSCALL fill_dirs_and_files( const wstr_c &path, wstrings_c &files, wstrings_c &dirs );
void    TSCALL del_dir(const wstr_c &path);
void    TSCALL copy_dir(const wstr_c &path_from, const wstr_c &path_clone, const wsptr &skip = CONSTWSTR(".svn;.hg;.git"));

bool    TSCALL dir_present(const wstr_c &path);

bool TSCALL is_file_exists(const wsptr &fname);
bool TSCALL is_file_exists(const wsptr &iroot, const wsptr &fname);

struct extension_s
{
    MOVABLE( true );
    ts::wstr_c ext;
    ts::wstr_c desc;
};
struct extensions_s
{
    extensions_s():exts(nullptr, 0) {}
    extensions_s(const extension_s *e, ts::aint cnt, int def = -1):exts(e,cnt), index(def) {}
    array_wrapper_c<const extension_s> exts;
    aint index = -1;
    const wchar *defext() const
    {
        if (index < 0 || index >= exts.size()) return nullptr;
        return (exts.begin() + index)->ext.cstr();
    }
};

#define ATTR_ANY -1
#define ATTR_DIR SETBIT(0)

bool    TSCALL find_files(const wsptr &wildcard, wstrings_c &files, int attributes, int skip_attributes = 0, bool full_names = false);

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
INLINE wstr_c fn_join(const wsptr &ipath, const wsptr &name)
{
    return fn_join<str_core_part_c<ZSTRINGS_WIDECHAR> >( pwstr_c(ipath), name );
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
wstr_c TSCALL fn_get_name(const wsptr &name);
wstr_c TSCALL fn_get_ext(const wsptr &name);
wstr_c TSCALL fn_get_path(const wstr_c &name); // with ending slash

template<typename TCHARACTER> bool TSCALL fn_mask_match( const sptr<TCHARACTER> &fn, const sptr<TCHARACTER> &mask );

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

class enum_files_c
{
#ifdef _WIN32
    uint8 data[ 768 ];
    bool prepare_file();
    void next_int();
#endif


public:

    enum_files_c( const wstr_c &base, const wstr_c &path, const wstr_c &wildcard );
    ~enum_files_c();
    operator bool() const;

    void next();

    void operator++() { next(); }
    void operator++( int ) { ++( *this ); }

    const wstr_c &operator* () const;
    const wstr_c *operator->() const;

    bool is_folder() const;
};

template<class RCV, class STRCORE> bool enum_files(const str_t<wchar, STRCORE> &base, RCV &pred, const str_t<wchar, STRCORE> &path = str_t<wchar, STRCORE>(), const wsptr &wildcard = CONSTWSTR("*.*"))
{
    enum_files_c ef( base, path, wildcard );
    for ( ;ef; ++ef )
    {
        if ( ef.is_folder() )
        {
            if ( !enum_files( base, pred, *ef, wildcard ) )
                return false;
        }
        else
            if ( !pred( base, *ef ) ) return true;
    }

	return true;
}


bool TSCALL check_write_access(const wsptr &path);

bool TSCALL kill_file(const wsptr &path);

enum copy_rslt_e
{
    CRSLT_OK,
    CRSLT_ACCESS_DENIED,
    CRSLT_FAIL
};
copy_rslt_e TSCALL copy_file( const wsptr &existingfn, const wsptr &newfn );
bool TSCALL rename_file( const wsptr &existingfn, const wsptr &newfn );

bool TSCALL is_64bit_os();

} // namespace ts


#endif