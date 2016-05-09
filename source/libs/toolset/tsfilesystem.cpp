#include "toolset.h"
#include "internal/platform.h"

namespace ts
{
    INLINE bool __ending_slash(const wsptr &path)
    {
        return __is_slash(path.s[path.l-1]);
    }
    template<typename CORE> void __append_slash_if_not(str_t<wchar, CORE> &path)
    {
        if (!__ending_slash(path.as_sptr()))
            path.append_char(NATIVE_SLASH);
    }

    void TSCALL parse_env(wstr_c &envstr)
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
                        swstr_t<MAX_PATH + 1> b, name;
                        int ll = iie - ii - 1;
                        if (ll >= MAX_PATH)
                        {
                            dprc = iie + 1;
                            break;
                        }
                        name.set(envstr.cstr() + ii + 1, ll);

                        if (name == CONSTWSTR("PERSONAL"))
                        {
                            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }

                        }
                        else if (name == CONSTWSTR("APPDATA"))
                        {
                            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if (name == CONSTWSTR("TEMP"))
                        {
                            if (DWORD pl =GetTempPathW(MAX_PATH, b.str()))
                            {
                                if (b.cstr()[pl - 1] == '\\') --pl;
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if (name == CONSTWSTR("PROGRAMS"))
                        {
                            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if (name == CONSTWSTR("USER"))
                        {
                            DWORD blen = (DWORD)b.get_capacity();
                            if (GetUserNameW(b.str(), &blen))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }

                        }
                        else if (name == CONSTWSTR("STARTUP"))
                        {

                            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_STARTUP | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if (name == CONSTWSTR("COMMONSTARTUP"))
                        {

                            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_STARTUP | CSIDL_FLAG_CREATE, nullptr, 0, b.str())))
                            {
                                int pl = CHARz_len(b.cstr());
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
                                break;
                            }
                            else
                            {
                                dprc = iie + 1;
                                break;
                            }
                        }
                        else if ( name == CONSTWSTR( "SYSTEM" ) )
                        {

                            if ( SUCCEEDED( SHGetFolderPathW( nullptr, CSIDL_SYSTEM, nullptr, 0, b.str() ) ) )
                            {
                                int pl = CHARz_len( b.cstr() );
                                envstr.replace( ii, ll + 2, wsptr( b, pl ) );
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

                            int pl = GetEnvironmentVariableW(name, b.str(), MAX_PATH);
                            if (pl && pl < MAX_PATH)
                            {
                                envstr.replace(ii, ll + 2, wsptr(b, pl));
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

	void  __build_full_path(wstr_c &path)
	{
        swstr_t<8> disk;

		if (path.get_length() > 1 && (path.get_char(1) == ':'))
		{
			disk.set(path.substr(0,2));
			path.cut(0,2);
		}

		if (!__is_slash(path.get_char(0)))
		{
			swstr_t<2048> buf;
			if ( disk.is_empty() )
			{
				DWORD len = GetCurrentDirectoryW(2048-8, buf.str());
				buf.set_length(len);
                __append_slash_if_not(buf);
				path.insert(0,buf);
			} else
			{
				DWORD len =GetFullPathNameW( disk, 2048-8, buf.str(), nullptr );
				buf.set_length(len);
                __append_slash_if_not(buf);
				path.insert(0,buf);
			}

		} else
			path.insert(0,disk);
	}

    void __remove_crap(wstr_c &path)
    {
        int prev_prev_slash = path.get_last_char() == '.' ? path.get_length() : -1;
        int prev_slash = prev_prev_slash;
        int cch = path.get_length() - 1;
        int to = -1;
        int doubledot = 0;
        if ( path.begins(CONSTWSTR("\\\\")) ) to = path.find_pos_of(2, CONSTWSTR("/\\")); // UNC path
        for(;cch >= to;--cch)
        {
            wchar c = cch >= 0 ? path.get_char(cch) : NATIVE_SLASH;
            if ( __is_slash(c) )
            {
                if (prev_slash - 1 == cch)
                {
                    if (cch < 0) break;

                    // remove double slash
                    if (c != ENEMY_SLASH || path.get_char(prev_slash) == ENEMY_SLASH)
                    {
                        ASSERT(prev_slash < path.get_length());
                        path.cut(prev_slash);
                    } else
                        path.cut(cch);
                    --prev_prev_slash;
                    prev_slash = cch;
                    continue;
                }
                if (prev_slash - 2 == cch && path.get_char(cch + 1) == '.')
                {
                    // remove /./

                    if (prev_slash < path.get_length() && (c != ENEMY_SLASH || path.get_char(prev_slash) == ENEMY_SLASH))
                        path.cut(prev_slash-1,2);
                    else
                        path.cut(cch,2);
                    prev_prev_slash -= 2;
                    prev_slash = cch;
                    continue;
                }
                if (prev_prev_slash - 3 == prev_slash && prev_slash > cch && path.get_char(prev_prev_slash-1) == '.' && path.get_char(prev_prev_slash-2) == '.')
                {
                    // remove /subfolder/..

                    if (prev_slash - 3 == cch && path.get_char(prev_slash-1) == '.' && path.get_char(prev_slash-2) == '.')
                    {
                        ++doubledot;
                        prev_prev_slash = prev_slash;
                        prev_slash = cch;
                        continue;
                    }

                    int n = prev_prev_slash - cch;
                    if (prev_prev_slash < path.get_length() && (c != ENEMY_SLASH || path.get_char(prev_prev_slash) == ENEMY_SLASH))
                        path.cut(cch + 1, n);
                    else
                        path.cut(cch, n);
                    prev_prev_slash = cch;
                    prev_slash = cch;
                    if (doubledot)
                    {
                        --doubledot;
                        prev_prev_slash += 3; // "/../"
                        ASSERT(__is_slash(path.get_char(prev_prev_slash)) && path.get_char(prev_prev_slash-1) == '.' && path.get_char(prev_prev_slash-2) == '.');
                    }
                    continue;
                }

                prev_prev_slash = prev_slash;
                prev_slash = cch;
            }

        }
    }

	void  TSCALL fix_path(wstr_c &path, int fnoptions)
	{
        if ( 0 != (FNO_TRIMLASTSLASH & fnoptions) && __ending_slash(path) )
            path.trunc_length(1);

        if (0 != (FNO_APPENDSLASH & fnoptions) && !__ending_slash(path))
            path.append_char(NATIVE_SLASH);

        if ( FNO_NORMALIZE & fnoptions )
            path.replace_all(ENEMY_SLASH, NATIVE_SLASH);

        if ( FNO_PARSENENV & fnoptions)
            parse_env(path);

        if ( FNO_FULLPATH & fnoptions )
            __build_full_path( path );

        if ( FNO_REMOVECRAP & fnoptions )
            __remove_crap(path);

#ifdef _WIN32
        if (FNO_LOWERCASEAUTO & fnoptions)
            path.case_down();
#endif // _WIN32

        if ( FNO_MAKECORRECTNAME & fnoptions )
        {
            wsptr badchars = CONSTWSTR("\\/?*|<>:");
            aint cnt = path.get_length();
            for(int i=0;i<cnt;++i)
                if ( pwstr_c(badchars).find_pos( path.get_char(i) ) >= 0 )
                    path.set_char(i,'_');
        }

	}

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

	void    TSCALL copy_dir(const wstr_c &path_from, const wstr_c &path_clone, const wsptr& skip)
	{
		wstrings_c files;
		wstrings_c dirs;

		fill_dirs_and_files( path_from, files, dirs );

		int pfl = path_from.get_length();
		if (__ending_slash(path_from)) --pfl;

		int pfc = path_clone.get_length();
		if (__ending_slash(path_clone)) --pfc;

		for (aint i=dirs.size()-1;i>=0;--i)
		{
            bool rm = false;
            token<wchar> t(skip, ';');
            for(;t;++t)
            {
                const wstr_c &d = dirs.get(i);
                int x = d.find_pos( *t );
                if (x >= 0 || __is_slash(d.get_char(x-1)))
                    if (x + t->get_length() == d.get_length() || __is_slash(d.get_char(x + t->get_length())))
                    {
                        rm = true;
                        break;
                    }
            }

			if (rm)
			{
				dirs.remove_fast(i);
				continue;
			}

			dirs.get(i).replace( 0, pfl, wsptr(path_clone.cstr(), pfc) );
		}
		dirs.sort(true);
		
        for (const auto & s : dirs)
			make_path( s, 0 );

		wstr_c t,tt;
		for (aint i=files.size()-1;i>=0;--i)
		{
            bool rm = false;
            t = files.get(i);
            token<wchar> tok(skip, ';');
            for (; tok; ++tok)
            {
                int x = t.find_pos(*tok);
                if (x >= 0 || __is_slash(t.get_char(x - 1)))
                    if (x + tok->get_length() == t.get_length() || __is_slash(t.get_char(x + tok->get_length())))
                    {
                        rm = true;
                        break;
                    }
            }
            if (rm) continue;

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
            ts::kill_file(s);

		for (const auto & s : dirs)
		{
			SetFileAttributesW(s,FILE_ATTRIBUTE_NORMAL); WINDOWS_ONLY
			RemoveDirectoryW(s); WINDOWS_ONLY
		}
	}

	bool    TSCALL make_path(const wstr_c &ipath, int fnoptions)
	{
		wstr_c path(ipath);
        fix_path(path, fnoptions | FNO_NORMALIZE | FNO_REMOVECRAP | FNO_TRIMLASTSLASH | FNO_FULLPATH);

		wstrings_c sa(path, NATIVE_SLASH);
		path.set_length(0);

		for (int i=ipath.begins(CONSTWSTR("\\\\")) ? (path.set(CONSTWSTR("\\\\")).append(sa.get(2)).append_char(NATIVE_SLASH), 3) : 0; i<sa.size(); ++i)
		{
			path.append(sa.get(i));
			if (i == 0)
			{
				ASSERT( path.get_last_char() == ':' );
				path.append_char(NATIVE_SLASH);
				continue;
			}

			if (!PathFileExistsW(path.cstr()))
				if (0 == CreateDirectoryW(path.cstr(), nullptr)) 
                    return false;

            path.append_char(NATIVE_SLASH);
		}
        return true;
	}

    copy_rslt_e TSCALL copy_file( const wsptr &existingfn, const wsptr &newfn )
    {
        if ( 0 == CopyFileW( ts::tmp_wstr_c( existingfn ), ts::tmp_wstr_c( newfn ), false ) )
        {
            if ( ERROR_ACCESS_DENIED == GetLastError() )
                return CRSLT_ACCESS_DENIED;
            return CRSLT_FAIL;
        }
        return CRSLT_OK;
    }
    bool TSCALL rename_file( const wsptr &existingfn, const wsptr &newfn )
    {
        return MoveFileW( tmp_wstr_c(existingfn), tmp_wstr_c( newfn ) ) != 0;
    }

    template <class TCHARACTER> bool TSCALL parse_text_file(const wsptr &filename, strings_t<TCHARACTER>& text, bool b_from_utf8)
    {
        buf_c buffer;
        text.clear();
        if (buffer.load_from_file(filename))
        {
            if (buffer.size() > 0)
            {
                if (buffer.size() >= 3)
                {
                    if (*buffer.data() == 0xEF && buffer.data()[1] == 0xBB && buffer.data()[2] == 0xBF) // utf8 BOM
                    {
                        text.split<wchar>(from_utf8(buffer.cstr().skip(3)), '\n');
                        return true;
                    }
                }
                if (buffer.size() >= 2)
                {
                    // TODO : is this stuff correct for big-endian architecture?

                    if (*buffer.data16() == 0xFEFF)
                    {
                        // native ucs16, no need swap bytes
                        text.split(wsptr((const wchar*)(buffer.data() + 2), (int)((buffer.size() - 2) / sizeof(wchar))), (wchar)'\n');
                        return true;
                    }
                    if (*buffer.data16() == 0xFFFE)
                    {
                        // alien ucs16 - swap bytes required
                        const wchar * endofbuf = (const wchar *)(buffer.data() + buffer.size());
                        for (const wchar *b = (const wchar *)buffer.data16(); b < endofbuf; ++b)
                            SWAP(*(uint8 *)b, *(((uint8 *)b) + 1));
                        text.split(wsptr((const wchar*)(buffer.data() + 2), (int)((buffer.size() - 2) / sizeof(wchar))), (wchar)'\n');
                        return true;
                    }
                }

                if (b_from_utf8)
                    text.split<wchar>( from_utf8(buffer.cstr()), '\n' );
                else
                    text.split<char>(buffer.cstr(), '\n');
                return true;
            }
        }
        return false;
    }
    template bool TSCALL parse_text_file<char>(const wsptr &filename, strings_t<char>& text, bool);
    template bool TSCALL parse_text_file<wchar>(const wsptr &filename, strings_t<wchar>& text, bool);

	void TSCALL fn_split( const wsptr &full_name, wstr_c &name, wstr_c &ext )
	{
		int i = pwstr_c(full_name).find_last_pos_of(CONSTWSTR("/\\"));
		if (i < 0) i = 0; else ++i;
		int j = pwstr_c(full_name).find_last_pos('.');
		if (j < i)
		{
			name.set(full_name.skip(i));
			ext.clear();
		} else
		{
			name.set(wsptr(full_name.s + i, j - i));
			ext.set(full_name.skip(j + 1));
		}
	}

	wstr_c  TSCALL fn_get_next_name(const wsptr &fullname, bool check)
	{
		if (check)
		{
			wstr_c n = fn_get_next_name(fullname,false);

			WIN32_FIND_DATAW fd;

			for(;;)
			{
				HANDLE h = FindFirstFileW(to_wstr(n),&fd);
				if ( INVALID_HANDLE_VALUE == h ) return n;
				FindClose(h);
				n = fn_get_next_name(n,false);
			}
		}

		wstr_c name = fn_get_name(fullname);
		int index = name.get_length() - 1;
		for (; index >= 0 && CHAR_is_digit(name.get_char(index)) ;--index);
		uint nn = name.substr(++index).as_uint() + 1;
		name.replace(index,name.get_length()-index, wstr_c().set_as_uint(nn));
		return fn_change_name(fullname,name);

	}

	template<typename TCHARACTER> str_t<TCHARACTER>  TSCALL fn_autoquote(const str_t<TCHARACTER> &name)
	{
		if (name.find_pos(' ') >= 0 && name.get_char(0) != '\"' && name.get_last_char() != '\"')
			return str_t<TCHARACTER>().append_char('\"').append(name).append_char('\"');
		return name;
	}

	template str_c  TSCALL fn_autoquote<char>(const str_c &name);
	template wstr_c  TSCALL fn_autoquote<wchar>(const wstr_c &name);

	wstr_c  TSCALL fn_get_name(const wsptr &name_)
	{
        pwstr_c name(name_);
		int i = name.find_last_pos_of(CONSTWSTR("/\\"));
		if (i < 0) i = 0; else ++i;
		int j = name.find_last_pos('.');
		if (j < i) j = name.get_length();
		return wstr_c(name_.s + i, j - i);
	}

	wstr_c  TSCALL fn_get_ext(const wsptr &name_)
	{
        pwstr_c name(name_);
		int i = name.find_last_pos_of(CONSTWSTR("/\\"));
		int j = name.find_last_pos('.');
		if (j <= i) j = name.get_length(); else ++j;
		return wstr_c(name_.skip(j));
	}

	wstr_c TSCALL fn_get_path(const wstr_c &name) // with ending slash
	{
		int i = name.find_last_pos_of(CONSTWSTR("/\\"));
		if (i<0) return name;
		return wstr_c(name.cstr(), i + 1);
	}

#define FROMCS "àáâãäå¸æçèéêëìíîïðñòóôõö÷øùúûüýþÿÀÁÂÃÄÅ¨ÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß"; ///\\.,;!@#$%^&*()[]|`~{}-=<>\"\'"
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
		aint cnt = name.get_length();
		bool already_with_ext = false;
		if (ext && name.ends_ignore_case(ext))
		{
			already_with_ext = true;
			cnt -= CHARz_len(ext);
		}
		for (int i=0;i<cnt;++i)
		{
			aint ii = CHARz_find( _fromc(ext), name.get_char(i) );
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

	wstr_c TSCALL fn_change_name(const wsptr &full_, const wsptr &name)
	{
        pwstr_c full(full_);
		int i = full.find_last_pos_of(CONSTWSTR("/\\"));
		if (i < 0) i = 0; else ++i;
		int j = full.find_last_pos('.');
		if (j < i) j = full.get_length();
		return wstr_c(full_.part(i)).append(name).append(full.as_sptr().skip(j));
	}

	wstr_c  TSCALL fn_change_ext(const wsptr &full_, const wsptr &ext)
	{
        pwstr_c full(full_);
		int i = full.find_last_pos_of(CONSTWSTR("/\\"));
		int j = full.find_last_pos('.');
		if (j <= i) j = full.get_length();
		return wstr_c(full_.part(j)).append_char('.').append(ext);
	}

    bool TSCALL fn_mask_match( const wsptr &fn, const wsptr &mask )
    {
        if (pwstr_c(mask).equals(CONSTWSTR("*.*")) || pwstr_c(mask).equals(CONSTWSTR("*")))
            return true;

        int index = 0;
        token<wchar> mp(mask, '*');
        bool left = true;
        for(;mp;++mp)
        {
            int i = mp->get_length() == 0 ? index : pwstr_c(fn).find_pos_t(index, *mp);
            if (i < 0) return false;
            if (left && i != 0) return false;
            left = false;
            index = i + mp->get_length();
        }

        return index == fn.l;
    }

    bool TSCALL is_file_exists(const wsptr &fname)
    {
        return INVALID_FILE_ATTRIBUTES != GetFileAttributesW(tmp_wstr_c(fname));
    }

	bool    TSCALL is_file_exists(const wsptr &iroot, const wsptr &fname)
	{
		wstr_c path(iroot);
		if (path.is_empty()) return false;
		wchar lch = path.get_last_char();
		if (lch != NATIVE_SLASH && lch != ENEMY_SLASH) path.append_char(NATIVE_SLASH);

		wstrings_c sa;

        wstr_c new_path(path);
        int lclip = new_path.get_length();

		if (find_files(new_path.set_length(lclip).append(fname), sa, ATTR_ANY, ATTR_DIR )) return true;
		if (find_files(new_path.set_length(lclip).append(CONSTWSTR("*.*")), sa, ATTR_DIR ))
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

	bool    TSCALL dir_present(const wstr_c &path)
	{
		WIN32_FIND_DATAW find_data;
		wstr_c p(path);

		if (__ending_slash(p)) p.trunc_length();
		if ( p.get_length() == 2 && p.get_last_char() == ':' )
		{
			UINT r = GetDriveTypeW( p.append_char(NATIVE_SLASH) );
			return (r != DRIVE_NO_ROOT_DIR);
		}

		HANDLE fh = FindFirstFileW(p, &find_data);
		if ( fh == INVALID_HANDLE_VALUE ) return false;
		FindClose( fh );

		return (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	}

    DWORD cvta( int a )
    {
        if ( a < 0 ) return 0xffffffff;
        if ( a & ATTR_DIR ) return FILE_ATTRIBUTE_DIRECTORY;
        return 0;
    }

	bool    TSCALL find_files(const wsptr &wildcard, wstrings_c &files, int attributes, int skip_attributes, bool full_names)
	{
		WIN32_FIND_DATAW find_data;
		HANDLE           fh = FindFirstFileW(tmp_wstr_c(wildcard), &find_data);
		bool             bResult = false;
        swstr_t<MAX_PATH> path;
        int pl = 0;
        if (full_names)
        {
            int i = pwstr_c(wildcard).find_last_pos_of(CONSTWSTR("/\\"));
            if (ASSERT(i >= 0))
            {
                path.set(pwstr_c(wildcard).substr(0, i + 1));
                pl = i+1;
            }

        }

        DWORD dwFileAttributes = cvta( attributes );
        DWORD dwSkipAttributes = cvta( skip_attributes );

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

    bool TSCALL check_write_access(const wsptr &ipath)
    {
        wstr_c path(ipath);
        fix_path(path, FNO_NORMALIZE | FNO_REMOVECRAP | FNO_TRIMLASTSLASH | FNO_FULLPATH);

        wstrings_c sa(path, NATIVE_SLASH);
        path.set_length(0);

        
        for (int i = pwstr_c(ipath).begins(CONSTWSTR("\\\\")) ? (path.set(CONSTWSTR("\\\\")).append(sa.get(2)).append_char(NATIVE_SLASH), 3) : 0; i < sa.size(); ++i)
        {
            ts::wstr_c prevpath = path;
            path.append(sa.get(i));
            if (i == 0)
            {
                ASSERT(path.get_last_char() == ':');
                path.append_char(NATIVE_SLASH);
                continue;
            }

            if (!PathFileExistsW(path))
            {
                path = prevpath;
                break;
            }

            path.append_char(NATIVE_SLASH);
        }

        HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (INVALID_HANDLE_VALUE == h)
            return false;
        CloseHandle(h);
        return true;

    }


    bool TSCALL kill_file(const wsptr &path)
    {
        WINDOWS_ONLY
        ts::swstr_t< MAX_PATH + 16 > p(path);
        SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
        return DeleteFileW(p) != 0;
    }

#ifdef _WIN32

    ts::wstr_c f_create( const ts::wsptr&fn )
    {
        HANDLE f = CreateFileW( tmp_wstr_c(fn), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
        if ( f == INVALID_HANDLE_VALUE )
        {
            ts::wstr_c errs( 1024, true );
            DWORD dw = GetLastError();

            FormatMessageW(
                /*FORMAT_MESSAGE_ALLOCATE_BUFFER |*/
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dw,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                errs.str(),
                1023, NULL );
            errs.set_length();
            return errs;
        }
        CloseHandle( f );
        return ts::wstr_c();
    }

    void *f_open( const ts::wsptr&fn )
    {
        void *h = CreateFileW( tmp_wstr_c( fn ), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
        if ( h != INVALID_HANDLE_VALUE )
            return h;
        return nullptr;
    }
    void *f_recreate( const ts::wsptr&fn )
    {
        void *h = CreateFileW( tmp_wstr_c( fn ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
        if ( h != INVALID_HANDLE_VALUE )
            return h;
        return nullptr;
    }

    void *f_continue( const ts::wsptr&fn )
    {
        void * h = CreateFileW( tmp_wstr_c( fn ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
        if ( h != INVALID_HANDLE_VALUE )
            return h;
        return nullptr;
    }

    bool f_set_pos( void *h, uint64 pos )
    {
        LARGE_INTEGER li;
        li.QuadPart = pos;
        return INVALID_SET_FILE_POINTER != SetFilePointer( h, li.LowPart, &li.HighPart, FILE_BEGIN );
    }
    uint64 f_get_pos( void *h )
    {
        LARGE_INTEGER li = {0};
        li.LowPart = SetFilePointer( h, 0, &li.HighPart, FILE_CURRENT );
        return li.QuadPart;
    }

    uint64 f_size( void *h )
    {
        LARGE_INTEGER li;
        li.LowPart = GetFileSize( h, (LPDWORD)&li.HighPart );
        return li.QuadPart;
    }
    aint f_read( void *h, void *ptr, aint sz )
    {
        DWORD r;
        ReadFile( h, ptr, (DWORD)sz, &r, nullptr );
        return r;
    }
    aint f_write( void *h, const void *ptr, aint sz )
    {
        DWORD w;
        WriteFile( h, ptr, (DWORD)sz, &w, nullptr );
        return w ;
    }
    void f_close( void *h )
    {
        CloseHandle( h );
    }

    uint64 f_time_last_write( void *h )
    {
        FILETIME ft;
        GetFileTime( h, nullptr, nullptr, &ft );
        return ref_cast<uint64>(ft);
    }
    struct efd_s
    {
        WIN32_FIND_DATAW fd;
        HANDLE h;
        ts::wstr_c base;
        ts::wstr_c path;
        ts::wstr_c wildcard;
        ts::wstr_c fn;
        bool ready;
        bool folder;
    };

    enum_files_c::enum_files_c( const wstr_c &base, const wstr_c &path, const wstr_c &wildcard )
    {
        TS_STATIC_CHECK( sizeof(efd_s) <= sizeof(data), "bad size" );
        memset( data, 0, sizeof( efd_s ) );
        efd_s &d = (efd_s &)data;

        TSPLACENEW( &d.base );
        TSPLACENEW( &d.path );
        TSPLACENEW( &d.wildcard );
        TSPLACENEW( &d.fn );

        d.h = FindFirstFileW( fn_join( base, path, wildcard ), &d.fd );
        if ( d.h == INVALID_HANDLE_VALUE )
        {
            d.ready = false;
            d.folder = false;
        } else
        {
            d.ready = true;
            d.folder = false;
            for ( ; d.ready && !prepare_file(); )
                next_int();
        }

    }
    enum_files_c::~enum_files_c()
    {
        efd_s &d = (efd_s &)data;
        d.base.~wstr_c();
        d.path.~wstr_c();
        d.wildcard.~wstr_c();
        d.fn.~wstr_c();
        FindClose( d.h );
    }
    enum_files_c::operator bool() const
    {
        efd_s &d = (efd_s &)data;
        return d.ready;
    }

    const wstr_c &enum_files_c::operator* () const
    {
        efd_s &d = (efd_s &)data;
        return  d.fn;
    }
    const wstr_c *enum_files_c::operator->() const 
    {
        efd_s &d = (efd_s &)data;
        return &d.fn;
    }

    bool enum_files_c::is_folder() const
    {
        const efd_s &d = (const efd_s &)data;
        return d.folder;
    }

    bool enum_files_c::prepare_file()
    {
        efd_s &d = (efd_s &)data;
        wstr_c sFileName( d.fd.cFileName );

        if ( d.fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN )
            return false;

        if ( d.fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
            if ( sFileName == CONSTWSTR( "." ) || sFileName == CONSTWSTR( ".." ) )
                return false;


            //if ( !enum_files( base, pred, fn_join( path, sFileName ), wildcard ) ) { FindClose( h ); return false; }
            d.folder = true;
        } else
        {
            d.folder = false;
        }

        d.fn = fn_join( d.path, sFileName );
        return true;
    }

    void enum_files_c::next_int()
    {
        efd_s &d = (efd_s &)data;
        if ( !d.ready ) return;

        d.ready = FindNextFileW( d.h, &d.fd ) != 0;
    }
    void enum_files_c::next()
    {
        efd_s &d = (efd_s &)data;
        if ( !d.ready ) return;

        do  { next_int(); } while ( d.ready && !prepare_file() );
    }
#endif


    bool TSCALL is_64bit_os()
    {
#ifdef MODE64
        return true;
#else
#ifdef _WIN32
        BOOL bIsWow64 = FALSE;

        //IsWow64Process is not available on all supported versions of Windows.
        //Use GetModuleHandle to get a handle to the DLL that contains the function
        //and GetProcAddress to get a pointer to the function if available.

        typedef BOOL ( WINAPI *LPFN_ISWOW64PROCESS ) ( HANDLE, PBOOL );
        LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress( GetModuleHandleW( L"kernel32" ), "IsWow64Process" );

        if ( nullptr != fnIsWow64Process )
        {
            if ( !fnIsWow64Process( GetCurrentProcess(), &bIsWow64 ) )
            {
                return false;
            }
        }
        return bIsWow64 != FALSE;
#endif
#ifdef __linux__
        UNFINISHED( "check 32 or 64 bit linux" );
        return false;
#endif
#endif

    }


} //namespace ts