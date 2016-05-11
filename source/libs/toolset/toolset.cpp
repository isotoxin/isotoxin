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

    bool fileop_load( const wsptr &fn, buf_wrapper_s &b, size_t reservebefore, size_t reserveafter )
    {
        return g_fileop->load(fn, b, reservebefore, reserveafter);
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
            spinlock::long3264 lock = 0;
        } internals;
    
        spinlock::simple_lock( internals.lock );

        if ( internals.src.volumesn == 0 )
        {
            // 1st initialization

            buf_c b;

#ifdef _WIN32
            char cn[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD cnSize = ARRAY_SIZE(cn);
            memset(cn, 0, sizeof(cn));
            GetComputerNameA(cn, &cnSize);


            tmp_tbuf_t<IP_ADAPTER_INFO> adapters;
            adapters.set_size(sizeof(IP_ADAPTER_INFO) + 64);
            adapters.tbegin<IP_ADAPTER_INFO>()->Next = nullptr; //-V807
            memcpy(adapters.tbegin<IP_ADAPTER_INFO>()->Address, cn, 6);//-V512 - mac address length is actually 6 bytes


            ULONG sz = (ULONG)adapters.byte_size();
            while (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(adapters.tbegin<IP_ADAPTER_INFO>(), &sz))
            {
                adapters.set_size(sz, false);
                adapters.tbegin<IP_ADAPTER_INFO>()->Next = nullptr;
                memcpy(adapters.tbegin<IP_ADAPTER_INFO>()->Address, cn, 6);//-V512
            }

            const IP_ADAPTER_INFO *infos = adapters.tbegin<IP_ADAPTER_INFO>();
            do
            {
                b.append_buf(infos->Address, 6);
                infos = infos->Next;
            } while (infos);

            GetVolumeInformationA("c:\\", nullptr, 0, &internals.src.volumesn, nullptr, nullptr, nullptr, 0);
#endif
#ifdef __linux__
            bool ok = false;
 
            int s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s != -1)
            {
                struct ifreq ifr;
                struct ifreq *IFR;
                struct ifconf ifc;
                char buf[1024];

                ifc.ifc_len = sizeof(buf);
                ifc.ifc_buf = buf;
                ioctl(s, SIOCGIFCONF, &ifc);
 
                IFR = ifc.ifc_req;
                for (int i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; IFR++)
                {
                    strcpy(ifr.ifr_name, IFR->ifr_name);
                    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
                    {
                        if (! (ifr.ifr_flags & IFF_LOOPBACK))
                        {
                            if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0)
                            {
                                ok = 1;
                                break;
                            }
                        }
                    }
                }
             
                shutdown(s, SHUT_RDWR);
                if (ok)
                {
                    b.append_buf(ifr.ifr_hwaddr.sa_data, 6);
                }
            }
            if (!ok)
            {
                char buf[6] = {0};
                b.append_buf(buf, 6);
            }
#endif

            md5_c md5;
            md5.update(b.data(), b.size());
            md5.done(internals.src.machash);
        }

#ifdef _WIN32
        QueryPerformanceCounter( &ref_cast<LARGE_INTEGER>(internals.src.timestamp) );
#endif
#ifdef __linux__
        timespec tspc;
        clock_gettime(CLOCK_REALTIME,&tspc);
        internals.src.timestamp = (((uint64)tspc.tv_sec) << 32) | tspc.tv_nsec;
#endif

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
        spinlock::simple_unlock( internals.lock );
        return r;
    }

} // namespace ts