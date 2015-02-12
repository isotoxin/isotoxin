#include "toolset.h"

namespace ts
{

static_setup< struct_pool_t<safe_object::POINTER_CONTAINER_SIZE, 1024> > pool;

safe_object::pointer_container_s *safe_object::pointer_container_s::create(safe_object *p)
{
    safe_object::pointer_container_s *pc = (safe_object::pointer_container_s *)pool().alloc();
    pc->pointer = p;
    pc->ref = 1;
    return pc;
}

void safe_object::pointer_container_s::die()
{
    pool().dealloc(this);
}

} // namespace ts