#ifndef __DUMMY_ULTRA64_H__
#define __DUMMY_ULTRA64_H__

/*
 * Minimal libultra types for the MIPS patches cross-compile.
 * ultramodern's ultra64.h is C++-only (it refers to struct types without their
 * tags), so patches can't use it from C. WCW has no decomp headers to borrow
 * (BMHero patches use lib/bmhero's), so this defines just the libultra primitive
 * types. Add OS structs (OSMesgQueue etc.) here as patches come to need them —
 * mirror the layouts in lib/N64ModernRuntime/ultramodern/include/ultramodern/ultra64.h.
 */

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float  f32;
typedef double f64;

typedef s32 OSPri;
typedef s32 OSId;
typedef u32 OSEvent;
typedef u32 OSIntMask;
typedef u32 OSPageMask;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
