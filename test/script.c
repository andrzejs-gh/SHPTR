#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>

#define SHPTR_IMPLEMENTATION
#include "../header-only/shptr.h"

atomic_bool is_destroyed = false;

typedef struct
{
    atomic_size_t strong;
    atomic_size_t weak;

} total_ref_made;

void borrowing_foo(void* ptr)
{
    printf("borrowing_foo: received raw pointer: %p\n", ptr);
    puts("borrowing_foo RETURNS");
}

void owning_foo(shptr* sh_ptr)
{
    puts("owning_foo: reference reveived");
    printf(
            "refcounts: strong = %zu, weak = %zu \n",
            shptr_STRONG_COUNT(sh_ptr), shptr_WEAK_COUNT(sh_ptr)
          );

    shptr_UNREF(sh_ptr);
    if ( sh_ptr )
    {
        puts("ERROR, reference is not NULL after UNREF");
        assert(false);
    }

    puts("owning_foo: reference released");
    puts("owning_foo RETURNS");
}

void another_owning_foo(shptr* ref)
{
    puts("another_owning_foo: reference received");
    puts("passing the reference to owning_foo");
    owning_foo(shptr_REF(ref));

    shptr_UNREF(ref);
    if ( ref )
    {
        puts("ERROR in another_owning_foo: reference is not NULL after UNREF");
        assert(false);
    }

    puts("another_owning_foo: reference released");
    puts("another_owning_foo RETURNS");
}

void observer_foo(shptr* weak_ref)
{
    puts("observer_foo: reference received");
    printf(
            "refcounts: strong = %zu, weak = %zu \n",
            shptr_STRONG_COUNT(weak_ref), shptr_WEAK_COUNT(weak_ref)
          );

    shptr_UNREF_WEAK(weak_ref);
    if ( weak_ref )
    {
        puts("ERROR in observer_foo: reference is not NULL after UNREF_WEAK");
        assert(false);
    }

    puts("observer_foo: reference released");
    puts("observer_foo RETURNS");
}

void foo_destructor(void* p)
{
    puts("foo_destructor RUNS");
    total_ref_made* obj = (total_ref_made*)p;

    printf
    (
        "total number of refs made:\n"
        "strong = %zu, weak = %zu \n",
        obj->strong, obj->weak
    );

    puts("foo_destructor RETURNS");
}

void owning_subworker(shptr* ref)
{
    shptr_VAL(ref, total_ref_made).strong++;

    shptr_UNREF(ref);

    return;
}

void observer_subworker(shptr* weak_ref)
{
    void* acquired = shptr_REF_TRY(weak_ref);
    if ( acquired )
    {
        shptr_VAL(acquired, total_ref_made).weak++;   // adding THIS weak ref
        shptr_VAL(acquired, total_ref_made).strong++; // adding temp strong ref

        shptr_UNREF(acquired);
    }

    shptr_UNREF_WEAK(weak_ref);

    return;
}

int owning_worker_1(void* strong_ref)
{
    shptr_VAL(strong_ref, total_ref_made).strong++; // adding THIS strong ref

    for ( size_t i = 0; i < 1000000; i++ )
    {
        switch ( i % 2 )
        {
            case 0: owning_subworker(shptr_REF(strong_ref)); break;
            case 1: observer_subworker(shptr_REF_WEAK(strong_ref)); break;
        }
    }

    shptr_UNREF(strong_ref);
    return 0;
}

int observer_worker_1(void* weak_ref)
{
    shptr* acquired = shptr_REF_TRY(weak_ref);
    if ( acquired )
    {
        shptr_VAL(acquired, total_ref_made).weak++;   // adding THIS weak ref
        shptr_VAL(acquired, total_ref_made).strong++; // adding temp strong ref

        shptr_UNREF(acquired);
    }

    for ( size_t i = 0; i < 1000000; i++ )
    {
        observer_subworker(shptr_REF_WEAK(weak_ref));

        acquired = shptr_REF_TRY(weak_ref);
        if ( acquired )
            owning_subworker(acquired);
    }

    shptr_UNREF_WEAK(weak_ref);
    return 0;
}

int owning_worker_0(void* strong_ref)
{
    shptr_VAL(strong_ref, total_ref_made).strong++; // adding THIS strong ref

    for ( size_t i = 0; i < 1000000; i++ )
    {
        switch ( i % 2 )
        {
            case 0: owning_subworker(shptr_REF(strong_ref)); break;
            case 1: observer_subworker(shptr_REF_WEAK(strong_ref)); break;
        }
    }

    shptr_UNREF(strong_ref);
    return 0;
}

