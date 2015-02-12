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

} // namespace ts

