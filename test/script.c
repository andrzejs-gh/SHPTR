#include "../shptr.h"
#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <assert.h>

#define LIMIT 1000000

typedef struct
{
	char c;
	int i;
	double d;

} some_t;

bool obj_destroyed = false;

void dtor(void* ptr)
{
	obj_destroyed = true;
	puts("Default dtor running");
	puts(">>> some_t object destroyed");
}

void alt_dtor(void* ptr)
{
	obj_destroyed = true;
	puts("Alt detor running");
	puts(">>> some_t object destroyed");
}

int owning_worker(void* arg)
{
	static atomic_int id_number = 0;
	int id = atomic_fetch_add(&id_number, 1);

	shptr* worker_strong_ref = (shptr*)arg;

	printf("Owning worker [ %d ] started.\n", id);

	for ( size_t i = 0; i < LIMIT; i++)
	{
		shptr* strong_ref = shptr_REF(worker_strong_ref);
		shptr* weak_ref = shptr_REF_WEAK(worker_strong_ref);

		int x = shptr_VAL(worker_strong_ref, some_t).i;
		shptr_VAL(worker_strong_ref, some_t).d = (double)i/(i+1);

		shptr_UNREF(strong_ref);
		shptr_UNREF_WEAK(weak_ref);
	}
	//!worker_strong_ref ? assert(false) : worker_strong_ref;
	shptr_UNREF(worker_strong_ref);
	return 0;
}

int observer_worker(void* arg)
{
	static atomic_int id_number = 0;
	int id = atomic_fetch_add(&id_number, 1);

	shptr* worker_weak_ref = (shptr*)arg; // this is a WEAK REFERENCE

	printf("Observer worker [ %d ] started.\n", id);

	for ( size_t i = 0; i < LIMIT; i++)
	{
		shptr* strong_ref = shptr_REF(worker_weak_ref);
		shptr* weak_ref = shptr_REF_WEAK(worker_weak_ref);

		// switch ( i % 3 )
		// {
		// 	case 0: shptr_SET_DTOR(worker_weak_ref, NULL); 	 break;
		// 	case 1: shptr_SET_DTOR(worker_weak_ref, dtor); 	 break;
		// 	case 2: shptr_SET_DTOR(worker_weak_ref, alt_dtor); break;
		// }

		shptr_UNREF(strong_ref);
		shptr_UNREF_WEAK(weak_ref);
	}
	!worker_weak_ref ? puts("WARNING! worker_weak_ref is NULL") : 1;
	shptr_UNREF_WEAK(worker_weak_ref);
	return 0;
}

int main(void)
{


	size_t strong, weak;

	shptr* p = shptr_INIT(some_t, dtor);
	if ( !p ){ puts("shptr_INIT failure."); return -1; }
	shptr_VAL(p, some_t) = (some_t){'a', -33, 0.1234};

	// shptr* weak_ref = shptr_REF_WEAK(p);
	// shptr* c = weak_ref;
	// shptr* cc = weak_ref;
	// strong = shptr_STRONG_COUNT(weak_ref);
	// weak = shptr_WEAK_COUNT(weak_ref);
	// printf("strong = %zu, weak = %zu \n", strong, weak);
 //
	// //shptr_UNREF_WEAK(p);
	// shptr_UNREF_WEAK(weak_ref); shptr_UNREF_WEAK(c); shptr_UNREF_WEAK(cc);
 //
	// strong = shptr_STRONG_COUNT(p);
	// weak = shptr_WEAK_COUNT(p);
	// printf("strong = %zu, weak = %zu \n", strong, weak);
 //
	// shptr_UNREF(p);
	// return 0;

	thrd_t T[4];
	thrd_create(&T[0], owning_worker, shptr_REF(p));
	thrd_create(&T[1], observer_worker, shptr_REF_WEAK(p));
	thrd_create(&T[2], owning_worker, shptr_REF(p));
	thrd_create(&T[3], observer_worker, shptr_REF_WEAK(p));

	// creating weak_ref to keep shptr obj alive
	shptr* weak_reference = shptr_REF_WEAK(p);

	strong = shptr_STRONG_COUNT(weak_reference);
	weak = shptr_WEAK_COUNT(weak_reference);
	printf("strong = %zu, weak = %zu \n", strong, weak);

	shptr_UNREF(p); // taking off strong reference

	for ( size_t i = 0; i < 4; i++ )
		thrd_join(T[i], NULL);

	//shptr_REF(p);
	if ( shptr_ISGONE(weak_reference) )//&& shptr_ISNULL(p) )
	{
		puts("OK, object has been destroyed");
		//for (size_t i = 0; i < 100; i++){
		strong = shptr_STRONG_COUNT(weak_reference);
		weak = shptr_WEAK_COUNT(weak_reference);
		printf("strong = %zu, weak = %zu \n", strong, weak);
		//}

		shptr_UNREF_WEAK(weak_reference);
		if ( weak_reference )
			puts("ERROR, weak reference was not NULLed");
	}
	else
	{
		puts("error");
	}

	return 0;
}
