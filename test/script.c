#include "../shptr.h"
#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct
{
	char c;
	int i;
	double d;

} some_t;

bool obj_destroyed = false;

void dter(const void* ptr)
{
	obj_destroyed = true;
	puts(">>> some_t object destroyed");
}

int owning_worker(void* arg)
{
	static atomic_int id_number = 0;
	int id = atomic_fetch_add(&id_number, 1);

	shptr* p = (shptr*)arg;

	for ( size_t i = 0; i < 1000000; i++)
	{
		shptr_REF(p);
		shptr_REF_WEAK(p);
		printf("Owning worker [ %d ] in action.\n", id);
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

	for ( size_t i = 0; i < 1000000; i++)
	{
		shptr_REF(p);
		shptr_REF_WEAK(p);
		printf("Observer worker [ %d ] in action.", id);
		shptr_UNREF(p);
		shptr_UNREF_WEAK(p);
	}

	shptr_UNREF_WEAK(p);
	return 0;
}

int main(void)
{
	shptr* p = shptr_INIT(some_t, dter);
	if ( !p ){ puts("shptr_INIT failure."); return -1; }
	shptr_SET(p, some_t) = (some_t){'a', -33, 0.1234};

	thrd_t T[4];
	thrd_create(&T[0], owning_worker, p);
	thrd_create(&T[1], observer_worker, p);
	thrd_create(&T[2], owning_worker, p);
	thrd_create(&T[3], observer_worker, p);

	for ( size_t i = 0; i < 4; i++ )
		thrd_join(T[i], NULL);

	shptr_UNREF(p);
	return 0;
}
