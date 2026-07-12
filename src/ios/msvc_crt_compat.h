#pragma once
// MSVC CRT spellings → libc, for iOS. The decomp calls the underscore-prefixed
// MSVC names at ~1000 sites (_stricmp alone: 816); libc has the POSIX ones.
// Macro aliases keep every call site untouched.
//
// Semantics note: MSVC _snprintf does NOT NUL-terminate on overflow and
// returns -1; C99 snprintf always terminates and returns the would-be length.
// The engine defensively writes buf[len-1]=0 after most calls, so the safer
// C99 behavior is acceptable here. Same for _vsnprintf/vsnprintf.
#ifdef KISAK_IOS

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cerrno>
#include <strings.h> // strcasecmp / strncasecmp

#define _stricmp   strcasecmp
#define stricmp    strcasecmp
#define _strnicmp  strncasecmp
#define strnicmp   strncasecmp
#define _snprintf  snprintf
#define _vsnprintf vsnprintf
#define _strdup    strdup
#define _isnan(x)  std::isnan(x)

// No libc equivalents — implemented in ios/msvc_crt_compat.cpp
char *q_ios_strlwr(char *s);
char *q_ios_strupr(char *s);
char *q_ios_itoa(int value, char *str, int radix);
#define _strlwr q_ios_strlwr
#define strlwr  q_ios_strlwr
#define _strupr q_ios_strupr
#define strupr  q_ios_strupr
#define _itoa   q_ios_itoa

// x86 timestamp counter → ARM64 virtual counter (fixed-frequency, monotonic;
// the engine only ever uses __rdtsc deltas for profiling/jitter, never wall time)
static inline unsigned long long q_ios_rdtsc(void)
{
    unsigned long long v;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}
#define __rdtsc q_ios_rdtsc

// MSVC secure-CRT variants (a handful of sites tree-wide; counts in git log).
// snprintf both truncates and NUL-terminates, which is the _TRUNCATE behavior.
#define _TRUNCATE ((size_t)-1)
static inline int q_ios_fopen_s(FILE **f, const char *name, const char *mode)
{
    *f = fopen(name, mode);
    return *f ? 0 : errno;
}
#define fopen_s q_ios_fopen_s
#define sprintf_s(buf, size, ...)              snprintf(buf, size, __VA_ARGS__)
#define _vsnprintf_s(buf, size, count, fmt, ap) vsnprintf(buf, size, fmt, ap)
#define strcpy_s(dst, size, src)               ((void)strlcpy(dst, src, size))
#define strcat_s(dst, size, src)               ((void)strlcat(dst, src, size))

// Win32 Interlocked* → GCC/clang atomics. Win32 semantics preserved exactly:
// Increment/Decrement return the NEW value; Exchange/ExchangeAdd/
// CompareExchange return the OLD value. Templated because the decomp calls
// these on volatile int / unsigned / LONG lvalues interchangeably.
template <typename T>
static inline T InterlockedIncrement(volatile T *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
template <typename T>
static inline T InterlockedDecrement(volatile T *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
template <typename T>
static inline T InterlockedExchange(volatile T *p, T v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
template <typename T>
static inline T InterlockedExchangeAdd(volatile T *p, T v) { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
template <typename T>
static inline T InterlockedCompareExchange(volatile T *p, T exchange, T comparand)
{
    __atomic_compare_exchange_n(p, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}

#endif // KISAK_IOS
