#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define shptr_INIT(type, deleter_ptr) shptr_init(sizeof(type), deleter_ptr)
#define shptr_DESTROY(sh_ptr, deleter_ptr) shptr_destroy(sh_ptr, deleter_ptr)

#define shptr_GET(sh_ptr, type) *(type*)shptr_get(sh_ptr)
#define shptr_SET(sh_ptr, type) shptr_GET(sh_ptr, type)

#define shptr_REFCOUNT(sh_ptr, refcount) shptr_refcount(sh_ptr, refcount)

#define shptr_REF(sh_ptr) shptr_ref(sh_ptr)
#define shptr_UNREF(sh_ptr) shptr_unref(sh_ptr)
#define shptr_REF_WEAK(sh_ptr) shptr_ref_weak(sh_ptr)
#define shptr_UNREF_WEAK(sh_ptr) shptr_unref_weak(sh_ptr)


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

shptr* shptr_init(size_t obj_size, deleter deleter)
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

shptr* shptr_destroy(shptr* sh_ptr, deleter deleter)
{
    if ( !sh_ptr )
        return NULL;

    if ( deleter )
        deleter(sh_ptr->obj);

    free(sh_ptr);

    return NULL;
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

shptr* shptr_ref(shptr* sh_ptr)
{
    if ( !sh_ptr )
      return NULL;

    sh_ptr->strong_refcount++;

    return sh_ptr;
}

shptr* shptr_ref_weak(shptr* sh_ptr)
{
    if ( !sh_ptr )
        return NULL;

    sh_ptr->weak_refcount++;

    return sh_ptr;
}

shptr* shptr_unref(shptr* sh_ptr)
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

shptr* shptr_unref_weak(shptr* sh_ptr)
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
