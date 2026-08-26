#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "shptr.h"

typedef struct shptr
{
    atomic_size_t strong_refcount;
    atomic_size_t weak_refcount;
    dtor_ptr destructor;
    void* ptr;

} shptr;

shptr* shptr_init(size_t obj_size, size_t alignment, dtor_ptr destructor)
{
    if ( !obj_size )
        return NULL;

    shptr* ctrl_block;
    size_t padding;

    if ( alignment <= _Alignof(max_align_t) )
    {
        padding = (alignment - (sizeof *ctrl_block % alignment)) % alignment;

        ctrl_block = malloc( sizeof *ctrl_block + padding + obj_size );
        if ( !ctrl_block )
            return NULL;
    }
    else // overaligned type
    {
        ctrl_block = malloc( sizeof *ctrl_block + alignment + obj_size );
        if ( !ctrl_block )
            return NULL;

        uintptr_t ctrl_block_end = (uintptr_t)ctrl_block + sizeof *ctrl_block;
        padding = ctrl_block_end % alignment;
        padding = ( padding ? alignment - padding : 0 );

        shptr* p = realloc(ctrl_block, sizeof *ctrl_block + padding + obj_size);
        if ( !p )
        {
            free(ctrl_block);
            return NULL;
        }
        ctrl_block = p;
    }

    *ctrl_block = (shptr){
        .strong_refcount = 1,
        .weak_refcount = 1,
        .destructor = (destructor ? destructor : NULL),
        .ptr = (char*)ctrl_block + sizeof *ctrl_block + padding
    };

    return ctrl_block;
}

void* shptr_ptr(shptr* sh_ptr)
{
    return sh_ptr->ptr;
}

dtor_ptr* shptr_destructor_field(shptr* sh_ptr)
{
    return &sh_ptr->destructor;
}

void* shptr_set_destructor(shptr* sh_ptr, dtor_ptr destructor)
{
    sh_ptr->destructor = destructor;

    return sh_ptr->ptr;
}

size_t shptr_strong(shptr* sh_ptr)
{
    return sh_ptr->strong_refcount;
}

size_t shptr_weak(shptr* sh_ptr)
{
    return sh_ptr->weak_refcount;
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
        if ( strong_refcount == 0 )   // ANOTHER thread set the value to 0
            return sh_ptr;            // or it was initialy 0

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
    size_t weak_refcount = sh_ptr->weak_refcount;

    do
    {
        if ( weak_refcount == 0 && sh_ptr->strong_refcount == 0 )
            return sh_ptr;

    } while
      (
          !atomic_compare_exchange_weak
          (
              &sh_ptr->weak_refcount,
              &weak_refcount,
              weak_refcount + 1
          )
      );

    return sh_ptr;
}

void* shptr_unref(shptr* sh_ptr)
{
    size_t strong_refcount = sh_ptr->strong_refcount;
    // if (x = expected)
    //    x = desired; return TRUE
    // else
    //    expected = x; return FALSE
    do
    {
        if ( strong_refcount == 0 ) // ANOTHER thread set the value to 0
            return NULL;            // or it was initialy 0

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
        if ( sh_ptr->destructor )
            sh_ptr->destructor(sh_ptr->ptr);

        sh_ptr->ptr = NULL;

        return shptr_unref_weak(sh_ptr); // take off the implicit weak reference
    }

    return NULL;
}

void* shptr_unref_weak(shptr* sh_ptr)
{
    size_t weak_refcount = sh_ptr->weak_refcount;

    do
    {
        if ( weak_refcount == 0 ) // ANOTHER thread has set the value to 0
            return NULL;

    } while
      (
          !atomic_compare_exchange_weak
          (
              &sh_ptr->weak_refcount,
              &weak_refcount,
              weak_refcount - 1
          )
      );

    // this thread set the value to 0 and strong_refcount is also 0,
    // so this thread must free the control block
    if ( weak_refcount == 1 && sh_ptr->strong_refcount == 0 )
    {
        free(sh_ptr);
        return NULL;
    }

    return NULL;
}
