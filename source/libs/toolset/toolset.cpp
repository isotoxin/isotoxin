#include "toolset.h"
#include "internal/platform.h"
#include "spinlock/spinlock.h"

#ifdef _WIN32
#ifndef _FINAL
namespace spinlock
{
    unsigned long pthread_self()
    {
        return GetCurrentThreadId();
    }
}
#endif // _FINAL
#endif

#ifdef _DEBUG
int THREADLOCAL g_current_memt = 0;
#endif // _DEBUG

namespace ts
{
    tmpalloc_c::tmpalloc_c()
    {
        threadid = spinlock::pthread_self();
        ASSERT( core == nullptr );
        core = this;
    }
    tmpalloc_c::~tmpalloc_c()
    {
        ASSERT( core == this );
        core = nullptr;
    }
    tmpbuf_s *tmpalloc_c::get()
    {
        ASSERT( core, "create tmpalloc_c object at begin of your thread proc" );
        ASSERT( spinlock::pthread_self() == core->threadid );
        for ( tmpbuf_s&b : core->bufs )
        {
            if ( !b.used )
            {
                b.used = true;
                return &b;
            }
        }
        return TSNEW( tmpbuf_s, true );
    }
    void tmpalloc_c::unget( tmpbuf_s * b )
    {
        ASSERT( spinlock::pthread_self() == core->threadid );
        if ( b->extra )
            TSDEL( b );
        else
        {
            ASSERT( b->used );
            b->used = false;
        }
    }


THREADLOCAL tmpalloc_c *tmpalloc_c::core = nullptr;

tmpalloc_c tmpb;

    blob_c tsfileop_c::load(const wstrings_c &paths, const wsptr &fn)
    {
        blob_c b;
        for ( const wstr_c &trypath : paths )
        {
            b = load( fn_join(trypath, fn) );
            if (b) return b;
        }
        return b;
    }

    blob_c tsfileop_c::load(const wsptr &fn)
    {
        struct bw : public buf_wrapper_s
        {
            blob_c b;
            /*virtual*/ void * alloc(aint sz) override
            {
                b.set_size(sz, false);
                return b.data();
            }
        } me;
        if ( this->read(fn, me) )
            return me.b;
        return blob_c();
    }

	bool tsfileop_c::load(const wsptr &fn, buf_wrapper_s &b, size_t reservebefore, size_t reserveafter)
	{
#if defined _MSC_VER
#pragma warning(push)
#pragma warning(disable:4822) //: 'ts::tsfileop_c::load::bw::bw' : local class member function does not have a body
#endif
        struct bw : public buf_wrapper_s
        {
            buf_wrapper_s &b;
            size_t reservebefore;
            size_t reserveafter;
            /*virtual*/ void * alloc(aint sz) override
            {
                char * a = (char *)b.alloc(sz + reservebefore + reserveafter);
                return a + reservebefore;
            }
            bw(buf_wrapper_s &b, size_t reservebefore, size_t reserveafter):b(b), reservebefore(reservebefore), reserveafter(reserveafter) {}
        private:
            bw(const bw&) UNUSED;
            bw &operator=(const bw&) UNUSED;
        } me(b,reservebefore,reserveafter);
#if defined _MSC_VER
#pragma warning(pop)
#endif

        return this->read(fn, me);
	}
	bool tsfileop_c::load(const wsptr &fn, abp_c &bp)
	{
        struct bw : public buf_wrapper_s
        {
            tmp_buf_c b;
            /*virtual*/ void * alloc(aint sz) override
            {
                b.set_size(sz, false);
                return b.data();
            }
        } me;
        if ( this->read(fn, me) )
        {
            bp.load(me.b.cstr());
            return true;
        }
		return false;
	}

	class tsfileop_def_c : public tsfileop_c
	{
	public:
		tsfileop_def_c() 
		{
		}

		virtual ~tsfileop_def_c() {}
        /*virtual*/ bool read(const wsptr &fn, buf_wrapper_s &b) override
		{
            void *f = f_open(fn);
			if (!f) return false;
            aint sz = (aint)f_size(f);
            bool rslt = f_read( f, b.alloc( sz ), sz ) == sz;
            f_close(f);
            return rslt;
		}
		/*virtual*/ bool size(const wsptr &fn, size_t &sz) override
		{
            void *f = f_open( fn );
            if (!f) return false;
            sz = (size_t)f_size(f);
            f_close( f );
			return true;;
		}

        /*virtual*/ void find(wstrings_c & files, const wsptr &fnmask, bool full_paths ) override
        {
            int x = pwstr_c(fnmask).find_pos(CONSTWSTR("/*"));
            bool hm = x >= 0 && x+2 < fnmask.l;
            if (hm && fnmask.s[x+2] == '/')
            {
                wstrings_c folders;
                find_files(pwstr_c(fnmask).substr(0, x + 2), folders, ATTR_DIR );
                for( const wstr_c &f : folders )
                {
                    if (f.equals(CONSTWSTR(".")) || f.equals(CONSTWSTR(".."))) continue;
                    find( files, ts::wstr_c(pwstr_c(fnmask).substr(0, x + 1),f,pwstr_c(fnmask).substr(x+2)), full_paths);
                }
            } else if (hm && fnmask.s[x+2] == '*' && x+3 < fnmask.l && fnmask.s[x+3] == '/')
            {
                wstrings_c folders;
                find_files(pwstr_c(fnmask).substr(0, x + 2), folders, ATTR_DIR );
                for (const wstr_c &f : folders)
                {
                    if (f.equals(CONSTWSTR(".")) || f.equals(CONSTWSTR(".."))) continue;
                    wstr_c curf(fnmask); curf.insert(x,CONSTWSTR("/") + f);
                    find(files, curf, full_paths);
                }

                wstr_c curf(fnmask); curf.cut(x, 3);
                find_files(curf, files, ATTR_ANY, ATTR_DIR, full_paths); //-V112

            } else
                find_files(fnmask, files, ATTR_ANY, ATTR_DIR, full_paths); //-V112
        }
	};



	sobase *sobase::first = nullptr;
    bool sobase::initialized = false;
    bool sobase::initializing = false;

	tsfileop_c *g_fileop;

	struct fileop_init
	{
		fileop_init()
		{
            MEMT( MEMT_FILEOP );
			g_fileop = TSNEW( tsfileop_def_c );
		}
		~fileop_init()
		{
			TSDEL( g_fileop );
			g_fileop = nullptr;
		}

	};
	fileop_init __fopinit; // do not use static_setup due initialization

	void tsfileop_c::killop()
	{
		TSDEL( g_fileop );
		g_fileop = nullptr;
	}

    bool fileop_load( const wsptr &fn, buf_wrapper_s &b, size_t reservebefore, size_t reserveafter )
    {
        return g_fileop->load(fn, b, reservebefore, reserveafter);
    }


} // namespace ts

#ifdef _NIX
#include "win32emu/win32emu.inl"
#endif
