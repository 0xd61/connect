/* dgl_test_helpers.h - v0.1
   No warranty is offered or implied; Use this code at your own risk

   This file is written mostly for my self and a work in progress!

LICENSE
   This software is dual-licensed (MIT and public domain) -- See the LICENSE file in this repository
   (https://github.com/0xd61/dgl/blob/main/LICENSE) for more information.

CREDITS
 Written by Daniel Glinka.

 Credits to Sean Barrett and his stb style libraries which inspired this library.
*/

#ifdef DGL_STATIC
#define DGL_DEF static
#else
#define DGL_DEF extern
#endif

#ifndef DGL_TEST_HELPERS_H
#define DGL_TEST_HELPERS_H

#include <stdio.h>

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

#if DGL_OS_WINDOWS
    #include <windows.h>
#elif DGL_OS_UNIX
    #include <time.h>
#else
    #error Testing currently not implemented for OS
#endif

//
// Types
//

typedef unsigned char            dglth_uint8 ;
typedef   signed char             dglth_int8 ;
typedef unsigned short           dglth_uint16;
typedef   signed short            dglth_int16;
typedef unsigned int             dglth_uint32;
typedef   signed int              dglth_int32;
typedef unsigned long long       dglth_uint64;
typedef          long long        dglth_int64;
typedef          float           dglth_real32;
typedef          double          dglth_real64;
typedef          dglth_int32     dglth_bool32;
#define true 1
#define false 0

// TODO(dgl): could we define a intptr without stdint.h?
#include <stdint.h>
typedef uintptr_t dglth_uintptr;
#include <stddef.h>
typedef size_t dglth_usize;

#ifdef __cplusplus
extern "C" {
#endif

static struct
{
    dglth_bool32 active_test_error;
    char *active_test_name;
    dglth_int32 test_count;
    dglth_int32 error_count;
#if DGL_OS_WINDOWS
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
#elif DGL_OS_UNIX
    struct timespec start_time;
    struct timespec end_time;
#endif
    dglth_real32 total_time_in_ms;
} dgl__test_context;

#if DGL_OS_WINDOWS
// TODO(dgl): not tested
DGL_DEF inline LARGE_INTEGER
dgl__get_wall_clock(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return(result);
}

DGL_DEF inline dglth_real32
dgl__get_ms_elapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    LARGE_INTEGER perf_count_frequency;
    QueryPerformanceFrequency(&perf_count_frequency);
    dglth_real32 result = ((dglth_real32)(end.QuadPart - start.QuadPart) * 1e3f /
                     (dglth_real32)perf_count_frequency.QuadPart);
    return(result);
}
#elif DGL_OS_UNIX
DGL_DEF inline struct timespec
dgl__get_wall_clock()
{
    struct timespec result;
    clock_gettime(CLOCK_MONOTONIC, &result);
    return(result);
}

DGL_DEF inline dglth_real32
dgl__get_ms_elapsed(struct timespec start, struct timespec end)
{
    dglth_real32 result = (dglth_real32)(end.tv_sec - start.tv_sec) +
                    ((dglth_real32)(end.tv_nsec - start.tv_nsec) * 1e-6f);
    return(result);
}
#endif

#define dgl__compare_type_full(Type, format, a, op, b)   \
    do { \
        Type typed_a = (a); \
        Type typed_b = (b); \
        if (!(typed_a op typed_b)) {                               \
            printf("=>\tFailed: " format " %s " format " :: %s %s %s\n", \
                   typed_a, #op, typed_b, #a, #op, #b); \
            dgl__test_context.active_test_error = true; \
        } else { \
            printf("\tPassed: %s %s %s\n", #a, #op, #b); \
        } \
    } while (0)

#define DGL_EXPECT(a, op, b, type, format) dgl__compare_type_full(type, format, a, op, b)
#define DGL_EXPECT_uint8(a, op, b) dgl__compare_type_full(dglth_uint8, "0x%.2x", a, op, b)
#define DGL_EXPECT_int8(a, op, b) dgl__compare_type_full(dglth_int8, "0x%.2x", a, op, b)
#define DGL_EXPECT_uint16(a, op, b) dgl__compare_type_full(dglth_uint16, "%u", a, op, b)
#define DGL_EXPECT_int16(a, op, b) dgl__compare_type_full(dglth_int16, "%d", a, op, b)
#define DGL_EXPECT_uint32(a, op, b) dgl__compare_type_full(dglth_uint32, "%u", a, op, b)
#define DGL_EXPECT_int32(a, op, b) dgl__compare_type_full(dglth_int32, "%d", a, op, b)
#define DGL_EXPECT_uint64(a, op, b) dgl__compare_type_full(dglth_uint64, "%lu", a, op, b)
#define DGL_EXPECT_int64(a, op, b) dgl__compare_type_full(dglth_int64, "%ld", a, op, b)
#define DGL_EXPECT_real32(a, op, b) dgl__compare_type_full(dglth_real32, "%f", a, op, b)
#define DGL_EXPECT_real64(a, op, b) dgl__compare_type_full(dglth_real64, "%lf", a, op, b)
#define DGL_EXPECT_bool32(a, op, b) dgl__compare_type_full(dglth_bool32, "%d", a, op, b)
#define DGL_EXPECT_usize(a, op, b) dgl__compare_type_full(dglth_usize, "%lu", a, op, b)
#define DGL_EXPECT_ptr(a, op, b) dgl__compare_type_full(dglth_uintptr, "0x%lx", (uintptr)(a), op, (uintptr)(b))
// TODO(dgl): expect_array types

#define DGL_BEGIN_TEST(name) do { \
        dgl__test_context.active_test_name = (name); \
        dgl__test_context.test_count++; \
        dgl__test_context.active_test_error = false; \
        printf("%s (%s:%d)\n", dgl__test_context.active_test_name, __FILE__, __LINE__); \
        dgl__test_context.start_time = dgl__get_wall_clock(); \
    } while(0)



#define DGL_END_TEST() do { \
        dgl__test_context.end_time = dgl__get_wall_clock(); \
        if(dgl__test_context.active_test_error) { dgl__test_context.error_count++; } \
        dglth_real32 test_duration = dgl__get_ms_elapsed(dgl__test_context.start_time, dgl__test_context.end_time); \
        printf("\t(%f ms)\n", test_duration); \
        dgl__test_context.total_time_in_ms += test_duration; \
    } while(0)

DGL_DEF inline dglth_bool32 dgl_test_result()
{
    printf("Executed %d test(s) in %f ms - Errors: %d\n", dgl__test_context.test_count, dgl__test_context.total_time_in_ms, dgl__test_context.error_count);
    return(dgl__test_context.error_count == 0);
}

#ifdef __cplusplus
}
#endif

#endif // DGL_TEST_HELPERS_H
