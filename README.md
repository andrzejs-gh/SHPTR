# SHPTR Documentation

## Table of contents

- [Description](#description)
- [Usage](#usage)
    - [API Contract](#api-contract)
    - [Code snippet](#code-snippet)
- [API](#api)

---

## Description

Universal and easy to use **thread-safe** shared pointer implemented in C11. 

- the control block `shptr` and the object are allocated at once and live together as a single block in memory `[ctrl] [padding (if needed)] [obj]`
- `shptr` supports both **strong** (owning) and **weak** (non-owning) references
- the initialization adds `1` **strong** reference to the strong reference count, and `1` (implicit) **weak** reference to the weak reference count
- the implicit **weak** reference is taken off when **strong** reference count hits `0`
- the object lives as long as there is at least `1` **strong** reference 
- the entire block lives as long as there is at least `1` **weak** reference
- `shptr` supports arbitrary custom **destructor** that can be set at initialization and **modified** (swaped) at any moment, as long as it has the signature:
```c
void dtor(void* obj); // the destructor is passed pointer to the object
``` 
- a destroyed object becomes inaccessible as its pointer field in the control block  becomes `NULL`, but the data in memory remains allocated untill the entire block is freed when all references are droped

<p align="right">
<a href="#table-of-contents">GO TO TOP ^</a>
</p>

---

## Usage

### API Contract

0. Always pass a **reference** (pointer) directly to a **shptr_** operator, never pass an expression that evaluates to it.
1. Always check the first **strong reference** returned by [shptr_INIT](#-shptr_init-), if allocation fails, a `NULL`-reference is returned.
2. To access and/or modify the object, a caller must own a **strong reference**.
3. To access the control block, a caller must own a **weak reference**.
4. A **reference owner**, strong or weak, must always release **the reference**.
5. After a reference is released with [shptr_UNREF](#-shptr_unref-) / [shptr_UNREF_WEAK](#-shptr_unref_weak-), it is set to `NULL` and must not be used afterwards. 
6. Never use [shptr_UNREF](#-shptr_unref-) on a **weak reference** or [shptr_UNREF_WEAK](#-shptr_unref_weak-) on a **strong reference**.
7. Never manually copy a reference, always use [shptr_REF](#-shptr_ref-) / [shptr_REF_WEAK](#-shptr_ref_weak-) or [shptr_REF_TRY](#-shptr_ref_try-).
8. Always check whether [shptr_REF_TRY](#-shptr_ref_try-) succeeded in acquiring a **strong reference**, if not, it returns a `NULL`-reference.

---

### Code snippet

```c
shptr* p = shptr_INIT(int);                  // shptr for int obj is initialized
                                             // without a destructor
                                             // strong_refs = 1, weak_refs = 1
if ( !p ) { ... }        // ...in case of a failed allocation
shptr_VAL(p, int) = -33; // obj value is set to -33

int* raw_typed_ptr = shptr_PTR(p, int); // getting raw typed ptr
void* raw_void_ptr = shptr_PTR(p);      // getting raw void ptr
*raw_typed_ptr = -22;                   // setting new value

int value = shptr_VAL(p, int); // getting value
shptr_VAL(p, int) = -44;       // setting new value
*shptr_PTR(p, int) = -55;      // setting new value

borrowing_foo(shptr_PTR(p)); // borrowing_foo borrows the raw pointer  

/* owners of a strong reference are    
responsible for releasing it */                            
owning_foo(shptr_REF(p));            // strong = 2, weak = 1
                                     //         v
                                     //         v
                                     // strong = 1, weak = 1
                                     
void* strong_ref = shptr_REF(p);     // strong = 2, weak = 1
another_owning_foo(strong_ref);      //         v
strong_ref = NULL;                   //         v
                                     // strong = 1, weak = 1
                            
/* strong_ref becomes a "dangling" reference after its owner returns,
   that's why it is better to call owner this way: 
   another_owning_foo(shptr_REF(p));
   to not leave any foreign reference dangling                     */  
                                     
                                     
/* owners of a weak reference are
responsible for releasing it */                                     
observer_foo(shptr_REF_WEAK(p));        // strong = 1, weak = 2
                                        //         v
                                        //         v
                                        // strong = 1, weak = 1

/*  --------------------------------------------------------------
    Suppose there are N workers working on the object in 
    separate threads. 
    --------------------------------------------------------------  */

shptr_DTOR(p) = some_destructor; // setting destructor
    
thrd_t T[N];
thrd_create(&T[0], owning_worker_0, shptr_REF(p));
thrd_create(&T[1], observer_worker_0, shptr_REF_WEAK(p));
thrd_create(&T[2], owning_worker_1, shptr_REF(p));
thrd_create(&T[3], observer_worker_1, shptr_REF_WEAK(p));
// ...
thrd_create(&T[N-1], owning_worker_X, shptr_REF(p));
thrd_create(&T[N-1], observer_worker_X, shptr_REF_WEAK(p));

/*  ---------------------------------------------------------------
    Because the object is destroyed by the last owner who releases
    a strong reference to it, if we released p now, the object would 
    be destroyed either by one of the owning workers, or by THIS 
    function if the workers finished before p was released.
    
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
for ( size_t i = 0; i < N; i++ )
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
    "UNREACHABLE";
}

                                     
```

<p align="right">
<a href="#table-of-contents">GO TO TOP ^</a>
</p>

---

## API

### INITIALIZATION

- [shptr_INIT](#-shptr_init-)

### ACCESSING RAW POINTER AND VALUE

- [shptr_PTR](#-shptr_ptr-)
- [shptr_VAL](#-shptr_val-)

### ACQUIRING AND RELEASING REFERENCES

- [shptr_REF](#-shptr_ref-)
- [shptr_UNREF](#-shptr_unref-)
- [shptr_REF_WEAK](#-shptr_ref_weak-)
- [shptr_UNREF_WEAK](#-shptr_unref_weak-)
- [shptr_REF_TRY](#-shptr_ref_try-)

### SETTING THE DESTRUCTOR

- [shptr_DTOR](#-shptr_dtor-)
- [shptr_SET_DTOR](#-shptr_set_dtor-)

### INFO

- [shptr_STRONG_COUNT](#-shptr_strong_count-)
- [shptr_WEAK_COUNT](#-shptr_weak_count-)
- [shptr_ISGONE](#-shptr_isgone-)
- [shptr_ISNULL](#-shptr_isnull-)


<p align="right">
<a href="#table-of-contents">GO TO TOP ^</a>
</p>

---

## API

---

### INITIALIZATION

---

### ** **shptr_INIT** **

```c
shptr_INIT(type);
shptr_INIT(type, destructor_ptr);

// examples:

shptr* sp = shptr_INIT(double);             // initialize obj of the type double
                                            // without a destructor attached

shptr* sp = shptr_INIT(double, some_dtor);  // initialization with a destructor 

shptr_VAL(sp, double) = 0.1234; // setting object's value
                                // right after initialization
```

Allocates:
```
HEAP ... [ control block ] [ padding (if needed) ] [ buffer for obj ] ... HEAP
```
and returns `shptr*` pointer to the control block. The object is uninitialized at this point, to set a value use [shptr_VAL](#-shptr_val-) or manually dereference [shptr_PTR](#-shptr_ptr-).

Reference counts are set to:

```c
.strong_refcount = 1;
.weak_refcount = 1;
```
the destructor pointer is set (if it was passed), and the returned pointer `shptr*` is meant to be used as the first **strong reference**.

For overaligned objects, the padding might be relatively large. This is because for overaligned objects, the allocated size is:
```c
sizeof(shptr) + _Alignof(type) + sizeof(type)
```
to make sure a correctly aligned address in the block can be given to them.
In certain cases, it might be a better strategy to allocate an overaligned object separately and store the pointer to it. Then the **destructor** can be used to deallocate it.

### Arguments:
- type name (the macro performs `sizeof` and `_Alignof` on it)
- optional:
    - destructor

The destructor must have the signature: 
```c
void destructor_name(void* obj)
```

### Returns:
- `shptr*` ptr to the control block, the first **strong reference**
- `NULL` on allocation failure

<p align="right">
<a href="#api">GO TO API ^</a>
</p>
  
---

### ACCESSING RAW POINTER AND VALUE

---

### ** **shptr_PTR** **

```c
shptr_PTR(reference);        // returns raw void* pointer
shptr_PTR(reference, type);  // returns raw type* pointer

// examples:

void* ptr = shptr_PTR(ref);
int* ptr = shptr_PTR(ref, int)
```

Returns raw pointer to the object. If a type name is passed, it returns pointer of that type, if no type name is passed, it reurns `void*`. 
If the object has been destroyed, `NULL` is returned. If the passed reference is `NULL`, the macro will also return `NULL`.


### Arguments:
- `shptr*` reference
- optional:
    - type name

### Returns:
- `type*` pointer (type name passed)
- `void*` pointer (no type name passed)
- `NULL` (`NULL`-reference passed)

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_VAL** **

```c
shptr_VAL(reference, type); // expands to *(type*)obj_pointer

// examples:

int value = shptr_VAL(ref, int);                    // getting values
double value = shptr_VAL(ref, double);

shptr_VAL(ref, int) = -11;                          // setting values
shptr_VAL(ref, struct s) = (struct s){-11, 0.1234};
```

Macro that performs `*(type*)` on the raw object pointer. There is no safety mechanism in case a `NULL`-reference is passed, so passing a dead reference will result in a crash. This is also the case if a caller doesn't own a **strong reference**, and the object has been destroyed.

### Arguments:
- `shptr*` reference
- type name

### Expands to:
- `*(type*)object_raw_pointer`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ACQUIRING AND RELEASING REFERENCES

---

### ** **shptr_REF** **

```c
shptr_REF(reference); // returns reference copy and increments 
                      // strong refcount by 1 (if it's not 0)

// examples:

some_function(shptr_REF(ref), ...) // leaves no left over copy

shptr* r = shptr_REF(ref);         // leaves left over copy in THIS scope, 
some_function(r, ...);             // r cannot be considered safe to use
                                   // once it's been passed to its owner:
                                   // some_function, and should be NULL'ed

```

Returns a reference copy and increments **strong reference count** by 1 (if it's greater than `0`).

If `NULL` is passed, the underlying function never gets called and `NULL` is returned.

### Arguments:
- `shptr*` reference

### Returns:
- new `shptr*` reference
- `NULL` (`NULL`-reference passed)

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_UNREF** **

```c
shptr_UNREF(reference); // decrements strong refcount by 1
                        // (if it's not 0)
                        // and NULL's the passed reference
                      
// examples:

                  //... an owner is done with the object
                  // and now it releases the reference:

shptr_UNREF(ref); // from now on ref == NULL
                  // and is thus unusable

```

Decrements **strong reference count** by 1 (if it's greater than `0`), and returns `NULL`. Additionaly, the macro `NULL`'s the reference. This is by design to render the reference unusable in subsequent code. 

If **strong reference count** hits `0`: 
- the **destructor** is called (if it isn't set to `NULL`) and the object is destroyed after it returns
- the **implicit weak reference** is taken off and if **weak reference count** hits `0`, the entire block: `[control block]+[object]` gets deallocated.

If `NULL` was passed, the underlying function never gets called and `NULL` is returned.

### Arguments:
- `shptr*` reference

### Returns:
- `NULL`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_REF_WEAK** **

```c
shptr_REF_WEAK(reference); // returns reference copy and increments 
                           // weak refcount by 1

// examples:

some_function(shptr_REF_WEAK(ref), ...) // leaves no left over copy

shptr* r = shptr_REF_WEAK(ref);         // leaves left over copy in THIS scope, 
some_function(r, ...);                  // r cannot be considered safe to use
                                        // once it's been passed to its owner:
                                        // some_function, and should be NULL'ed

```

Returns a reference copy and increments **weak reference count** by 1. 

If `NULL` is passed, the underlying function never gets called and `NULL` is returned.

### Arguments:
- `shptr*` reference

### Returns:
- new `shptr*` reference
- `NULL` (`NULL`-reference passed)

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_UNREF_WEAK** **

```c
shptr_UNREF_WEAK(reference); // decrements weak refcount by 1
                             // (if it's not 0)
                             // and NULL's the passed reference
                      
// examples:

//... a non-owner is done with the object
// and now it releases the reference:

shptr_UNREF_WEAK(weak_ref); // from now on weak_ref == NULL
                            // and is thus unusable

```

Decrements **weak reference count** by 1 (if it's greater than `0`), and returns `NULL`. Additionaly, the macro `NULL`'s the reference. This is by design to render the reference unusable in subsequent code.

If **weak reference count** is `0` and the **strong reference count** is also `0`, the entire block: `[control block]+[object]` gets deallocated.
If `NULL` was passed, the underlying function never gets called and `NULL` is returned.

### Arguments:
- `shptr*` reference

### Returns:
- `NULL`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_REF_TRY** **

```c
shptr_REF_TRY(reference);

// examples:

shptr* acquired = shptr_REF_TRY(weak_ref); // attempting to acquire ownership

if ( acquired == NULL )                    // if the attempt failed ...
    ...
```

Attempts to take the ownership by acquiring a **strong** reference. Can succeed only if the **strong reference count** is already greater than `0`, meaning the object is still alive. 

It does not change **weak reference** into a **strong reference**, instead:
- when acquisition succeeds:
    - a copy of a reference is returned and **strong reference count** is incremented by `1`
- when acquisition fails:
    - `NULL` is returned
    
It should always be checked whether the reference was acquired before using it.
If the macro is given a `NULL`-reference, it returns `NULL`.

### Arguments:
- `shptr*` reference

### Returns:
- `shptr*` new **strong** reference (successful acquisition)
- `NULL` if the object is already gone (failed acquisition)
- `NULL` if the passed reference is `NULL`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### SETTING THE DESTRUCTOR

---

### ** **shptr_DTOR** **

```c
shptr_DTOR(reference);

// examples

shptr_DTOR(ref) = some_dtor; // setting destructor to some_dtor

shptr_DTOR(ref) = NULL;      // setting destructor to NULL
```

Sets the destructor for an object. The destructor (non-`NULL` case) must have the signature: 
```c
void destructor(void* obj);
```
it is passed the raw pointer to the object when called.

The macro has no safety mechanism in case `NULL`-reference is passed as it performs the dereference of a pointer to the `shptr` destructor field.

### Arguments:
- `shptr*` reference

### Expands to:
- `*(destructor_field_ptr)`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_SET_DTOR** **

```c
shptr_SET_DTOR(reference, destructor_ptr);

// examples

shptr_SET_DTOR(ref, some_dtor); // setting destructor to some_dtor

shptr_SET_DTOR(ref, NULL);      // setting destructor to NULL
```

Sets the destructor for an object and returns object's raw pointer. The destructor (non-`NULL` case) must have the signature: 
```c
void destructor(void* obj);
```
it is passed the raw pointer to the object when called.

If **shptr_SET_DTOR** macro is passed a `NULL`-reference, the underlying function never gets called and `NULL` is returned.

### Arguments:
- `shptr*` reference
- destructor pointer

### Returns:
- `void*` raw object pointer 
- `NULL` if passed reference was `NULL`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### INFO

---

### ** **shptr_STRONG_COUNT** **

```c
shptr_STRONG_COUNT(reference);

// examples

size_t strong = shptr_STRONG_COUNT(ref);  // returns strong reference count
size_t strong = shptr_STRONG_COUNT(NULL); // returns SIZE_MAX
```

Returns **strong reference count**. If passed reference is `NULL`, it returns `SIZE_MAX`.

### Arguments:
- `shptr*` reference

### Returns:
- `size_t` strong reference count 
- `size_t SIZE_MAX` if passed reference was `NULL`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_WEAK_COUNT** **

```c
shptr_WEAK_COUNT(reference);

// examples

size_t strong = shptr_WEAK_COUNT(ref);  // returns strong reference count
size_t strong = shptr_WEAK_COUNT(NULL); // returns SIZE_MAX
```

Returns **strong reference count**. If passed reference is `NULL`, it returns `SIZE_MAX`.

### Arguments:
- `shptr*` reference

### Returns:
- `size_t` strong reference count 
- `size_t SIZE_MAX` if passed reference was `NULL`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_ISGONE** **

```c
shptr_ISGONE(reference);

// examples

bool is_gone = shptr_ISGONE(ref);  // TRUE if the object 
                                   // has been destroyed
                                   // FALSE if the object 
                                   // is still alive

bool is_gone = shptr_ISGONE(NULL); // evaluates to TRUE
                                  
if ( shptr_ISGONE(ref) )
{
    ...

```

Evaluates to `true` if the object has been destroyed, and to `false` if the object is still alive. If the macro is passed a `NULL`-reference, it evaluates to `true`. 

### Arguments:
- `shptr*` reference

### Expands to:
- `( ref == NULL || strong_refcount == 0 )`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---

### ** **shptr_ISNULL** **

```c
shptr_ISNULL(reference);

// examples

bool is_null = shptr_ISNULL(NULL); // returns TRUE
                                  
if ( shptr_ISNULL(ref) )
{
    ...

```

Defined simply as:
```c
#define shptr_ISNULL(sh_ptr) ( sh_ptr == NULL )
```

### Arguments:
- `shptr*` reference

### Expands to:
- `(ref == NULL)`

<p align="right">
<a href="#api">GO TO API ^</a>
</p>

---
