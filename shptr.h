#ifndef SHPTR_LIB_H
#define SHPTR_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

#define shptr_INIT(type, destructor_ptr) \
                   shptr_init(sizeof(type), _Alignof(type), (atomic_fptr)destructor_ptr)

#define shptr_VOID_PTR(sh_ptr)        (sh_ptr ? shptr_ptr(sh_ptr)        : NULL)
#define shptr_TYPED_PTR(sh_ptr, type) (sh_ptr ? (type*)shptr_ptr(sh_ptr) : NULL)

#define PTR_MACRO_SELECTOR(arg1, arg2, arg3, ...) arg3
#define shptr_PTR(...)                                                             \
                  PTR_MACRO_SELECTOR(__VA_ARGS__, shptr_TYPED_PTR, shptr_VOID_PTR) \
                  (__VA_ARGS__)

#define shptr_VAL(sh_ptr, type) (*(type*)shptr_ptr(sh_ptr))

#define shptr_SET_DTOR(sh_ptr, destructor_ptr)                                 \
                   (sh_ptr ?                                                   \
                   shptr_set_destructor(sh_ptr, (atomic_fptr)destructor_ptr) : \
                   NULL)

#define shptr_STRONG_COUNT(sh_ptr) (sh_ptr ? shptr_strong(sh_ptr) : SIZE_MAX)
#define shptr_WEAK_COUNT(sh_ptr)   (sh_ptr ? shptr_weak(sh_ptr)   : SIZE_MAX)

#define shptr_ISNULL(sh_ptr) ( sh_ptr ? false : true )
#define shptr_ISGONE(sh_ptr) ( sh_ptr ? ( shptr_ptr(sh_ptr) ? false : true ) : true )

#define shptr_REF(sh_ptr)        ( sh_ptr ? (sh_ptr = shptr_ref(sh_ptr))        : NULL )
#define shptr_UNREF(sh_ptr)      ( sh_ptr ? (sh_ptr = shptr_unref(sh_ptr))      : NULL )
#define shptr_REF_WEAK(sh_ptr)   ( sh_ptr ? (sh_ptr = shptr_ref_weak(sh_ptr))   : NULL )
#define shptr_UNREF_WEAK(sh_ptr) ( sh_ptr ? (sh_ptr = shptr_unref_weak(sh_ptr)) : NULL )

typedef void* _Atomic atomic_ptr;
typedef void (* _Atomic atomic_fptr)(void* ptr);

typedef struct shptr shptr;

shptr* shptr_init(size_t obj_size, size_t alignment, atomic_fptr destructor);
void* shptr_ptr(shptr* sh_ptr);
shptr* shptr_set_destructor(shptr* sh_ptr, atomic_fptr destructor);
size_t shptr_strong(shptr* sh_ptr);
size_t shptr_weak(shptr* sh_ptr);
shptr* shptr_ref(shptr* sh_ptr);
shptr* shptr_ref_weak(shptr* sh_ptr);
void* shptr_unref(shptr* sh_ptr);
void* shptr_unref_weak(shptr* sh_ptr);


#endif
