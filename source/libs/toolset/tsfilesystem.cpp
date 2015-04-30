#include "toolset.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>

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
                            DWORD blen = b.get_capacity();
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

		for (int i=dirs.size()-1;i>=0;--i)
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
		for (int i=files.size()-1;i>=0;--i)
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

	void    TSCALL make_path(const wstr_c &ipath, int fnoptions)
	{
		wstr_c path(ipath);
        fix_path(path, fnoptions | FNO_NORMALIZE | FNO_REMOVECRAP | FNO_TRIMLASTSLASH);

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
			{
				if (0 == CreateDirectoryW(path.cstr(), nullptr)) return;
			}
			path.append_char(NATIVE_SLASH);
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

		wstr_c root(iroot);
        fix_path(root, FNO_FULLPATH);

		o.lStructSize=sizeof(o);
		o.hwndOwner=g_main_window;
		o.hInstance=GetModuleHandle(nullptr);

		wstr_c filter(filt);
		filter.replace_all(ENEMY_SLASH,0);

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

        wstr_c root(iroot);
        fix_path(root, FNO_FULLPATH);

		o.lStructSize=sizeof(o);
        o.hwndOwner = g_main_window;
        o.hInstance = GetModuleHandle(nullptr);

		wstr_c filter(filt);
		filter.replace_all(ENEMY_SLASH,0);

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

        wstr_c root(iroot);
        fix_path(root, FNO_FULLPATH);

		o.lStructSize=sizeof(o);
        o.hwndOwner = g_main_window;
        o.hInstance = GetModuleHandle(nullptr);

		wstr_c filter(filt);
		filter.replace_all(ENEMY_SLASH,0);

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
			files.add( wstr_c(buffer.cstr()).append_char(NATIVE_SLASH).append(zz) );
		}

		return true;
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