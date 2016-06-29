#include "nix_common.inl"

TS_STATIC_CHECK( sizeof( master_internal_stuff_s ) <= MASTERCLASS_INTERNAL_STUFF_SIZE, "bad size" );