int observer_worker_0(void* weak_ref)
{
    shptr* acquired = shptr_REF_TRY(weak_ref);
    if ( acquired )
    {
        shptr_VAL(acquired, total_ref_made).weak++;   // adding THIS weak ref
        shptr_VAL(acquired, total_ref_made).strong++; // adding temp strong ref

        shptr_UNREF(acquired);
    }

    for ( size_t i = 0; i < 1000000; i++ )
    {
        observer_subworker(shptr_REF_WEAK(weak_ref));

        acquired = shptr_REF_TRY(weak_ref);
        if ( acquired )
            owning_subworker(acquired);
    }

    shptr_UNREF_WEAK(weak_ref);
    return 0;
}

void test(void)
{
    shptr* p = shptr_INIT(total_ref_made);
    if ( !p )
    {
        puts("ERROR, shptr_INIT: allocation failure");
        assert(false);
    }        // ...in case of a failed allocation
    shptr_VAL(p, total_ref_made) = (total_ref_made)
                                   {
                                      shptr_STRONG_COUNT(p),
                                      shptr_WEAK_COUNT(p)
                                   };

    borrowing_foo(shptr_PTR(p)); // borrowing_functon borrows the raw pointer

    /* owning functions must own a strong reference and are
    r esponsible for releasing it */
    owning_foo(shptr_REF(p));       // strong = 2, weak = 1
                                    //         v
                                    //         v
                                    // strong = 1, weak = 1

    shptr* strong_ref = shptr_REF(p);    // strong = 2, weak = 1
    another_owning_foo(strong_ref);      //         v
                                         //         v
                                         // strong = 1, weak = 1

    /* non-owning functions may own a weak reference and are
    t hen responsible for releasing it */
    observer_foo(shptr_REF_WEAK(p)); // strong = 1, weak = 2
                                       //         v
                                       //         v
                                       // strong = 1, weak = 1

    /*  --------------------------------------------------------------
    S uppose there are N workers worki*ng on the object in
    separate threads.
    --------------------------------------------------------------  */

    shptr_DTOR(p) = foo_destructor; // setting destructor

    thrd_t T[4];
    thrd_create(&T[0], owning_worker_0, shptr_REF(p));
    thrd_create(&T[1], observer_worker_0, shptr_REF_WEAK(p));
    thrd_create(&T[2], owning_worker_1, shptr_REF(p));
    thrd_create(&T[3], observer_worker_1, shptr_REF_WEAK(p));
    // ...
    //thrd_create(&T[N-1], owning_worker_X, shptr_REF(p));
    //thrd_create(&T[N-1], observer_worker_X, shptr_REF_WEAK(p));

    /*  ---------------------------------------------------------------
    B ecause the object is destroyed b*y the last owner who releases
    a strong reference to it, if we released p now, the object would
    be destroyed  either by one of the owning workers, or by THIS
    function if the workers finished before p is released.

    Let's create a new weak reference to ensure the block stays
    alive in memory, let's release p, and lets wait for the workers
    to finish.
    ---------------------------------------------------------------  */

    shptr* weak_ref = shptr_REF_WEAK(p); // strong >= 1, weak >= 2

    shptr_UNREF(p);                      // strong = ?, weak >= 1
    // shptr_UNREF(p) additionaly
    // makes p = NULL, a dead
    // reference is by design
    // unusable

    /* wait for workers to finish */
    for ( size_t i = 0; i < 4; i++ )
        thrd_join(T[i], NULL);

    /* confirm that the object has been destroyed */
    if ( shptr_ISGONE(weak_ref) )
    {
        puts("Object has been destroyed");

        size_t strong = shptr_STRONG_COUNT(weak_ref); // returns 0
        size_t weak = shptr_WEAK_COUNT(weak_ref);     // returns 1

        printf("strong = %zu vs weak = %zu \n", strong, weak); // prints:
        // strong = 0 vs weak = 1

        shptr_UNREF_WEAK(weak_ref); // strong = 0, weak = 0
        // shptr is freed
    }
    else
    {
        puts("UNREACHABLE");
    }
}

int main(void)
{
    test();

    return 0;
}
