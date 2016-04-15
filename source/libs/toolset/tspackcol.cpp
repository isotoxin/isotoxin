#include "toolset.h"


extern "C"
{
void * __fastcall arc_mem_alloc( unsigned int size )
{
    return MM_ALLOC( size );
}

void * __fastcall arc_mem_realloc( void * p, unsigned int size )
{
    return MM_RESIZE( p, size );
}
void __fastcall arc_mem_free( void * p )
{
    MM_FREE( p );
}
};

namespace ts 
{

//***********************************************************
//***************** Packet File Collection ******************
//***********************************************************

void    ccollection_c::clear()
{
    close_containers();
    m_containers.clear(true);
}

uint  ccollection_c::add_container(const wsptr &name, int prior)
{
    for (container_c *pf : m_containers)
        if (pf->fn().equals(name)) return pf->get_id();
    
    tmp_wstr_c n(name);
    void * h = f_open(n);
    if (!h)
    {
        // pack in pack not supported, sorry
        //if ( !file_exists(n) )
            return 0;
    }

    container_c *pf = nullptr;
    blob_c body;

    body.set_size(64);
        
    if (64 != f_read(h, body.data(), body.size()))
        return 0;

    uint64 filetime = f_time_last_write(h);
    f_close(h);

    for(;;)
    {
        if ( zip_container_c::detect(body)) { pf = TSNEW(zip_container_c, name, prior, m_idpool); break; }
        break;
    }

    if (pf) 
    {
        pf->set_time_stamp(filetime);
        int ins_pos = -1;
        uint cnt = m_containers.size();
        for(uint i = 0; i < cnt; ++i)
        {
            int pr = m_containers.get(i)->get_priority();
            if (pr <= prior)
            {
                if ((pr < prior) || filetime > m_containers.get(i)->get_time_stamp())
                {
                    ins_pos = i;
                    break;
                }
            }
        }
        if (ins_pos >= 0) m_containers.insert(ins_pos, pf); else m_containers.add(pf);

		++m_idpool;

		return pf->get_id();
    }
	return 0;
}


void    ccollection_c::remove_container(uint container_id)
{
	int index;
	container_c *P = find(container_id, index);
	if (CHECK(P))
		m_containers.remove_slow(index);

}

bool ccollection_c::open_containers()
{
    bool ok = false;
    int i=0;
    for(;i<m_containers.size();)
    {
        if (!m_containers.get(i)->open())
        {
            m_containers.remove_slow(i);
            continue;
        }
        ok = true;
        ++i;
    }
    return ok;
}

bool  ccollection_c::close_containers()
{
    for (container_c *c : m_containers)
        c->close();
    return true;
}


bool  ccollection_c::file_exists(const wsptr &name)
{
    for (container_c *c : m_containers)
        if (c->file_exists(name)) return true;
    return false;
}

bool  ccollection_c::path_exists(const wsptr &path)
{
    for (container_c *c : m_containers)
        if (c->path_exists(path)) return true;
    return false;
}

void  ccollection_c::iterate_files(const wsptr &path, ITERATE_FILES_CALLBACK ef)
{
    for (container_c *c : m_containers)
        if (!c->iterate_files(path, ef))
			break;
}

void ccollection_c::iterate_folders(const wsptr &path, ITERATE_FILES_CALLBACK ef)
{
    for (container_c *c : m_containers)
        if (!c->iterate_folders(path, ef))
            break;
}

void  ccollection_c::iterate_all_files(ITERATE_FILES_CALLBACK ef)
{
    for(container_c *c : m_containers)
		if (!c->iterate_files(ef))
			break;
}

void ccollection_c::find_by_mask(wstrings_c & files, const ts::wsptr &fnmask, bool full_paths)
{
#pragma warning (disable:4822) // 'ts::ccollection_c::find_by_mask::middle::operator =' : local class member function does not have a body

    pwstr_c masks(fnmask);

    struct folders
    {
        wstrings_c m_folders;
        folders &operator=(const folders &)UNUSED;
        folders(const folders &) UNUSED;
        folders() {}
        bool filer(const wsptr & path, const wsptr & fn, container_c *)
        {
            m_folders.add(fn);
            return true;
        }
    };

    int x = masks.find_pos(CONSTWSTR("/*"));
    bool hm = x >= 0 && x+2 < fnmask.l;
    if (hm && fnmask.s[x+2] == '/')
    {
        folders fld;

        iterate_folders( pwstr_c(fnmask).substr(0, x), DELEGATE(&fld, filer) );
        for( const wstr_c &f : fld.m_folders )
        {
            find_by_mask( files, ts::wstr_c(pwstr_c(fnmask).substr(0, x + 1)).append(f).append(pwstr_c(fnmask).substr(x+2)), full_paths);
        }
        return;
    }

    wstr_c curf;

    if (hm && fnmask.s[x + 2] == '*' && x + 3 < fnmask.l && fnmask.s[x + 3] == '/')
    {
        folders fld;

        iterate_folders(pwstr_c(fnmask).substr(0, x), DELEGATE(&fld, filer));
        for (const wstr_c &f : fld.m_folders)
        {
            curf.set(fnmask); curf.insert(x,CONSTWSTR("/") + f);
            find_by_mask(files, curf, full_paths);
        }
        curf.set(masks.substr(0,x)).append(masks.substr(x+3));
        masks.set( curf.as_sptr() ); // ACHTUNG! curf must be declared outside this block due masks is just a pointer
    }

    int lastd = masks.find_last_pos_of(CONSTWSTR("/\\"));

    struct middle
    {
        wstrings_c & files;
        pwstr_c m;
        bool fullpaths;
        bool allfiles;
        middle &operator=(const middle &) UNUSED;
        middle(const middle &) UNUSED;
        middle(wstrings_c & files, const pwstr_c &m, bool fp):files(files), m(m), fullpaths(fp)
        {
            allfiles = m.equals(CONSTWSTR("*.*")) || m.equals(CONSTWSTR("*"));
        }
        bool filer( const wsptr & path, const wsptr & fn, container_c * )
        {
            if (allfiles || fn_mask_match(fn, m))
            {
                if (fullpaths)
                    files.add( fn_join(ts::tmp_wstr_c(path),fn) );
                else
                    files.add(fn);
            }
            return true;
        }
    } m(files, masks.substr(lastd+1), full_paths);
    if (lastd >= 0)
        iterate_files( masks.substr(0, lastd), DELEGATE(&m, filer) );
    else
        iterate_all_files( DELEGATE(&m, filer) );
}

bool    ccollection_c::read(uint container_id, const wsptr &name, buf_wrapper_s &b)
{
    for (container_c *pf : m_containers)
        if (pf->get_id() == container_id)
            return pf->read(name, b);

    return false;
}

bool ccollection_c::read(const wsptr &name, buf_wrapper_s &b)
{
    for(container_c *c : m_containers)
        if (c->read(name, b))
            return true;

    return false;
}
size_t  ccollection_c::size(uint container_id, const wsptr &name)
{
    for (container_c *pf : m_containers)
        if (pf->get_id() == container_id)
            return pf->size(name);

    return 0;
}

size_t  ccollection_c::size(const wsptr &name)
{
    for (container_c *c : m_containers)
        if (c->file_exists(name))
            return c->size(name);

    return 0;
}

    
} // namespace ts
