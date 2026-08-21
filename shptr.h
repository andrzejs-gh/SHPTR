#ifndef SHPTR_LIB_H
#define SHPTR_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

#define shptr_INIT(type, deleter_ptr) \
                   shptr_init(sizeof(type), _Alignof(type), (atomic_fptr)deleter_ptr)

#define shptr_P(sh_ptr)         (sh_ptr ? shptr_ptr(sh_ptr)        : NULL)
#define shptr_PTR(sh_ptr, type) (sh_ptr ? (type*)shptr_ptr(sh_ptr) : NULL)

#define shptr_GET(sh_ptr, type) (*(type*)shptr_ptr(sh_ptr))
#define shptr_SET(sh_ptr, type) shptr_GET(sh_ptr, type)

#define shptr_SET_DELETER(sh_ptr, deleter_ptr)                           \
                   (sh_ptr ?                                             \
                   shptr_set_deleter(sh_ptr, (atomic_fptr)deleter_ptr) : \
                   NULL)

#define shptr_STRONG_COUNT(sh_ptr) (sh_ptr ? shptr_strong(sh_ptr) : SIZE_MAX)
#define shptr_WEAK_COUNT(sh_ptr)   (sh_ptr ? shptr_weak(sh_ptr)   : SIZE_MAX)

#define shptr_ISNULL(sh_ptr) ( sh_ptr ? false : true )
#define shptr_ISGONE(sh_ptr) ( sh_ptr ? ( shptr_P(sh_ptr) ? false : true ) : true )

#define shptr_REF(sh_ptr)        ( sh_ptr ? (sh_ptr = shptr_ref(sh_ptr))        : NULL )
#define shptr_UNREF(sh_ptr)      ( sh_ptr ? (sh_ptr = shptr_unref(sh_ptr))      : NULL )
#define shptr_REF_WEAK(sh_ptr)   ( sh_ptr ? (sh_ptr = shptr_ref_weak(sh_ptr))   : NULL )
#define shptr_UNREF_WEAK(sh_ptr) ( sh_ptr ? (sh_ptr = shptr_unref_weak(sh_ptr)) : NULL )

typedef void* _Atomic atomic_ptr;
typedef void (* _Atomic atomic_fptr)(const void* ptr);

typedef struct shptr shptr;

shptr* shptr_init(size_t obj_size, size_t alignment, atomic_fptr deleter);
void* shptr_ptr(shptr* sh_ptr);
shptr* shptr_set_deleter(shptr* sh_ptr, atomic_fptr deleter);
size_t shptr_strong(shptr* sh_ptr);
size_t shptr_weak(shptr* sh_ptr);
shptr* shptr_ref(shptr* sh_ptr);
shptr* shptr_ref_weak(shptr* sh_ptr);
shptr* shptr_unref(shptr* sh_ptr);
shptr* shptr_unref_weak(shptr* sh_ptr);


#endif
