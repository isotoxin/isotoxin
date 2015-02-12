#ifndef _INCLUDE_SERVPACKFILE
#define _INCLUDE_SERVPACKFILE

namespace ts
{
class container_c;
typedef fastdelegate::FastDelegate<bool (const wsptr &, const wsptr &, container_c *)> ITERATE_FILES_CALLBACK;


class container_c
{

protected:
    wstr_c          m_filename;                 // container's file name
    FILETIME        m_timestamp;
    int             m_priority;
	uint			m_id;

public:
    container_c(const wsptr &name, int priority, uint id) : m_filename(name), m_priority(priority), m_id(id)
    {
        memset((void*)&m_timestamp, 0, sizeof(FILETIME));
    };
    virtual ~container_c(void) {};

    void            SetTimeStamp(const FILETIME ft) { m_timestamp = ft;   }
    const FILETIME &GetTimeStamp(void) const        { return m_timestamp; }

    int get_priority(void) const {return m_priority;}
	uint get_id(void) const {return m_id;}

    const wstr_c  &fn() const {return m_filename;}

    virtual bool    open() = 0;               // Открывает пакетный файл и считывает данные чтение
    virtual bool    close() = 0;              // Закрывает пакетный файл и уничтожает посторонние объекты чтение

    virtual bool    path_exists(const wsptr &path) = 0;
    virtual bool    file_exists(const wsptr &path) = 0;
    virtual bool    iterate_files(const wsptr &path, ITERATE_FILES_CALLBACK ef) = 0;
    virtual bool    iterate_files(ITERATE_FILES_CALLBACK ef) = 0;

    virtual bool    read(const wsptr &filename, buf_wrapper_s &b) = 0;
    virtual size_t  size(const wsptr &filename) = 0;
};




} // namespace ts

#endif

//#include "tsfile_rar.h"
#include "tsfile_zip.h"
