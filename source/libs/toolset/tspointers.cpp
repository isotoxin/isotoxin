#include "toolset.h"

namespace ts
{

    namespace
    {
        struct dummy_struct_s
        {
            uint8 b[ safe_object::POINTER_CONTAINER_SIZE ];
        };
    }

static_setup< struct_buf_t<dummy_struct_s, 128>, -3 > pool;

safe_object::pointer_container_s *safe_object::pointer_container_s::create(safe_object *p)
{
    safe_object::pointer_container_s *pc = pool().alloc_t<safe_object::pointer_container_s>();
    pc->pointer = p;
    pc->ref = 1;
    return pc;
}

void safe_object::pointer_container_s::die()
{
    pool().dealloc_t<safe_object::pointer_container_s>(this);
}

} // namespace ts