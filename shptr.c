#include "shptr.h"
#include <stdlib.h>

typedef struct shptr
{
    atomic_size_t strong_refcount;
    atomic_size_t weak_refcount;
    atomic_fptr deleter;
    atomic_ptr ptr;

} shptr;

static inline void shptr_dummy_deleter(atomic_ptr ptr)
{
    return;
}

shptr* shptr_init(size_t obj_size, size_t alignment, atomic_fptr deleter)
{
    if ( !obj_size )
        return NULL;

    shptr* ctrl_block;

    if ( alignment <= _Alignof(max_align_t) )
        ctrl_block = malloc( sizeof *ctrl_block + obj_size + alignment );
    else
        ctrl_block = aligned_alloc(alignment,
                                   sizeof *ctrl_block + obj_size + alignment);

    if ( !ctrl_block )
        return NULL;

    uintptr_t ctrl_block_end = (uintptr_t)ctrl_block + sizeof *ctrl_block;
    size_t padding = ctrl_block_end % alignment;
           padding = ( padding ? alignment - padding : 0 );

    *ctrl_block = (shptr){
        .strong_refcount = 1,
        .weak_refcount = 1,
        .deleter = (deleter ? deleter : shptr_dummy_deleter),
        .ptr = (char*)ctrl_block + sizeof *ctrl_block + padding
    };

    return ctrl_block;
}

void* shptr_ptr(shptr* sh_ptr)
{
    return sh_ptr->ptr;
}

shptr* shptr_set_deleter(shptr* sh_ptr, atomic_fptr deleter)
{
    if ( sh_ptr->ptr )
    {
        sh_ptr->deleter = (deleter ? deleter : shptr_dummy_deleter);
        return sh_ptr;
    }

    return NULL;
}

size_t shptr_refcount(shptr* sh_ptr, char refcount)
{
    switch (refcount)
    {
        case STRONG: return sh_ptr->strong_refcount;
        case WEAK:   return sh_ptr->weak_refcount;
        default:     return SIZE_MAX;
    }
}

shptr* shptr_ref(shptr* sh_ptr)
{
    // if (x = expected)
    //    x = desired; return TRUE
    // else
    //    expected = x; return FALSE
    size_t strong_refcount = sh_ptr->strong_refcount;

    do
    {
        if ( strong_refcount == 0 ) // ANOTHER thread set the value to 0
            return sh_ptr;          // or it was initialy 0

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
    sh_ptr->weak_refcount++;

    return sh_ptr;
}

shptr* shptr_unref(shptr* sh_ptr)
{
    size_t strong_refcount = sh_ptr->strong_refcount;
    // if (x = expected)
    //    x = desired; return TRUE
    // else
    //    expected = x; return FALSE
    do
    {
        if ( strong_refcount == 0 ) // ANOTHER thread set the value to 0
            return sh_ptr;          // or it was initialy 0

    } while
      (
          !atomic_compare_exchange_weak
          (
              &sh_ptr->strong_refcount,
              &strong_refcount,
              strong_refcount - 1
          )
      );

    if ( strong_refcount == 1 ) // THIS thread set the value to 0
    {
        sh_ptr->deleter(sh_ptr->ptr);
        sh_ptr->ptr = NULL;

        return shptr_UNREF_WEAK(sh_ptr); // take off the implicit weak reference
    }

    return sh_ptr;
}

shptr* shptr_unref_weak(shptr* sh_ptr)
{
    size_t weak_refcount = sh_ptr->weak_refcount;
    if ( weak_refcount == 1 && sh_ptr->strong_refcount > 0 )
        return sh_ptr;

    do
    {
        if ( weak_refcount == 0 ) // ANOTHER thread has set the value to 0
            return NULL;          // so ANOTHER thread destroys the ctrl block

    } while
      (
          !atomic_compare_exchange_weak
          (
              &sh_ptr->weak_refcount,
              &weak_refcount,
              weak_refcount - 1
          )
      );

    if ( weak_refcount == 1 ) // THIS thread has set the value to 0
    {                         // so THIS thread must destroy the ctrl block
        free(sh_ptr);
        return NULL;
    }

    return sh_ptr;
}
