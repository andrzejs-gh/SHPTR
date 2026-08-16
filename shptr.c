#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define shptr_INIT(type, deleter_ptr) init_shptr(sizeof(type), deleter_ptr)
#define shptr_GET(sh_ptr, type) *(type*)shptr_get(sh_ptr)

#define STRONG 's'
#define WEAK 'w'

typedef void (*deleter)(void*);

typedef struct
{
    size_t strong_refcount;
    size_t weak_refcount;
    deleter deleter;
    void* obj;

} shptr;

shptr* init_shptr(size_t obj_size, deleter deleter)
{
    if ( !obj_size )
        return NULL;

    shptr* ctrl_block = malloc( sizeof(shptr) + obj_size );
    if ( !ctrl_block )
        return NULL;

    *ctrl_block = (shptr){
        .strong_refcount = 1,
        .weak_refcount = 0,
        .deleter = deleter,
        .obj = ctrl_block + 1
    };

    return ctrl_block;
}

void* shptr_get(shptr* sh_ptr)
{
    if ( !sh_ptr || sh_ptr->strong_refcount == 0 )
        return NULL;

    return sh_ptr->obj;
}

shptr* shptr_set_deleter(shptr* sh_ptr, deleter deleter)
{
    if ( !sh_ptr )
        return NULL;

    sh_ptr->deleter = deleter;

    return sh_ptr;
}

size_t shptr_refcount(shptr* sh_ptr, char refcount)
{
    if ( !sh_ptr )
        return SIZE_MAX;

    switch (refcount)
    {
        case 's': return sh_ptr->strong_refcount;
        case 'w': return sh_ptr->weak_refcount;
        default: return SIZE_MAX;
    }
}

shptr* REF(shptr* sh_ptr)
{
    if ( !sh_ptr )
      return NULL;

    sh_ptr->strong_refcount++;

    return sh_ptr;
}

shptr* weak_REF(shptr* sh_ptr)
{
    if ( !sh_ptr )
        return NULL;

    sh_ptr->weak_refcount++;

    return sh_ptr;
}

shptr* UNREF(shptr* sh_ptr)
{
    if ( !sh_ptr )
        return NULL;

    if ( sh_ptr->strong_refcount == 0 )
        return sh_ptr;

    if ( --sh_ptr->strong_refcount == 0 )
    {
        if ( sh_ptr->deleter )
            sh_ptr->deleter(sh_ptr->obj);

        if ( sh_ptr->weak_refcount == 0 )
        {
            free(sh_ptr);
            return NULL;
        }
    }

    return sh_ptr;
}

shptr* weak_UNREF(shptr* sh_ptr)
{
    if ( !sh_ptr )
        return NULL;

    if ( sh_ptr->weak_refcount == 0 )
        return sh_ptr;

    if ( --sh_ptr->weak_refcount == 0 && sh_ptr->strong_refcount == 0 )
    {
        free(sh_ptr);
        return NULL;
    }

    return sh_ptr;
}
