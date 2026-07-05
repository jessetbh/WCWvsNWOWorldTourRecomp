#ifndef __DUMMY_STDINT_H__
#define __DUMMY_STDINT_H__

/*
 * Minimal freestanding stdint.h for the MIPS patches cross-compile.
 * The Makefile builds with -nostdinc (no host headers, no clang builtins), but
 * ultramodern's ultra64.h includes <stdint.h>. Clang predefines the __*_TYPE__
 * macros correctly for -target mips -mabi=32, so just alias them.
 */

typedef __INT8_TYPE__   int8_t;
typedef __INT16_TYPE__  int16_t;
typedef __INT32_TYPE__  int32_t;
typedef __INT64_TYPE__  int64_t;
typedef __UINT8_TYPE__  uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

#define INT8_MAX   0x7F
#define INT16_MAX  0x7FFF
#define INT32_MAX  0x7FFFFFFF
#define INT64_MAX  0x7FFFFFFFFFFFFFFFLL
#define UINT8_MAX  0xFFU
#define UINT16_MAX 0xFFFFU
#define UINT32_MAX 0xFFFFFFFFU
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL

#endif
