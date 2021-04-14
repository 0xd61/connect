/* dgl.h - v0.1
   No warranty is offered or implied; Use this code at your own risk

   This file is written mostly for my self and a work in progress!

 ============================================================================
   You MUST
      #define DGL_IMPLEMENTATION
   in EXACTLY _one_ C or C++ file that includes this header, BEFORE the
   include, like this:
      #define DGL_IMPLEMENTATION
      #include "dgl.h"
   All other files should just #include "dgl.h" without the #define.
 ============================================================================

LICENSE
   This software is dual-licensed (MIT and public domain) -- See the LICENSE file in this repository
   (https://github.com/0xd61/dgl/blob/main/LICENSE) for more information.

CREDITS
 Written by Daniel Glinka.

 Credits to Sean Barrett and his stb style libraries which inspired this library.
*/

/*
   TODOs:
   - handle module deps if a module is disabled (e.g. memory depends on log)
*/

#ifdef DGL_STATIC
#define DGL_DEF static
#else
#define DGL_DEF extern
#endif

#ifndef DGL_H
#define DGL_H

#ifdef __cplusplus
extern "C" {
#endif

//
// Compilers and Platforms
//

#if defined(_WIN32) || defined(_WIN64)
	#ifndef DGL_OS_WINDOWS
	#define DGL_OS_WINDOWS 1
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#ifndef DGL_OS_OSX
	#define DGL_OS_OSX 1
	#endif
#elif defined(__unix__)
	#ifndef DGL_OS_UNIX
	#define DGL_OS_UNIX 1
	#endif
#else
	#error This operating system is not supported
#endif

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#elif __llvm__
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif


//
// Useful defines
//

#define internal static
#define global static
#define local_persist static
#define local_inline static inline

// NOTE(dgl): Casts can be very annoying while debugging. This is to identiy/search for them faster.
#define dgl_cast(type) (type)
#define array_count(array) (sizeof(array) / sizeof((array)[0]))


#if DGL_DEBUG
#include <stdio.h> // fprintf
#define dgl_assert(cond, msg) do                                                   \
{                                                                              \
    if (!(cond))                                                               \
    {                                                                          \
      fprintf(stderr, "Fatal error: %s:%d: dgl_assertion '%s' failed with %s\n",   \
      __FILE__, __LINE__, #cond, #msg);                                        \
      __builtin_trap();                                                        \
    }                                                                          \
} while(0)
#else
#define dgl_assert(cond, msg)
#endif

#define dgl_max(a,b) ((a) > (b) ? (a) : (b))
#define dgl_min(a,b) ((a) < (b) ? (a) : (b))
#define dgl_clamp(x,lo,hi) (dgl_min((hi), dgl_max((lo), (x))))

//
// Types
//

#define kilobytes(value) ((value)*1024LL)
#define megabytes(value) (kilobytes(value)*1024LL)
#define gigabytes(value) (megabytes(value)*1024LL)
#define terabytes(value) (gigabytes(value)*1024LL)

typedef unsigned char      uint8 ;
typedef   signed char       int8 ;
typedef unsigned short     uint16;
typedef   signed short      int16;
typedef unsigned int       uint32;
typedef   signed int        int32;
typedef unsigned long long uint64;
typedef          long long  int64;
typedef          float     real32;
typedef          double    real64;
typedef          int32     bool32;
#define true 1
#define false 0

// TODO(dgl): could we define a intptr without stdint.h?
#include <stdint.h>
typedef uintptr_t uintptr;
#include <stddef.h>
typedef size_t usize;

//
// Safe Truncate
//

DGL_DEF inline uint32
dgl_safe_truncate_uint32(uint64 value)
{
    dgl_assert(value <= 0xFFFFFFFF, "Failed to safely truncate value");
    uint32 result = dgl_cast(uint32)value;
    return(result);
}

DGL_DEF inline int32
dgl_safe_truncate_int32(int64 value)
{
    dgl_assert(value <= 0xFFFFFFFF, "Failed to safely truncate value");
    int32 result = dgl_cast(int32)value;
    return(result);
}

DGL_DEF inline uint32
dgl_safe_size_to_uint32(usize value)
{
    dgl_assert(value <= 0xFFFFFFFF, "Failed to safely truncate value");
    uint32 result = dgl_cast(uint32)value;
    return(result);
}

DGL_DEF inline int32
dgl_safe_size_to_int32(usize value)
{
    dgl_assert(value <= 0x7FFFFFFF, "Failed to safely cast value");
    int32 result = dgl_cast(int32)value;
    return(result);
}

DGL_DEF inline int32
dgl_safe_uint32_to_int32(uint32 value)
{
   dgl_assert(value <= 0x7FFFFFFF, "Failed to safely cast value");
   int32 result = dgl_cast(int32)value;
   return(result);
}

//
// Intrinsics
//
#ifndef DGL_NO_INTRINSICS
#include <math.h>
DGL_DEF inline uint32
dgl_round_real32_to_uint32(real32 value)
{
    dgl_assert(value >= 0.0f, "Number must be positive");
    // TODO(dgl): should we do the faster (uint32)value + 0.5f; ?
    uint32 result = (uint32)roundf(value);
    return(result);
}

DGL_DEF inline int32
dgl_round_real32_to_int32(real32 value)
{
    int32 result = (int32)roundf(value);
    return(result);
}

#if COMPILER_LLVM
DGL_DEF inline uint32
dgl_atomic_compare_exchange_uint32(uint32 volatile *value, uint32 new_val, uint32 expected)
{
    uint32 result = __sync_val_compare_and_swap(value, expected, new_val);
    return(result);
}
DGL_DEF inline int32
dgl_atomic_compare_exchange_int32(int32 volatile *value, int32 new_val, int32 expected)
{
    int32 result = __sync_val_compare_and_swap(value, expected, new_val);
    return(result);
}
DGL_DEF inline uintptr
dgl_atomic_compare_exchange_uintptr(uintptr volatile *value, uintptr new_val, uintptr expected)
{
    uintptr result = __sync_val_compare_and_swap(value, expected, new_val);
    return(result);
}

// TODO(dgl): not tested
#elif COMPILER_MSVC
DGL_DEF inline uint32
dgl_atomic_compare_exchange_uint32(uint32 volatile *value, uint32 new_val, uint32 expected)
{
    uint32 result = _InterlockedCompareExchange((long *)value, new_val, expected);

    return(result);
}
DGL_DEF inline int32
dgl_atomic_compare_exchange_int32(int32 volatile *value, int32 new_val, int32 expected)
{
    int32 result = _InterlockedCompareExchange((long *)value, new_val, expected);
    return(result);
}
DGL_DEF inline uintptr
dgl_atomic_compare_exchange_uintptr(uintptr volatile *value, uintptr new_val, uintptr expected)
{
    uintptr result = _InterlockedCompareExchange(value, new_val, expected);
    return(result);
}
#else
// TODO(dgl): support other compilers
#endif
#endif // DGL_NO_INTRINSICS

//
// Log
//

#ifndef DGL_NO_LOG

// NOTE(dgl): You have to use the dynamic linker because we use a global logger struct.
// To have it working with shared libraries, link with the linker option -rdynamic. Maybe there is
// a better solution for this, but this is how it works currently. Maybe we have to look into it
// IMPORTANT: -rdynamic, and dlopen are not supported on all platforms!
// the future.
// Usage: include the implementation in the main executable and use the definition in the shared lib.

#include <time.h>
#include <stdarg.h>
#include <stdio.h>

// TODO(dgl): Add thread info and maybe execution block
#if DGL_DEBUG
#define DGL_LOG(fmt, ...) dgl__log_internal(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define DGL_LOG_DEBUG(fmt, ...) dgl__log_internal(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define DGL_LOG(fmt, ...) dgl__log_internal(0, -1, fmt, ## __VA_ARGS__)
#define DGL_LOG_DEBUG(...)
#endif

typedef void (*dgl_lock_F)(bool32 lock);
typedef int64 (*dgl_time_in_ms_F)();

void dgl__log_internal(char *file, int32 line, char *fmt, ...);

#endif // DGL_NO_LOG

//
// Memory
//

#ifndef DGL_NO_MEMORY

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

#ifndef dgl_memcpy
#include <string.h> /* memset, memcpy */
#define dgl_memcpy memcpy
#define dgl_memset memset
#endif

typedef usize DGL_Mem_Index;

typedef struct DGL_Mem_Arena
{
    uint8 *base;
    DGL_Mem_Index size;
    DGL_Mem_Index curr_offset;
    // NOTE(dgl): Useful, when we need to resize.
    DGL_Mem_Index prev_offset;
} DGL_Mem_Arena;

typedef struct DGL_Mem_Temp_Arena
{
    DGL_Mem_Arena *arena;
    DGL_Mem_Index curr_offset;
    DGL_Mem_Index prev_offset;
} DGL_Mem_Temp_Arena;

typedef struct DGL_Mem_Pool_Free_Node DGL_Mem_Pool_Free_Node;
struct DGL_Mem_Pool_Free_Node
{
    DGL_Mem_Pool_Free_Node *next;
};

typedef struct DGL_Mem_Pool
{
    uint8 *base;
    DGL_Mem_Index size;
    DGL_Mem_Index chunk_size;
    // TODO(dgl): Does this have to be volatile?
    DGL_Mem_Pool_Free_Node *head;
} DGL_Mem_Pool;

DGL_DEF void dgl_mem_arena_init(DGL_Mem_Arena *arena, uint8 *base, DGL_Mem_Index size);
#define dgl_mem_arena_push_struct(arena, type) (type *)dgl_mem_arena_alloc_align(arena, sizeof(type), DEFAULT_ALIGNMENT)
#define dgl_mem_arena_push_array(arena, type, count) (type *)dgl_mem_arena_alloc_align(arena, (count)*sizeof(type), DEFAULT_ALIGNMENT)
#define dgl_mem_arena_push(arena, size) dgl_mem_arena_alloc_align(arena, size, DEFAULT_ALIGNMENT)
DGL_DEF void * dgl_mem_arena_alloc_align(DGL_Mem_Arena *arena, DGL_Mem_Index size, DGL_Mem_Index align);
#define dgl_mem_arena_resize_array(arena, type, current_base, current_size, new_size) (type *) dgl_mem_arena_resize_align(arena, dgl_cast(uint8 *)(current_base), (current_size)*sizeof(type), (new_size)*sizeof(type), DEFAULT_ALIGNMENT)
#define dgl_mem_arena_resize(arena, current_base, current_size, new_size) dgl_mem_arena_resize_align(arena, current_base, current_size, new_size, DEFAULT_ALIGNMENT)
DGL_DEF void * dgl_mem_arena_resize_align(DGL_Mem_Arena *arena, uint8 *current_base, DGL_Mem_Index current_size, DGL_Mem_Index new_size, usize align);
DGL_DEF void dgl_mem_arena_free_all(DGL_Mem_Arena *arena);
DGL_DEF DGL_Mem_Temp_Arena dgl_mem_arena_begin_temp(DGL_Mem_Arena *arena);
DGL_DEF void dgl_mem_arena_end_temp(DGL_Mem_Temp_Arena temp);

#define dgl_mem_pool_init_struct(arena, base, size, type) dgl_mem_pool_init_align(arena, base, size, sizeof(type), DEFAULT_ALIGNMENT)
#define dgl_mem_pool_init(arena, base, size, chunk_size) dgl_mem_pool_init_align(arena, base, size, chunk_size, DEFAULT_ALIGNMENT)
DGL_DEF void dgl_mem_pool_init_align(DGL_Mem_Pool *arena, uint8 *base, DGL_Mem_Index size, DGL_Mem_Index chunk_size, DGL_Mem_Index chunk_alignment);
DGL_DEF void dgl_mem_pool_free_all(DGL_Mem_Pool *arena);
#define dgl_mem_pool_push(arena, type) (type *)dgl__mem_pool_alloc_internal(arena)
DGL_DEF void * dgl__mem_pool_alloc_internal(DGL_Mem_Pool *arena);
#define dgl_mem_pool_release(arena, ptr) dgl__mem_pool_free_internal(arena)
DGL_DEF void dgl__mem_pool_free_internal(DGL_Mem_Pool *arena, void *ptr);
#define dgl_mem_pool_push_threadsafe(arena, type) (type *)dgl__mem_pool_alloc_threadsafe_internal(arena)
DGL_DEF void * dgl__mem_pool_alloc_threadsafe_internal(DGL_Mem_Pool *arena);
#define dgl_mem_pool_release_threadsafe(arena, ptr) dgl__mem_pool_free_threadsafe_internal(arena, ptr)
DGL_DEF void dgl__mem_pool_free_threadsafe_internal(DGL_Mem_Pool *arena, void *ptr);

#endif // DGL_NO_MEMORY

//
// String
//

#ifndef DGL_NO_STRING

#include <stdarg.h>

typedef struct DGL_String_Builder
{
    DGL_Mem_Arena *arena;
    usize capacity;
    usize count;
    union
    {
       uint8 *data;
       char *string;
    };
} DGL_String_Builder;

DGL_DEF inline usize
dgl_string_length(char *string)
{
    usize count = 0;
    if(string)
    {
        while(*string++) { ++count; }
    }
    return(count);
}

DGL_DEF DGL_String_Builder dgl_string_builder_init(DGL_Mem_Arena *arena, usize capacity);
#define dgl_string_append(builder, fmt, ...) dgl__string_append_internal(builder, fmt, ## __VA_ARGS__)
DGL_DEF void dgl__string_append_internal(DGL_String_Builder *builder, char *fmt, ...);
DGL_DEF char * dgl_string_c_style(DGL_String_Builder *builder);

#endif // DGL_NO_STRING

#ifdef __cplusplus
}
#endif

#endif // DGL_H

//_------------------------------------------------------------------------------------------------
//
// IMPLEMENTATION
//
//-------------------------------------------------------------------------------------------------
#ifdef DGL_IMPLEMENTATION

//
// Log
//
#ifndef DGL_NO_LOG

global struct DGL_Logger
{
    dgl_lock_F lock;
    dgl_time_in_ms_F get_time;
    bool32 initialized;
} dgl_logger;

DGL_DEF void
dgl_log_init_threadsafe(dgl_time_in_ms_F time_func, dgl_lock_F lock_func)
{
    dgl_logger.get_time = time_func;
    dgl_logger.lock = lock_func;
    dgl_logger.initialized = true;
}

DGL_DEF void
dgl_log_init(dgl_time_in_ms_F time_func)
{
    dgl_log_init_threadsafe(time_func, 0);
}


DGL_DEF void
dgl__lock()
{
  if (dgl_logger.lock) { dgl_logger.lock(true); }
}

DGL_DEF void
dgl__unlock()
{
    if (dgl_logger.lock) { dgl_logger.lock(false); }
}

void
dgl__log_internal(char *file, int32 line, char *fmt, ...)
{
    if(dgl_logger.initialized)
    {
       dgl_assert(dgl_logger.get_time, "time_in_ms must be a valid function");
       int64 milliseconds = dgl_logger.get_time();
       int32 seconds = dgl_cast(int32)(milliseconds / 1000);
       milliseconds = milliseconds % 1000;
       int32 minutes = seconds / 60;
       seconds = seconds % 60;
       int32 hours = minutes / 60;
       minutes = minutes % 60;

       dgl__lock();
       if(file)
       {
           fprintf(stdout, "%02d:%02d:%02d.%04lld %s:%d: ", hours, minutes, seconds, milliseconds, file, line);
       }
       else
       {
           fprintf(stdout, "%02d:%02d:%02d.%04lld: ", hours, minutes, seconds, milliseconds);
       }

       va_list ap;
       va_start(ap, fmt);
       vfprintf(stdout, fmt, ap);
       fprintf(stdout, "\n");
       fflush(stdout);
       va_end(ap);
       dgl__unlock();
    }
    else
    {
       //fprintf(stdout, "Failed to write log at %s:%d (Logger must be initialized)\n", file, line);
    }

}

#endif // DGL_NO_LOG

//
// Memory
//
#ifndef DGL_NO_MEMORY

#ifndef DGL_LOG
#define DGL_LOG(...)
#endif

#ifndef DGL_LOG_DEBUG
#define DGL_LOG_DEBUG(...)
#endif

#include <string.h>
DGL_DEF uintptr
dgl__align_forward_uintptr(uintptr base, usize align)
{
    uintptr result = base;

    dgl_assert((align & (align - 1)) == 0, "Alignment has to be a power of two");
    // Same as (base % align) but faster as 'align' is a power of two
    uintptr modulo = base & (dgl_cast(uintptr)align - 1);

    if(modulo != 0)
    {
        result = base + modulo;
    }

    return(result);
}

DGL_DEF DGL_Mem_Index
dgl__align_forward_memory_index(DGL_Mem_Index size, usize align)
{
    DGL_Mem_Index result = size;

    dgl_assert((align & (align - 1)) == 0, "Alignment has to be a power of two");

    DGL_Mem_Index modulo = size & (align - 1);

    if(modulo != 0)
    {
        result = size + modulo;
    }

    return(result);
}

void
dgl_mem_arena_init(DGL_Mem_Arena *arena, uint8 *base, DGL_Mem_Index size)
{
    arena->size = size;
    arena->base = base;
    arena->curr_offset = 0;
    arena->prev_offset = 0;
}

DGL_DEF void *
dgl_mem_arena_alloc_align(DGL_Mem_Arena *arena, DGL_Mem_Index size, usize align)
{
    uintptr curr_ptr = dgl_cast(uintptr)(arena->base + arena->curr_offset);
    uintptr new_ptr = dgl__align_forward_uintptr(curr_ptr, align);

    DGL_Mem_Index offset = dgl_cast(DGL_Mem_Index)(new_ptr - dgl_cast(uintptr)arena->base); // revert back to relative offset

    dgl_assert((offset + size) <= arena->size, "Arena overflow. Cannot allocate size");

    void *result = arena->base + offset;
    arena->prev_offset = offset;
    arena->curr_offset = offset + size;

    // Zero new memory by default (we do not zero the memory on init or free_all)
    dgl_memset(result, 0, size);

    return(result);
}

DGL_DEF void *
dgl_mem_arena_resize_align(DGL_Mem_Arena *arena, uint8 *current_base, DGL_Mem_Index current_size, DGL_Mem_Index new_size, usize align)
{
    void *result = 0;
    dgl_assert(arena->base <= current_base && current_base < arena->base + arena->size, "This allocation does not belong to the arena");

    if(current_size == new_size)
    {
        result = current_base;
    }
    else if(arena->base + arena->prev_offset == current_base)
    {
        dgl_assert((arena->prev_offset + new_size) <= arena->size, "Arena overflow. Cannot allocate new size");
        arena->curr_offset = arena->prev_offset + new_size;
        if (new_size > current_size)
        {
            // Zero the newly allocated memory
            dgl_memset(arena->base + arena->prev_offset + current_size, 0, new_size - current_size);
        }
        result = current_base;
        DGL_LOG_DEBUG("Resize allocation at 0x%p from %d to %d (%d bytes)", current_base, current_size, new_size, new_size - current_size);
    }
    else
    {
        void *new_base = dgl_mem_arena_alloc_align(arena, new_size, align);
        // NOTE(dgl): copy the existing data to the new location
        usize copy_size = new_size < current_size ? new_size : current_size;
        dgl_memcpy(new_base, current_base, copy_size);
        result = new_base;
        DGL_LOG_DEBUG("New allocation for resizing 0x%p from %d to %d (%d bytes). New address is 0x%p", current_base, current_size, new_size, new_size - current_size, new_base);
    }

    return(result);
}

DGL_DEF void
dgl_mem_arena_free_all(DGL_Mem_Arena *arena)
{
    arena->curr_offset = 0;
    arena->prev_offset = 0;
}

DGL_DEF DGL_Mem_Temp_Arena
dgl_mem_arena_begin_temp(DGL_Mem_Arena *arena)
{
    DGL_Mem_Temp_Arena result;
    result.arena = arena;
    result.prev_offset = arena->prev_offset;
    result.curr_offset = arena->curr_offset;
    return(result);
}

DGL_DEF void
dgl_mem_arena_end_temp(DGL_Mem_Temp_Arena temp)
{
    temp.arena->prev_offset = temp.prev_offset;
    temp.arena->curr_offset = temp.curr_offset;
}

DGL_DEF void
dgl_mem_pool_free_all(DGL_Mem_Pool *arena)
{
    DGL_Mem_Index chunk_count = arena->size / arena->chunk_size;

    for(DGL_Mem_Index index = 0; index < chunk_count; ++index)
    {
        void *chunk = arena->base + (arena->chunk_size * index);

        DGL_Mem_Pool_Free_Node *node = dgl_cast(DGL_Mem_Pool_Free_Node *)(chunk);
        node->next = arena->head;
        arena->head = node;
    }
}

DGL_DEF void
dgl_mem_pool_init_align(DGL_Mem_Pool *arena, uint8 *base, DGL_Mem_Index size, DGL_Mem_Index chunk_size, usize chunk_alignment)
{
    uintptr initial_base = (uintptr)base;
    uintptr new_base = dgl__align_forward_uintptr(initial_base, chunk_alignment);
    size -= dgl_cast(DGL_Mem_Index)(new_base - initial_base);

    // NOTE(dgl): Align the size of each memory chunk
    DGL_Mem_Index aligned_chunk_size = dgl__align_forward_memory_index(chunk_size, chunk_alignment);

    // NOTE(dgl): dgl_assert if everything fits
    dgl_assert(chunk_size >= sizeof(DGL_Mem_Pool_Free_Node), "Chunk size is too small");
    dgl_assert(size >= chunk_size, "Backing buffer length is smaller than the chunk size");

    arena->size = size;
    arena->base = dgl_cast(uint8 *)new_base;
    arena->chunk_size = aligned_chunk_size;
    arena->head = 0;

    dgl_mem_pool_free_all(arena);
}

DGL_DEF void *
dgl__mem_pool_alloc_internal(DGL_Mem_Pool *arena)
{
    void *result = 0;
    DGL_Mem_Pool_Free_Node *node = arena->head;

    if(node) {
        result = node;
        arena->head->next = arena->head->next;
        dgl_memset(result, 0, arena->chunk_size);
    }
    else
    {
        // TODO(dgl): logging
        dgl_assert(!node, "No free node in memory pool");
    }

    return(result);
}

DGL_DEF void
dgl__mem_pool_free_internal(DGL_Mem_Pool *arena, void *ptr)
{
    dgl_assert((ptr >= dgl_cast(void *)arena->base) &&
           (ptr < dgl_cast(void *)(arena->base + arena->size)), "Pointer is not in memory pool range");

    DGL_Mem_Pool_Free_Node *node = dgl_cast(DGL_Mem_Pool_Free_Node *)ptr;
    node->next = arena->head;
    arena->head = node;
}

DGL_DEF void *
dgl__mem_pool_alloc_threadsafe_internal(DGL_Mem_Pool *arena)
{
    void *result = 0;
    DGL_Mem_Pool_Free_Node *node = arena->head;

    if(node) {
        uintptr old_head = dgl_cast(uintptr)node;
        uintptr new_head = dgl_cast(uintptr)arena->head->next;
        uintptr head = dgl_cast(uintptr)arena->head;

        if(dgl_atomic_compare_exchange_uintptr(&head,
           new_head,
           old_head) == old_head)
        {
            result = node;
            dgl_memset(result, 0, arena->chunk_size);
        }
        else
        {
           // TODO(dgl): logging
            printf("compare exchange failed.");
        }
    }
    else
    {
        // TODO(dgl): logging
        dgl_assert(!node, "No free node in memory pool");
    }

    return(result);
}

DGL_DEF void
dgl__mem_pool_free_threadsafe_internal(DGL_Mem_Pool *arena, void *ptr)
{
    dgl_assert((ptr >= dgl_cast(void *)arena->base) &&
           (ptr < dgl_cast(void *)(arena->base + arena->size)), "Pointer is not in memory pool range");

    DGL_Mem_Pool_Free_Node *node = dgl_cast(DGL_Mem_Pool_Free_Node *)ptr;

    uintptr old_head;
    uintptr new_head;
    uintptr head;
    do
    {
        node->next = arena->head;
        head = dgl_cast(uintptr)arena->head;
        old_head = head;
        new_head = dgl_cast(uintptr)node;
    } while(dgl_atomic_compare_exchange_uintptr(&head,
            new_head,
            old_head) != old_head);
}

#endif // DGL_NO_MEMORY

//
//  String
//

#ifndef DGL_NO_STRING

DGL_DEF DGL_String_Builder
dgl_string_builder_init(DGL_Mem_Arena *arena, usize capacity)
{
    DGL_String_Builder result = {};
    result.arena = arena;
    result.data = dgl_mem_arena_push_array(arena, uint8, capacity);
    result.count = 0;
    result.capacity = capacity;

    return(result);
}

DGL_DEF void
dgl__string_append_internal(DGL_String_Builder *builder, char *fmt, ...)
{
    DGL_Mem_Arena *a = builder->arena;
    va_list ap;
retry:
    va_start(ap, fmt);
    int32 size = vsnprintf(dgl_cast(char *)(builder->data + builder->count), builder->capacity - builder->count, fmt, ap);
    if(size < 0)
    {
        DGL_LOG("Failed to append string to %p", builder->data);
    }
    else
    {
        usize required_cap = builder->count + dgl_cast(usize)size;
        if(builder->capacity <= required_cap)
        {
            usize new_capacity = builder->capacity;
            while(new_capacity <= required_cap)
            {
                new_capacity *= 2;
            }
            // NOTE(dgl): If the current buffer size is too small,
            // we resize the buffer with double the size.

            builder->data = dgl_cast(uint8 *)dgl_mem_arena_resize(a, builder->data, builder->capacity, new_capacity);
            builder->capacity = new_capacity;
            goto retry;
        }
        builder->count += dgl_cast(usize)size;
        builder->data[builder->count] = '\0';
    }
}

DGL_DEF char *
dgl_string_c_style(DGL_String_Builder *builder)
{
   char *result = builder->string;
   return(result);
}

#endif // DGL_NO_STRING

#endif // DGL_IMPLEMENTATION
