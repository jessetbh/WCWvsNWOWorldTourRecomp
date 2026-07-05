#include <cstdio>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"

// Game-side runtime API callable from MIPS patches (adapted from BMHero's
// src/game/recomp_api.cpp). Every function here needs a matching address entry in
// patches/syms.ld and is exposed to patches via the generated manual_patch_symbols
// table — only implement (and list in syms.ld) what patches actually use.

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stdout);
    }
    // Redirected stdout is fully buffered; without a flush, output sits in the CRT
    // buffer and is lost if the process is killed (this is a debugging API — flush).
    fflush(stdout);
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}
