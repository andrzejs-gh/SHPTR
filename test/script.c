#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "../shptr.h"

int* strong_counts;
int* weak_counts;
atomic_size_t strong_counts_index = 0;
atomic_size_t weak_counts_index = 0;

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
    // printf(
    //         "refcounts: strong = %zu, weak = %zu \n",
    //         shptr_STRONG_COUNT(sh_ptr), shptr_WEAK_COUNT(sh_ptr)
    //       );
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
    // printf(
    //         "refcounts: strong = %zu, weak = %zu \n",
    //         shptr_STRONG_COUNT(ref), shptr_WEAK_COUNT(ref)
    //       );
    puts("another_owning_foo RETURNS");
}

void non_owning_foo(shptr* weak_ref)
{
    puts("non_owning_foo: reference received");
    printf(
            "refcounts: strong = %zu, weak = %zu \n",
            shptr_STRONG_COUNT(weak_ref), shptr_WEAK_COUNT(weak_ref)
          );

    shptr_UNREF_WEAK(weak_ref);
    if ( weak_ref )
    {
        puts("ERROR in non_owning_foo: reference is not NULL after UNREF_WEAK");
        assert(false);
    }

    puts("non_owning_foo: reference released");
    // printf(
    //         "refcounts: strong = %zu, weak = %zu \n",
    //         shptr_STRONG_COUNT(weak_ref), shptr_WEAK_COUNT(weak_ref)
    //       );
    puts("non_owning_foo RETURNS");
}

void foo_destructor(void* p)
{
    puts("foo_destructor RUNS");
}

void owning_subworker(shptr* ref)
{
    int strong = shptr_STRONG_COUNT(ref);
    size_t index = strong_counts_index++ % 1000;

    strong_counts[index] = strong;

    shptr_UNREF(ref);

    return;
}

void observer_subworker(shptr* ref)
{
    int weak = shptr_WEAK_COUNT(ref);
    size_t index = weak_counts_index++ % 1000;

    weak_counts[index] = weak;

    shptr_UNREF_WEAK(ref);

    return;
}

int owning_worker_1(void* strong_ref)
{
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
    for ( size_t i = 0; i < 1000000; i++ )
    {
        observer_subworker(shptr_REF_WEAK(weak_ref));

        shptr* acquired = shptr_REF_TRY(weak_ref);
        if ( acquired )
        {
            // puts("acquired");
            owning_subworker(acquired);
        }
        else
            ;// puts("denied");
    }

    shptr_UNREF_WEAK(weak_ref);
    return 0;
}

int owning_worker_0(void* strong_ref)
{
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
    for ( size_t i = 0; i < 1000000; i++ )
    {
        observer_subworker(shptr_REF_WEAK(weak_ref));

        shptr* acquired = shptr_REF_TRY(weak_ref);
        if ( acquired )
            owning_subworker(acquired);
    }

    shptr_UNREF_WEAK(weak_ref);
    return 0;
}

void test(void)
{
    strong_counts = malloc( 1000 * sizeof(int) );
    weak_counts = malloc( 1000 * sizeof(int) );
    if ( !strong_counts || !weak_counts )
        assert(false);

    shptr* p = shptr_INIT(int);             // shptr for int obj is initialized
                                            // without a destructor
                                            // strong_refs = 1, weak_refs = 1
    if ( !p ) { assert(false); }        // ...in case of a failed allocation
    shptr_VAL(p, int) = -33; // obj value is set to -33

    int* raw_typed_ptr = shptr_PTR(p, int); // getting raw typed ptr
    void* raw_void_ptr = shptr_PTR(p);      // getting raw void ptr
    *raw_typed_ptr = -22;                   // setting new value

    int value = shptr_VAL(p, int); // getting value
    shptr_VAL(p, int) = -44;       // setting new value
    *shptr_PTR(p, int) = -55;      // setting new value

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
    non_owning_foo(shptr_REF_WEAK(p)); // strong = 1, weak = 2
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

    for ( size_t i = 0; i < 1000; i++ )
        printf("strong = %d, weak = %d \n", strong_counts[i], weak_counts[i]);

    return 0;
}
