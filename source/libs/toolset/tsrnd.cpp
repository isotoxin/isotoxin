#include "toolset.h"
#include "internal/platform.h"

namespace ts
{

    random_modnar_c::random_modnar_c() :m_seed( GetTickCount() )
    {
    }
    void random_modnar_c::mutate( bool use_timer, uint32 in_seed )
    {
        uint32 x = ( use_timer ? timeGetTime() : 0 ) + in_seed;
        const uint32 y = 3363093571U;
        m_seed = x*y
            + ( x >> 16 )*( y >> 16 )
            + ( ( ( x >> ( 16 + 1 ) )*( ( y & 0xFFFF ) >> 1 ) ) >> ( 16 - 2 ) )
            + ( ( ( ( x & 0xFFFF ) >> 1 )*( y >> ( 16 + 1 ) ) ) >> ( 16 - 2 ) ) + 1013904223L;
    }

    uint da_seed;

}
