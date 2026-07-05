// Minimal stand-in for librecomp/sections.h, just enough to compile the generated
// recomp_overlays.inl in our standalone experiment harness.
#ifndef __HARNESS_SECTIONS_H__
#define __HARNESS_SECTIONS_H__

#include <stddef.h>
#include <stdint.h>
#include "recomp.h"

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    recomp_func_t* func;
    uint32_t offset;    // offset of this function from the section's ram_addr
    uint32_t rom_size;
} FuncEntry;

typedef struct {
    uint32_t rom_addr;
    uint32_t ram_addr;
    uint32_t size;
    FuncEntry* funcs;
    size_t num_funcs;
    void* relocs;
    size_t num_relocs;
    uint32_t index;
} SectionTableEntry;

#endif
