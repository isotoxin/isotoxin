#pragma once

#include "tspackfile.h"

namespace ts
{

class ccollection_c
{
    array_del_t<container_c, 2> m_containers;
	uint m_idpool;

	container_c *find(uint container_id, aint &index)
	{
		aint cnt = m_containers.size();
		for(aint i=0;i<cnt;++i)
		{
			container_c *c = m_containers.get(i);
			if (c->get_id() == container_id)
			{
				index = i;
				return c;
			}
		}
		index = -1;
		return nullptr;
	}

public:
    ccollection_c():m_idpool(1) {}
    ~ccollection_c() { clear(); };

    void			clear();
    uint			add_container(const wsptr &filename, int prior = 0); // only add, not open // returns container id or zero, if fail
    void			remove_container(uint container_id);

    bool            open_containers();
    bool			close_containers();

    bool        file_exists(const wsptr &name);
    bool        path_exists(const wsptr &path);
    void        iterate_files(const wsptr &path, ITERATE_FILES_CALLBACK ef);
    void        iterate_folders(const wsptr &path, ITERATE_FILES_CALLBACK ef);
    void        iterate_all_files(ITERATE_FILES_CALLBACK ef);

    void        find_by_mask(ts::wstrings_c & files, const ts::wsptr &fnmask, bool full_paths);

    bool        read(uint container_id, const wsptr &name, buf_wrapper_s &b);
    bool        read(const wsptr &name, buf_wrapper_s &b);
    size_t      size(uint container_id, const wsptr &name);
    size_t      size(const wsptr &name);

};



} // namespace ts

