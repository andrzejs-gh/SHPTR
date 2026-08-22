#include "../shptr.h"
#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <assert.h>

typedef struct
{
	char c;
	int i;
	double d;

} some_t;

bool obj_destroyed = false;

void dtor(const void* ptr)
{
	obj_destroyed = true;
	puts(">>> some_t object destroyed");
}

int owning_worker(void* arg)
{
	static atomic_int id_number = 0;
	int id = atomic_fetch_add(&id_number, 1);

	shptr* p = (shptr*)arg;

	printf("Owning worker [ %d ] started.\n", id);

	for ( size_t i = 0; i < 1000000; i++)
	{
		shptr_REF(p);
		shptr_REF_WEAK(p);

		int x = shptr_GET(p, some_t).i;
		shptr_SET(p, some_t).d = (double)i/(i+1);

		shptr_UNREF(p);
		shptr_UNREF_WEAK(p);
	}

	shptr_UNREF(p);
	return 0;
}

int observer_worker(void* arg)
{
	static atomic_int id_number = 0;
	int id = atomic_fetch_add(&id_number, 1);

	shptr* p = (shptr*)arg;

	printf("Observer worker [ %d ] started.\n", id);

	for ( size_t i = 0; i < 1000000; i++)
	{
		shptr_REF(p);
		shptr_REF_WEAK(p);

		// if ( i % 10000 == 0 )
		// 	printf("");

		shptr_UNREF(p);
		shptr_UNREF_WEAK(p);
	}

	shptr_UNREF_WEAK(p);
	return 0;
}

void underflow_test(void)
{
	shptr* p = shptr_INIT(some_t, dtor);
	if ( !p ){ puts("shptr_INIT failure in underflow test."); assert(false); }
	shptr_SET(p, some_t) = (some_t){'a', -33, 0.1234};

	shptr_UNREF(p);
	printf("strong = %zu, weak = %zu \n", shptr_STRONG_COUNT(p), shptr_WEAK_COUNT(p));
	return;
	//shptr_REF_WEAK(p);

	for ( size_t i = 0; i < 100; i++ )
	{
		shptr_UNREF(p);
		//shptr_UNREF_WEAK(p);
		// p ? printf("strong = %zu \n", shptr_STRONG_COUNT(p)) : puts("dead") ;
		// p ? printf("weak = %zu \n", shptr_WEAK_COUNT(p)) : puts("dead") ;
		//if ( !p ) puts("p is DEAD");
	}

	//assert(false);
}

int main(void)
{
	//underflow_test(); return 0;

	shptr* p = shptr_INIT(some_t, dtor);
	if ( !p ){ puts("shptr_INIT failure."); return -1; }
	shptr_SET(p, some_t) = (some_t){'a', -33, 0.1234};
	// shptr_UNREF_WEAK(p); shptr_UNREF_WEAK(p); shptr_UNREF_WEAK(p);

	thrd_t T[4];
	thrd_create(&T[0], owning_worker, shptr_REF(p));
	thrd_create(&T[1], observer_worker, shptr_REF_WEAK(p));
	thrd_create(&T[2], owning_worker, shptr_REF(p));
	thrd_create(&T[3], observer_worker, shptr_REF_WEAK(p));

	for ( size_t i = 0; i < 4; i++ )
		thrd_join(T[i], NULL);

	shptr_UNREF(p);
	if ( !p )
		puts("OK, object has been destroyed and shared ptr is gone");
	else
	{
		puts("error");
		size_t strong = shptr_STRONG_COUNT(p);
		size_t weak = shptr_WEAK_COUNT(p);
		printf("strong = %zu, weak = %zu \n", strong, weak);
	}

	return 0;
}
