#pragma once

namespace ts
{
class container_c;
typedef fastdelegate::FastDelegate<bool (const wsptr &, const wsptr &, container_c *)> ITERATE_FILES_CALLBACK;


class container_c
{

protected:
    wstr_c          m_filename;                 // container's file name
    uint64          m_timestamp = 0;
    int             m_priority;
	uint			m_id;

public:
    container_c(const wsptr &name, int priority, uint id) : m_filename(name), m_priority(priority), m_id(id) //-V730
    {
    };
    virtual ~container_c() {};

    void set_time_stamp(uint64 ft) { m_timestamp = ft; }
    const uint64 get_time_stamp() const { return m_timestamp; }

    int get_priority() const {return m_priority;}
	uint get_id() const {return m_id;}

    const wstr_c  &fn() const {return m_filename;}
    void set_fn(const wsptr &name)
    {
        m_filename = name;
    }


    virtual bool    open() = 0;
    virtual bool    close() = 0;

    virtual bool    path_exists(const wsptr &path) = 0;
    virtual bool    file_exists(const wsptr &path) = 0;
    virtual bool    iterate_folders(const wsptr &path, ITERATE_FILES_CALLBACK ef) = 0;
    virtual bool    iterate_files(const wsptr &path, ITERATE_FILES_CALLBACK ef) = 0;
    virtual bool    iterate_files(ITERATE_FILES_CALLBACK ef) = 0;

    virtual bool    read(const wsptr &filename, buf_wrapper_s &b) = 0;
    virtual size_t  size(const wsptr &filename) = 0;
};




} // namespace ts

#include "tsfile_zip.h"
