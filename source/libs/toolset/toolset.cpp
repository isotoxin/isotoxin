#include "toolset.h"
#include "internal/platform.h"
#include "spinlock/spinlock.h"

#ifndef _FINAL
namespace spinlock
{
    unsigned long pthread_self()
    {
        return GetCurrentThreadId();
    }
}
#endif // INLINE_PTHREAD_SELF

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


__declspec(thread) tmpalloc_c *tmpalloc_c::core = nullptr;

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
#pragma warning(push)
#pragma warning(disable:4822) //: 'ts::tsfileop_c::load::bw::bw' : local class member function does not have a body
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
#pragma warning(pop)

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
			HANDLE f = CreateFileW( tmp_wstr_c(fn), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
			if (f == INVALID_HANDLE_VALUE)
				return false;
            aint sz = GetFileSize(f, nullptr);
            void *d = b.alloc(sz);
            DWORD r;
            ReadFile(f, d, sz, &r, nullptr);
            CloseHandle(f);
            return (aint)r == sz;
		}
		/*virtual*/ bool size(const wsptr &fn, size_t &sz) override
		{
            HANDLE f = CreateFileW(tmp_wstr_c(fn), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f == INVALID_HANDLE_VALUE)
                return false;
            sz = GetFileSize(f, nullptr);
            CloseHandle(f);
			return true;;
		}

        /*virtual*/ void find(wstrings_c & files, const wsptr &fnmask, bool full_paths ) override
        {
            aint x = pwstr_c(fnmask).find_pos(CONSTWSTR("/*"));
            bool hm = x >= 0 && x+2 < fnmask.l;
            if (hm && fnmask.s[x+2] == '/')
            {
                wstrings_c folders;
                find_files(pwstr_c(fnmask).substr(0, x + 2), folders, ATTR_DIR );
                for( const wstr_c &f : folders )
                {
                    if (f.equals(CONSTWSTR(".")) || f.equals(CONSTWSTR(".."))) continue;
                    find( files, ts::wstr_c(pwstr_c(fnmask).substr(0, x + 1)).append(f).append(pwstr_c(fnmask).substr(x+2)), full_paths);
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



    uint64 uuid()
    {
        static struct internals_
        {
            struct
            {
                uint8  machash[16];
                uint64 timestamp = 0;
                uint32 volumesn = 0;
                uint32 nonce = 0;
            } src;
            union
            {
                struct
                {
                    uint64 dummy;
                    uint64 uid;
                };
                uint8  rslt[16];
            };
            long lock = 0;
        } internals;
    
        simple_lock( internals.lock );

        if ( internals.src.volumesn == 0 )
        {
            // 1st initialization

            char cn[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD cnSize = LENGTH(cn);
            memset(cn, 0, sizeof(cn));
            GetComputerNameA(cn, &cnSize);


            tmp_tbuf_t<IP_ADAPTER_INFO> adapters;
            adapters.set_size(sizeof(IP_ADAPTER_INFO) + 64);
            adapters.tbegin<IP_ADAPTER_INFO>()->Next = nullptr; //-V807
            memcpy(adapters.tbegin<IP_ADAPTER_INFO>()->Address, cn, 6);//-V512 - mac address length is actually 6 bytes


            ULONG sz = adapters.byte_size();
            while (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(adapters.tbegin<IP_ADAPTER_INFO>(), &sz))
            {
                adapters.set_size(sz, false);
                adapters.tbegin<IP_ADAPTER_INFO>()->Next = nullptr;
                memcpy(adapters.tbegin<IP_ADAPTER_INFO>()->Address, cn, 6);//-V512
            }

            buf_c b;

            const IP_ADAPTER_INFO *infos = adapters.tbegin<IP_ADAPTER_INFO>();
            do
            {
                b.append_buf(infos->Address, 6);
                infos = infos->Next;
            } while (infos);

            GetVolumeInformationA("c:\\", nullptr, 0, &internals.src.volumesn, nullptr, nullptr, nullptr, 0);

            md5_c md5;
            md5.update(b.data(), b.size());
            md5.done(internals.src.machash);
        }

        QueryPerformanceCounter( &ref_cast<LARGE_INTEGER>(internals.src.timestamp) );
        md5_c md5;
    again:
        md5.update(&internals, sizeof(internals.src));
        md5.done(internals.rslt);
        ++internals.src.nonce;
        uint64 r = internals.uid ^ internals.dummy;
        if (r == 0)
        {
            md5.reset();
            goto again;
        }
        simple_unlock( internals.lock );
        return r;
    }

} // namespace ts