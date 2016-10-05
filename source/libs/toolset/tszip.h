#pragma once

namespace ts
{

struct arc_file_s
{
    asptr fn;
    virtual blob_c get() const = 0;
};

typedef fastdelegate::FastDelegate<bool (const arc_file_s &)> ARC_FILE_HANDLER;

bool zip_open( const void *data, aint datasize, ARC_FILE_HANDLER h );

struct zip_c
{
    uint8 internaldata[128];
public:
    zip_c();
    ~zip_c();
    void addfile( const asptr&fn, const void *data, aint datasize, int compresslevel = 9 );
    blob_c getblob() const;
};

} // namespace ts

