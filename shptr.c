#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

#define shptr_INIT(type, deleter_ptr) shptr_init(sizeof(type), deleter_ptr)

#define shptr_GET(sh_ptr, type) *(type*)shptr_get(sh_ptr)
#define shptr_SET(sh_ptr, type) shptr_GET(sh_ptr, type)

#define shptr_SET_DELETER(sh_ptr, deleter_ptr) shptr_set_deleter(sh_ptr, deleter_ptr)

#define shptr_REFCOUNT(sh_ptr, refcount) shptr_refcount(sh_ptr, refcount)

#define shptr_REF(sh_ptr) shptr_ref(sh_ptr)
#define shptr_UNREF(sh_ptr) shptr_unref(sh_ptr)
#define shptr_REF_WEAK(sh_ptr) shptr_ref_weak(sh_ptr)
#define shptr_UNREF_WEAK(sh_ptr) shptr_unref_weak(sh_ptr)

#define STRONG 's'
#define WEAK 'w'

typedef void* _Atomic atomic_ptr;
typedef void (* _Atomic atomic_fptr)(atomic_ptr);

typedef struct
{
    atomic_size_t strong_refcount;
    atomic_size_t weak_refcount;
    atomic_fptr deleter;
    atomic_ptr obj;
    // atomic_bool is_destroyed;

} shptr;

shptr* shptr_unref_weak(shptr* sh_ptr);

shptr* shptr_init(size_t obj_size, atomic_fptr deleter)
{
    if ( !obj_size )
        return NULL;

    shptr* ctrl_block = malloc( sizeof(shptr) + obj_size );
    if ( !ctrl_block )
        return NULL;

    *ctrl_block = (shptr){
        .strong_refcount = 1,
        .weak_refcount = 1,
        .deleter = deleter,
        .obj = ctrl_block + 1
    };

    return ctrl_block;
}

void* shptr_get(shptr* sh_ptr)
{
    if ( !sh_ptr )
        return NULL;

    return sh_ptr->obj;
}

static inline void shptr_dummy_deleter(atomic_ptr obj)
{
    return;
}

shptr* shptr_set_deleter(shptr* sh_ptr, atomic_fptr deleter)
{
    if ( !sh_ptr )
        return NULL;

    if ( sh_ptr->obj )
        sh_ptr->deleter = (deleter ? deleter : shptr_dummy_deleter);

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

    // if (x = expected)
    //    x = desired; return TRUE
    // else
    //    expected = x; return FALSE
    size_t strong_refcount = sh_ptr->strong_refcount;

    do
    {
        if ( strong_refcount == 0 )
            return sh_ptr;

    } while
      (
            !atomic_compare_exchange_weak
            (
                &sh_ptr->strong_refcount,
                &strong_refcount,
                strong_refcount + 1
            )
      );

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

    size_t strong_refcount = sh_ptr->strong_refcount;
    if ( strong_refcount == 0 )
        return sh_ptr;
    // if (x = expected)
    //    x = desired; return TRUE
    // else
    //    expected = x; return FALSE

    while
    (
        strong_refcount != 0 &&
        !atomic_compare_exchange_weak
        (
            &sh_ptr->strong_refcount,
            &strong_refcount,
            strong_refcount - 1
        )
    );

    if ( strong_refcount == 0 )
    {
        sh_ptr->deleter(sh_ptr->obj);
        sh_ptr->obj = NULL;

        return shptr_UNREF_WEAK(sh_ptr); // taking off the implicit weak reference
    }

    return sh_ptr;
}

shptr* shptr_unref_weak(shptr* sh_ptr)
{
    if ( !sh_ptr )
        return NULL;

    size_t weak_refcount = sh_ptr->weak_refcount;
    while
    (
        weak_refcount != 1 &&
        !atomic_compare_exchange_weak
        (
            &sh_ptr->weak_refcount,
            &weak_refcount,
            weak_refcount - 1
        )
    );

    if ( weak_refcount == 1 )
    {

    }


    return sh_ptr;
}
