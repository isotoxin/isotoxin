#include "toolset.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace ts
{
	static INLINE DWORD GetCurrentDirectoryX( __in DWORD nBufferLength, LPWSTR lpBuffer )
	{
		return GetCurrentDirectoryW( nBufferLength, lpBuffer );
	}
	static INLINE DWORD GetCurrentDirectoryX( __in DWORD nBufferLength, LPSTR lpBuffer )
	{
		return GetCurrentDirectoryA( nBufferLength, lpBuffer );
	}
	static INLINE DWORD GetFullPathNameX( LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR *lpFilePart )
	{
		return GetFullPathNameW(lpFileName, nBufferLength, lpBuffer, lpFilePart);
	}
	static INLINE DWORD GetFullPathNameX( LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR *lpFilePart )
	{
		return GetFullPathNameA(lpFileName, nBufferLength, lpBuffer, lpFilePart);
	}

	INLINE DWORD WINAPI GetEnvironmentVariable__( const char * lpName, char * lpBuffer, DWORD nSize )
	{
		return GetEnvironmentVariableA(lpName, lpBuffer, nSize);
	}

	INLINE DWORD WINAPI GetEnvironmentVariable__( const wchar * lpName, wchar * lpBuffer, DWORD nSize )
	{
		return GetEnvironmentVariableW(lpName, lpBuffer, nSize);
	}

    INLINE HRESULT SHGetFolderPath__(_Reserved_ HWND hwnd, _In_ int csidl, _In_opt_ HANDLE hToken, _In_ DWORD dwFlags, _Out_writes_(MAX_PATH) LPSTR pszPath)
    {
        return SHGetFolderPathA(hwnd, csidl, hToken, dwFlags, pszPath);
    }
    INLINE HRESULT SHGetFolderPath__(_Reserved_ HWND hwnd, _In_ int csidl, _In_opt_ HANDLE hToken, _In_ DWORD dwFlags, _Out_writes_(MAX_PATH) LPWSTR pszPath)
    {
        return SHGetFolderPathW(hwnd, csidl, hToken, dwFlags, pszPath);
    }

    INLINE BOOL GetUserName__(char *b, DWORD *bl)
    {
        return GetUserNameA(b, bl);
    }
    INLINE BOOL GetUserName__(wchar *b, DWORD *bl)
    {
        return GetUserNameW(b,bl);
    }

    template<typename TCHARACTER> void TSCALL parse_env(str_t<TCHARACTER> &envstr)
    {
        int dprc = 0;
        for (;;)
        {
            int ii = envstr.find_pos(dprc, '%');
            if (ii < 0) break;
            ii += dprc;
            int iie = ii + 1;
            for (;;)
            {
                if (envstr.get_char(iie) == '%')
                {
                    if ((iie - ii) > 1)
                    {
                        sxstr_t<TCHARACTER, MAX_PATH + 1> b, name;
                        int ll = iie - ii - 1;
                        if (ll >= MAX_PATH)
                        {
                            dprc = iie + 1;
                            break;
                        }
                        name.set(envstr.cstr() + ii + 1, ll);

                        if (name == CONSTSTR(TCHARACTER, "PERSONAL"))
                        {
                            if (SUCCEEDED(SHGetFolderPath__(nullptr, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, sptr<TCHARACTER>(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }

                        }
                        else if (name == CONSTSTR(TCHARACTER, "APPDATA"))
                        {
                            if (SUCCEEDED(SHGetFolderPath__(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, sptr<TCHARACTER>(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if (name == CONSTSTR(TCHARACTER, "PROGRAMS"))
                        {
                            if (SUCCEEDED(SHGetFolderPath__(nullptr, CSIDL_PROGRAM_FILES | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, sptr<TCHARACTER>(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if (name == CONSTSTR(TCHARACTER, "USER"))
                        {
                            DWORD blen = b.get_capacity();
                            if (GetUserName__(b.str(), &blen))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, sptr<TCHARACTER>(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }

                        }
                        else
                        {

                            int pl = GetEnvironmentVariable__(name, b.str(), MAX_PATH);
                            if (pl && pl < MAX_PATH)
                            {
                                envstr.replace(ii, ll + 2, sptr<TCHARACTER>(b, pl));
                                break;

                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }

                    }
                    else
                    {
                        dprc = iie + 1;
                        break;
                    }
                }
                if (CHAR_is_letter(envstr.get_char(iie)) || CHAR_is_digit(envstr.get_char(iie)) || envstr.get_char(iie) == '_')
                {
                    ++iie;
                    continue;
                }
                dprc = iie + 1;
                break;
            }
        }
    }
    template void  TSCALL parse_env(str_t<char> &strenv);
    template void TSCALL parse_env(str_t<wchar> &strenv);

	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL get_full_path(const str_t<TCHARACTER> &ipath, bool lower_case, bool bparse_env)
	{
		str_t<TCHARACTER> path(ipath);

		if (bparse_env)
            parse_env(path);


		path.replace_all('/', '\\');

        sxstr_t<TCHARACTER, 8> disk;

		if (path.get_length() > 1 && (path.get_char(1) == ':'))
		{
			disk.set(path.substr(0,2));
			path.cut(0,2);
		}

		if (path.get_char(0) != '\\')
		{
			sxstr_t<TCHARACTER, 2048> buf;
			if ( disk.is_empty() )
			{
				DWORD len = GetCurrentDirectoryX(2048-8, buf.str());
				buf.set_length(len);
				if (buf.get_last_char() != '\\') buf.append_char('\\');
				path.insert(0,buf);
			} else
			{
				DWORD len =GetFullPathNameX( disk, 2048-8, buf.str(), nullptr );
				buf.set_length(len);
				if (buf.get_last_char() != '\\') buf.append_char('\\');
				path.insert(0,buf);
			}

		} else
			path.insert(0,disk);

		return lower_case ? path.case_down() : path;
	}

	template str_t<char>  TSCALL get_full_path(const str_t<char> &path, bool lower_case, bool parse_env);
	template str_t<wchar>  TSCALL get_full_path(const str_t<wchar> &path, bool lower_case, bool parse_env);


	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL simplify_path(const str_t<TCHARACTER> &ipath, bool lower_case, bool parse_env)
	{
		bool slashing = false;

		str_t<TCHARACTER> path(get_full_path<TCHARACTER>(ipath, lower_case, parse_env));

		if(path.get_char(path.get_length()-1) == '\\')
        {
			path.cut(path.get_length()-1);
			slashing = true;
		}

		strings_t<TCHARACTER> sa(path, '\\');

		sa.kill_empty_slow(1);

		for (int i=0;i<sa.size();)
		{
			const str_t<TCHARACTER> &ss = sa.get(i);

			if (ss.get_char(0) == '.')
			{
				if (ss.get_length() == 2 && ss.get_char(1) == '.')
				{
					sa.remove_slow(i);
					if (i > 0)
					{
						--i;
						sa.remove_slow(i);
					}
					continue;
				}
				if ( ss.get_length() == 1 )
				{
					sa.remove_slow(i);
					continue;
				}

			}
			++i;
		}

		path.set_length(0);

		for (const auto & s : sa)
			path.append('\\', s);

		return path;
	}
	template str_t<char>  TSCALL simplify_path(const str_t<char> &ipath, bool lower_case, bool parse_env);
	template str_t<wchar>  TSCALL simplify_path(const str_t<wchar> &ipath, bool lower_case, bool parse_env);


	void TSCALL fill_dirs_and_files( const wstr_c &path, wstrings_c &files, wstrings_c &dirs )
	{
		wstr_c wildcard(fn_join( path, wsptr(CONSTWSTR("*.*")) ));
		WIN32_FIND_DATAW fff;

		HANDLE h = FindFirstFileW(wildcard, &fff);
		if (h == INVALID_HANDLE_VALUE)
		{
			dirs.add(path);
			return;
		}
		for (BOOL isFound = TRUE; isFound; isFound = FindNextFileW(h, &fff))
		{
			pwstr_c sFileName(wsptr(fff.cFileName, CHARz_len(fff.cFileName)));
			if (fff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (sFileName == CONSTWSTR(".") || sFileName == CONSTWSTR("..") ) continue;
				fill_dirs_and_files( fn_join( path, sFileName.as_sptr() ), files, dirs );
			} else
			{
				files.add( fn_join( path, sFileName.as_sptr() ) );
			}
		}
		FindClose( h );
		dirs.add( path );
	}

	void    TSCALL copy_dir(const wstr_c &path_from, const wstr_c &path_clone)
	{
		wstrings_c files;
		wstrings_c dirs;

		fill_dirs_and_files( path_from, files, dirs );

		int pfl = path_from.get_length();
		if (path_from.get_last_char() == '\\' || path_from.get_last_char() == '/') --pfl;

		int pfc = path_clone.get_length();
		if (path_clone.get_last_char() == '\\' || path_clone.get_last_char() == '/') --pfc;

		for (int i=dirs.size()-1;i>=0;--i)
		{
			int x = dirs.get(i).find_pos(L"\\.svn");
			if (x >= 0 && (dirs.get(i).get_char(x+5) == 0 || dirs.get(i).get_char(x+5) == '\\'))
			{
				dirs.remove_fast(i);
				continue;
			}

			dirs.get(i).replace( 0, pfl, wsptr(path_clone.cstr(), pfc) );
		}
		dirs.sort(true);
		
        for (const auto & s : dirs)
			make_path( s, false );

		wstr_c t,tt;
		for (int i=files.size()-1;i>=0;--i)
		{
			int x = files.get(i).find_pos(L"\\.svn");
			if (x >= 0 && (files.get(i).get_char(x+5) == 0 || files.get(i).get_char(x+5) == '\\'))
			{
				continue;
			}

			t = files.get(i);
			tt = t;
			tt.replace( 0, pfl, wsptr(path_clone.cstr(), pfc) );
			CopyFileW( t, tt, false );
		}


	}

	void    TSCALL del_dir(const wstr_c &path)
	{
		wstrings_c files;
		wstrings_c dirs;

		fill_dirs_and_files( path, files, dirs );

        for (const auto & s : files)
		{
			SetFileAttributesW(s,FILE_ATTRIBUTE_NORMAL);
			DeleteFileW(s);
		}

		for (const auto & s : dirs)
		{
			SetFileAttributesW(s,FILE_ATTRIBUTE_NORMAL);
			RemoveDirectoryW(s);
		}
	}

	void    TSCALL make_path(const wstr_c &ipath, bool lower_case)
	{

		wstr_c path(get_full_path(ipath, lower_case));

		if ( path.get_last_char() == '\\' ) path.trunc_length();

		wstrings_c sa(path, '\\');

		for (int i=0;i<sa.size();)
		{
			if (sa.get(i) == CONSTWSTR("."))
			{
				sa.remove_slow(i);
				continue;
			}
			if (sa.get(i) == CONSTWSTR(".."))
			{
				sa.remove_slow(i);
				if (i > 0)
				{
					--i;
					sa.remove_slow(i);
				}
				continue;
			}
			++i;
		}

		path.set_length(0);

		for (int i=ipath.begins(L"\\\\") ? (path.set(L"\\\\", 2).append(sa.get(2)).append_char('\\'), 3) : 0; i<sa.size(); ++i)
		{
			path.append(sa.get(i));
			if (i == 0)
			{
				ASSERT( path.get_last_char() == ':' );
				path.append_char('\\');
				continue;
			}

			if (!PathFileExistsW(path.cstr()))
			{
				if (0 == CreateDirectoryW(path.cstr(), nullptr)) return;
			}
			path.append_char('\\');
		}
	}

    template <class TCHARACTER> bool TSCALL parse_text_file(const wsptr &filename, strings_t<TCHARACTER>& text, bool from_utf8)
    {
        buf_c buffer;
        text.clear();
        if (buffer.load_from_file(filename))
        {
            if (buffer.size() > 0)
            {
                if (buffer.size() >= 2)
                {
                    if (*buffer.data16() == 0xFEFF)
                    {
                        // native ucs16, no need swap bytes
                        text.split(wsptr((const wchar*)(buffer.data() + 2), (buffer.size() - 2) / sizeof(wchar)), (wchar)'\n');
                        return true;
                    }
                    if (*buffer.data16() == 0xFFFE)
                    {
                        // alien ucs16 - swap bytes required
                        const wchar * endofbuf = (const wchar *)(buffer.data() + buffer.size());
                        for (const wchar *b = (const wchar *)buffer.data16(); b < endofbuf; ++b)
                            SWAP(*(uint8 *)b, *(((uint8 *)b) + 1));
                        text.split(wsptr((const wchar*)(buffer.data() + 2), (buffer.size() - 2) / sizeof(wchar)), (wchar)'\n');
                        return true;
                    }
                }

                if (from_utf8)
                    text.split<wchar>(wstr_c().set_as_utf8(asptr((const char*)buffer.data(), buffer.size())), '\n');
                else
                    text.split<char>(asptr((const char*)buffer.data(), buffer.size()), '\n');
                return true;
            }
        }
        return false;
    }
    template bool TSCALL parse_text_file<char>(const wsptr &filename, strings_t<char>& text, bool);
    template bool TSCALL parse_text_file<wchar>(const wsptr &filename, strings_t<wchar>& text, bool);

	template<typename TCHARACTER> void                     TSCALL fn_split( const str_t<TCHARACTER> &full_name, str_t<TCHARACTER> &name, str_t<TCHARACTER> &ext )
	{
		int i = full_name.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
		if (i < 0) i = 0; else ++i;
		int j = full_name.find_last_pos('.');
		if (j < i)
		{
			name.set(full_name.cstr() + i, full_name.get_length() - i);
			ext.clear();
		} else
		{
			name.set(full_name.cstr() + i, j - i);
			ext.set(full_name.cstr() + j + 1, full_name.get_length() - j - 1);
		}
	}

	template void TSCALL fn_split<wchar>( const str_t<wchar> &full_name, str_t<wchar> &name, str_t<wchar> &ext );
	template void TSCALL fn_split<char>( const str_t<char> &full_name, str_t<char> &name, str_t<char> &ext );

	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_next_name(const str_t<TCHARACTER> &fullname, bool check)
	{
		if (check)
		{
			str_t<TCHARACTER> n = fn_get_next_name(fullname,false);

			WIN32_FIND_DATAW fd;

			for(;;)
			{
				HANDLE h = FindFirstFileW(to_wstr(n),&fd);
				if ( INVALID_HANDLE_VALUE == h ) return n;
				FindClose(h);
				n = fn_get_next_name(n,false);
			}
		}

		str_t<TCHARACTER> name = fn_get_name(fullname.as_sptr());
		int index = name.get_length() - 1;
		for (; index >= 0 && CHAR_is_digit(name.get_char(index)) ;--index);
		uint nn = name.substr(++index).as_uint() + 1;
		name.replace(index,name.get_length()-index, str_t<TCHARACTER>().set_as_uint(nn));
		return fn_change_name<TCHARACTER>(fullname,name);

	}

	template str_c  TSCALL fn_get_next_name<char>(const str_c &fullname,bool check);
	template wstr_c  TSCALL fn_get_next_name<wchar>(const wstr_c &fullname,bool check);


	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_autoquote(const str_t<TCHARACTER> &name)
	{
		if (name.find_pos(' ') >= 0 && name.get_char(0) != '\"' && name.get_last_char() != '\"')
			return str_t<TCHARACTER>().append_char('\"').append(name).append_char('\"');
		return name;
	}

	template str_c  TSCALL fn_autoquote<char>(const str_c &name);
	template wstr_c  TSCALL fn_autoquote<wchar>(const wstr_c &name);



	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_name(const sptr<TCHARACTER> &name_)
	{
        pstr_t<TCHARACTER> name(name_);
		int i = name.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
		if (i < 0) i = 0; else ++i;
		int j = name.find_last_pos('.');
		if (j < i) j = name.get_length();
		return str_t<TCHARACTER>(name_.s + i, j - i);
	}

	template str_c  TSCALL fn_get_name<char>(const asptr &name);
	template wstr_c  TSCALL fn_get_name<wchar>(const wsptr &name);

	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_ext(const str_t<TCHARACTER> &name)
	{
		int i = name.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
		int j = name.find_last_pos('.');
		if (j <= i) j = name.get_length(); else ++j;
		return str_t<TCHARACTER>(name.cstr() + j);
	}
	template str_t<char>  TSCALL fn_get_ext(const str_t<char> &name);
	template str_t<wchar>  TSCALL fn_get_ext(const str_t<wchar> &name);

	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_get_path(const str_t<TCHARACTER> &name) // with ending slash
	{
		int i = name.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
		if (i<0) return name;
		return str_t<TCHARACTER>(name.cstr(), i + 1);
	}
	template str_t<wchar>  TSCALL fn_get_path(const str_t<wchar> &name); // with ending slash
	template str_t<char>  TSCALL fn_get_path(const str_t<char> &name); // with ending slash

#define FROMCS "‡·‚„‰Â∏ÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛ˇ¿¡¬√ƒ≈®∆«»… ÀÃÕŒœ–—“”‘’÷◊ÿŸ⁄€‹›ﬁﬂ"; ///\\.,;!@#$%^&*()[]|`~{}-=<>\"\'"
#define   TOCS "abvgdeezziyklmnoprstufhccssyyyeuaABVGDEEZZIYKLMNOPRSTUFHCCSSYYYEUA"; //_______________________________"

	static INLINE wchar *_fromc( const wchar * )
	{
		return JOINMACRO1( L, FROMCS );
	}
	static INLINE wchar *_toc( const wchar * )
	{
		return JOINMACRO1( L, TOCS );
	}
	static INLINE char *_fromc( const char * )
	{
		return FROMCS;
	}
	static INLINE char *_toc( const char * )
	{
		return TOCS;
	}

	template<typename TCHARACTER> void  TSCALL fn_validizate(str_t<TCHARACTER> &name, const TCHARACTER *ext)
	{
		int cnt = name.get_length();
		bool already_with_ext = false;
		if (ext && name.ends_ignore_case(ext))
		{
			already_with_ext = true;
			cnt -= CHARz_len(ext);
		}
		for (int i=0;i<cnt;++i)
		{
			int ii = CHARz_find( _fromc(ext), name.get_char(i) );
			if (ii >= 0) name.set_char(i, _toc(ext)[ii] );
			TCHARACTER c = name.get_char(i);
			if ( (c >= '0' && c <= '9') ||
				(c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') || c == '_' ) ; else
			{
				name.set_char(i, '_');
			}
		}
		if (ext && !already_with_ext)
		{
			name.append(ext);
		}
	}
	template void  TSCALL fn_validizate(str_t<wchar> &name, const wchar *ext); // with ending slash
	template void  TSCALL fn_validizate(str_t<char> &name, const char *ext); // with ending slash

	template<typename TCHARACTER> str_t<TCHARACTER>   TSCALL fn_change_name(const str_t<TCHARACTER> &full, const sptr<TCHARACTER> &name)
	{
		int i = full.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
		if (i < 0) i = 0; else ++i;
		int j = full.find_last_pos('.');
		if (j < i) j = full.get_length();
		return str_t<TCHARACTER>(sptr<TCHARACTER>(full.cstr(), i)).append(name).append(full.as_sptr().skip(j));
	}

	template str_t<char>   TSCALL fn_change_name(const str_t<char> &full, const sptr<char> &name);
	template str_t<wchar>   TSCALL fn_change_name(const str_t<wchar> &full, const sptr<wchar> &name);

	template<typename TCHARACTER> str_t<TCHARACTER>   TSCALL fn_change_ext(const str_t<TCHARACTER> &full, const sptr<TCHARACTER> &ext)
	{
		int i = full.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\"));
		int j = full.find_last_pos('.');
		if (j <= i) j = full.get_length();
		return str_t<TCHARACTER>(sptr<TCHARACTER>(full.cstr(), j)).append_char('.').append(ext);
	}

	template str_t<char>   TSCALL fn_change_ext<char>(const str_t<char> &full, const sptr<char> &ext);
	template str_t<wchar>   TSCALL fn_change_ext<wchar>(const str_t<wchar> &full, const sptr<wchar> &ext);


	template<typename TCHARACTER> str_t<TCHARACTER> TSCALL fn_change_name_ext(const str_t<TCHARACTER> &full, const sptr<TCHARACTER> &name, const sptr<TCHARACTER> &ext)
	{
		int i = full.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\")) + 1;
		return str_t<TCHARACTER>(sptr<TCHARACTER>(full.cstr(), i)).append(name).append_char('.').append(ext);
	}
	template str_t<char> TSCALL fn_change_name_ext(const str_t<char> &full, const sptr<char> &name, const sptr<char> &ext);
	template str_t<wchar> TSCALL fn_change_name_ext(const str_t<wchar> &full, const sptr<wchar> &name, const sptr<wchar> &ext);

	template<typename TCHARACTER> str_t<TCHARACTER> TSCALL fn_change_name_ext(const str_t<TCHARACTER> &full, const sptr<TCHARACTER> &nameext)
	{
		int i = full.find_last_pos_of(CONSTSTR(TCHARACTER, "/\\")) + 1;
		return str_t<TCHARACTER>(sptr<TCHARACTER>(full.cstr(), i)).append(nameext);
	}
	template str_t<char> TSCALL fn_change_name_ext(const str_t<char> &full, const asptr &nameext);
	template str_t<wchar> TSCALL fn_change_name_ext(const str_t<wchar> &full, const wsptr &nameext);

    template<typename TCHARACTER> bool TSCALL fn_mask_match( const sptr<TCHARACTER> &fn, const sptr<TCHARACTER> &mask )
    {
        int index = 0;
        token<TCHARACTER> mp(mask, '*');
        bool left = true;
        for(;mp;++mp)
        {
            int i = mp->get_length() == 0 ? index : pstr_t<TCHARACTER>(fn).find_pos_t(index, *mp);
            if (i < 0) return false;
            if (left && i != 0) return false;
            left = false;
            index = i + mp->get_length();
        }

        return index == fn.l;
    }
    template bool TSCALL fn_mask_match( const asptr &, const asptr & );
    template bool TSCALL fn_mask_match( const wsptr &, const wsptr & );


    bool TSCALL is_file_exists(const wsptr &fname)
    {
        return INVALID_FILE_ATTRIBUTES != GetFileAttributesW(tmp_wstr_c(fname));
    }

	bool    TSCALL is_file_exists(const wsptr &iroot, const wsptr &fname)
	{
		wstr_c path(iroot);
		if (path.is_empty()) return false;
		wchar lch = path.get_last_char();
		if (lch != '\\' && lch != '/') path.append_char('\\');

		wstrings_c sa;

        wstr_c new_path(path);
        aint lclip = new_path.get_length();

		if (find_files(new_path.set_length(lclip).append(fname), sa, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY)) return true;
		if (find_files(new_path.set_length(lclip).append(CONSTWSTR("*.*")), sa, FILE_ATTRIBUTE_DIRECTORY))
		{
			for (const auto & dir : sa)
			{
				if (dir == CONSTWSTR(".") || dir == CONSTWSTR("..")) continue;
				new_path.set_length(lclip).append(dir);
				if (is_file_exists(new_path, fname)) return true;
			}
		}
		return false;
	}

	wstr_c  TSCALL get_exe_full_name()
	{
		wstr_c wd;
		wd.set_length(2048 - 8);
		int len = GetModuleFileNameW(nullptr, wd.str(), 2048 - 8);
		wd.set_length( len );

		if (wd.get_char(0) == '\"')
		{
			int s = wd.find_pos(1,'\"');
			wd.set_length(s);
		}

		wd.trim_right();
		wd.trim_right('\"');
		wd.trim_left('\"');

		return wd;
	}

	void  TSCALL set_work_dir(wstr_c &wd, wstr_c *exename)
	{
		wd = get_exe_full_name();

		int idx = wd.find_last_pos_of(L"/\\");
		if (idx < 0)
		{
			//while(true) Sleep(0);
			DEBUG_BREAK(); // OPA!
		}
		if (exename)
		{
			exename->set( sptr<wchar>(wd.cstr() + idx + 1) );
			wd.set_length(idx + 1);
		}

		wd.set_length(idx + 1);
		SetCurrentDirectoryW( wd.cstr() );
	}

	static int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
	{
		switch (uMsg)
		{
		case BFFM_INITIALIZED:
			if (lpData) SendMessageW(hWnd,BFFM_SETSELECTION,TRUE,lpData);
			break;
		}
		return 0;
	}

	wstr_c TSCALL  get_save_directory_dialog(const wsptr &root, const wsptr &title, const wsptr &selectpath, bool nonewfolder)
	{
		wstr_c buffer;
		buffer.clear();

		LPMALLOC pMalloc;
		if (::SHGetMalloc(&pMalloc) == NOERROR)
		{
			LPITEMIDLIST idl = root.s ? ILCreateFromPathW(wstr_c(root)) : nullptr;

			BROWSEINFOW bi;
			ZeroMemory(&bi, sizeof(bi));

            wstr_c title_(title);
            wstr_c selectpath_(selectpath);

			bi.hwndOwner = g_main_window;
			bi.pidlRoot  = idl;
			bi.lpszTitle = title_;
			bi.lpfn      = BrowseCallbackProc;
			bi.lParam    = (LPARAM)selectpath_.cstr();
			bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | (nonewfolder ? BIF_NONEWFOLDERBUTTON : 0);
			LPITEMIDLIST pidl = ::SHBrowseForFolderW(&bi);
			if (pidl != nullptr) {
				buffer.set_length(2048-16);
				if (::SHGetPathFromIDListW(pidl, buffer.str()))
				{
					buffer.set_length(CHARz_len(buffer.cstr()));
				}
				else
				{
					buffer.clear();            
				}
				pMalloc->Free(pidl);
			}
			ILFree(idl);
			pMalloc->Release();
		}

		return buffer;
	}

	wstr_c   TSCALL get_save_filename_dialog(const wsptr &iroot, const wsptr &name, const wsptr &filt, const wchar *defext, const wchar *title)
	{
		wchar cdp[ MAX_PATH + 16 ];
		GetCurrentDirectoryW(MAX_PATH + 15,cdp);

		OPENFILENAMEW o;
		memset(&o, 0, sizeof(OPENFILENAMEW));

		wstr_c root(get_full_path<wchar>(iroot));

		o.lStructSize=sizeof(o);
		o.hwndOwner=g_main_window;
		o.hInstance=GetModuleHandle(nullptr);

		wstr_c filter(filt);
		filter.replace_all('/',0);

		wstr_c buffer(MAX_PATH + 16,true);
        buffer.set(name);

		o.lpstrTitle = title;
		o.lpstrFile = buffer.str();
		o.nMaxFile=MAX_PATH;
		o.lpstrDefExt = defext;

		o.lpstrFilter = filter.is_empty() ? nullptr : filter.cstr();
		o.lpstrInitialDir = root;
		o.Flags=OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_OVERWRITEPROMPT;

		if(!GetSaveFileNameW(&o))
		{
			SetCurrentDirectoryW(cdp);
			return wstr_c();
		}


		SetCurrentDirectoryW(cdp);
		buffer.set_length( CHARz_len(buffer.cstr()) );

		return buffer;
	}

	wstr_c   TSCALL get_load_filename_dialog(const wsptr &iroot, const wsptr &name, const wsptr &filt, const wchar *defext, const wchar *title)
	{
		wchar cdp[ MAX_PATH ];
		GetCurrentDirectoryW(MAX_PATH,cdp);

		OPENFILENAMEW o;
		memset(&o, 0, sizeof(OPENFILENAMEW));

		wstr_c root(get_full_path<wchar>(iroot));

		o.lStructSize=sizeof(o);
        o.hwndOwner = g_main_window;
        o.hInstance = GetModuleHandle(nullptr);

		wstr_c filter(filt);
		filter.replace_all('/',0);

		wstr_c buffer;
		buffer.set_length(2048);
		buffer.set(name);

		o.lpstrTitle = title;
		o.lpstrFile = buffer.str();
		o.nMaxFile=2048;
		o.lpstrDefExt = defext;

		o.lpstrFilter = filter;
		o.lpstrInitialDir = root;
		o.Flags=OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

		if(!GetOpenFileNameW(&o))
		{
			SetCurrentDirectoryW(cdp);
			return wstr_c();
		}


		SetCurrentDirectoryW(cdp);
		buffer.set_length( CHARz_len(buffer.cstr()) );
		return buffer;
	}


	bool    TSCALL get_load_filename_dialog(wstrings_c &files, const wsptr &iroot, const wsptr& name, const wsptr &filt, const wchar *defext, const wchar *title)
	{
		wchar cdp[ MAX_PATH ];
		GetCurrentDirectoryW(MAX_PATH,cdp);

		OPENFILENAMEW o;
		memset(&o, 0, sizeof(OPENFILENAMEW));

		wstr_c root(get_full_path<wchar>(iroot));

		o.lStructSize=sizeof(o);
        o.hwndOwner = g_main_window;
        o.hInstance = GetModuleHandle(nullptr);

		wstr_c filter(filt);
		filter.replace_all('/',0);

		wstr_c buffer(16384, true);
		buffer.set(name);

		o.lpstrTitle = title;
		o.lpstrFile = buffer.str();
		o.nMaxFile=16384;
		o.lpstrDefExt = defext;

		o.lpstrFilter = filter.cstr();
		o.lpstrInitialDir = root.cstr();
		o.Flags=OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;
		if(!GetOpenFileNameW(&o))
		{
			SetCurrentDirectoryW(cdp);
			return false;
		}

		SetCurrentDirectoryW(cdp);

		files.clear();


		const wchar *zz = buffer.cstr() + o.nFileOffset;

		if ( o.nFileOffset < CHARz_len( buffer.cstr() ) )
		{
			files.add( buffer.cstr() );
			zz += CHARz_len(zz) + 1;
		}

		for (; *zz ; zz += CHARz_len(zz) + 1 )
		{
			files.add( wstr_c(buffer.cstr()).append_char('\\').append(zz) );
		}

		return true;
	}

	bool    TSCALL dir_present(const wstr_c &path, bool path_already_nice)
	{
		WIN32_FIND_DATAW find_data;
		wstr_c p(path);
        if (!path_already_nice) p = simplify_path(path);

		if (p.get_last_char() == '\\') p.trunc_length();
		if ( p.get_length() == 2 && p.get_last_char() == ':' )
		{
			UINT r = GetDriveTypeW( p.append_char('\\') );
			return (r != DRIVE_NO_ROOT_DIR);
		}

		HANDLE           fh = FindFirstFileW(p, &find_data);
		if ( fh == INVALID_HANDLE_VALUE ) return false;
		FindClose( fh );

		return (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	}

	bool    TSCALL find_files(const wsptr &wildcard, wstrings_c &files, const DWORD dwFileAttributes, const DWORD dwSkipAttributes, bool full_names)
	{
		WIN32_FIND_DATAW find_data;
		HANDLE           fh = FindFirstFileW(tmp_wstr_c(wildcard), &find_data);
		bool             bResult = false;
        swstr_t<MAX_PATH> path;
        int pl = 0;
        if (full_names)
        {
            int i = pwstr_c(wildcard).find_last_pos_of(CONSTWSTR("/\\"));
            if (ASSERT(i > 0))
            {
                path.set(pwstr_c(wildcard).substr(0, i + 1));
                pl = i+1;
            }

        }

		while (fh != INVALID_HANDLE_VALUE)
		{
			if ((find_data.dwFileAttributes & dwSkipAttributes) == 0)
			{
				if (find_data.dwFileAttributes & dwFileAttributes)
				{
                    if (full_names)
                        files.add(path.set_length(pl).append(find_data.cFileName));
                    else
					    files.add(find_data.cFileName);
					bResult = true;
				}
			}
			if (!FindNextFileW(fh, &find_data)) break;;
		}

		if (fh != INVALID_HANDLE_VALUE) FindClose( fh );

		return bResult;
	};

} //namespace ts