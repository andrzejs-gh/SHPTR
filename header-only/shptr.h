#ifndef SHPTR_LIB_H
#define SHPTR_LIB_H

#include <stddef.h>
#include <stdatomic.h>
#include <stdbool.h>

#define shptr_INIT_DTOR(type, destructor_ptr)                                      \
                        shptr_init(sizeof(type), _Alignof(type), (destructor_ptr))
#define shptr_INIT_NODTOR(type)                                                    \
                          shptr_init(sizeof(type), _Alignof(type), NULL)

#define INIT_MACRO_SELECTOR(arg1, arg2, arg3, ...) arg3
#define shptr_INIT(...)                                                           \
        INIT_MACRO_SELECTOR(__VA_ARGS__, shptr_INIT_DTOR, shptr_INIT_NODTOR)      \
                   (__VA_ARGS__)

#define shptr_VOID_PTR(sh_ptr)        ((sh_ptr) ? shptr_ptr(sh_ptr)        : NULL)
#define shptr_TYPED_PTR(sh_ptr, type) ((sh_ptr) ? (type*)shptr_ptr(sh_ptr) : NULL)

#define PTR_MACRO_SELECTOR(arg1, arg2, arg3, ...) arg3
#define shptr_PTR(...)                                                             \
                  PTR_MACRO_SELECTOR(__VA_ARGS__, shptr_TYPED_PTR, shptr_VOID_PTR) \
                  (__VA_ARGS__)

#define shptr_VAL(sh_ptr, type) (*(type*)shptr_ptr(sh_ptr))

#define shptr_SET_DTOR(sh_ptr, destructor_ptr)         \
(                                                      \
    (sh_ptr) ?                                         \
    (                                                  \
        shptr_set_destructor(sh_ptr, destructor_ptr)   \
    )                                                  \
    : NULL                                             \
)

#define shptr_DTOR(sh_ptr) (*shptr_destructor_field(sh_ptr))

#define shptr_STRONG_COUNT(sh_ptr) ((sh_ptr) ? shptr_strong(sh_ptr) : SIZE_MAX)
#define shptr_WEAK_COUNT(sh_ptr)   ((sh_ptr) ? shptr_weak(sh_ptr)   : SIZE_MAX)

#define shptr_ISNULL(sh_ptr) ( (sh_ptr) == NULL )
#define shptr_ISGONE(sh_ptr) ( (sh_ptr) == NULL || shptr_strong(sh_ptr) == 0 )

#define shptr_REF(sh_ptr)        ( (sh_ptr) ? shptr_ref(sh_ptr)        : NULL )
#define shptr_REF_WEAK(sh_ptr)   ( (sh_ptr) ? shptr_ref_weak(sh_ptr)   : NULL )

#define shptr_UNREF(sh_ptr)      ( (sh_ptr) ? (sh_ptr = shptr_unref(sh_ptr))      : NULL )
#define shptr_UNREF_WEAK(sh_ptr) ( (sh_ptr) ? (sh_ptr = shptr_unref_weak(sh_ptr)) : NULL )

#define IS_REF_ACQUIRED(sh_ptr) (shptr_REF(sh_ptr) && shptr_strong(sh_ptr) > 0)
#define shptr_REF_TRY(sh_ptr)                      \
(                                                  \
    (sh_ptr) ?                                     \
    (                                              \
        IS_REF_ACQUIRED(sh_ptr) ? (sh_ptr) : NULL  \
    )                                              \
    : NULL                                         \
)

typedef void (*dtor_ptr)(void* ptr);

typedef struct shptr shptr;

shptr* shptr_init(size_t obj_size, size_t alignment, dtor_ptr destructor);
void* shptr_ptr(shptr* sh_ptr);
dtor_ptr* shptr_destructor_field(shptr* sh_ptr);
void* shptr_set_destructor(shptr* sh_ptr, dtor_ptr destructor);
size_t shptr_strong(shptr* sh_ptr);
size_t shptr_weak(shptr* sh_ptr);
shptr* shptr_ref(shptr* sh_ptr);
shptr* shptr_ref_weak(shptr* sh_ptr);
void* shptr_unref(shptr* sh_ptr);
void* shptr_unref_weak(shptr* sh_ptr);

#ifdef SHPTR_IMPLEMENTATION

#include <stdint.h>
#include <stdlib.h>

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
    }

    *ctrl_block = (shptr){
        .strong_refcount = 1,
        .weak_refcount = 1,
        .destructor = destructor,
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

    // this thread set the value to 0
    // so this thread must free the block
    if ( weak_refcount == 1 )
    {
        free(sh_ptr);
        return NULL;
    }

    return NULL;
}

#endif

#endif
