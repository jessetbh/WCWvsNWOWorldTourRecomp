#include "patches.h"

/*
 * Required boot/runtime patches for WCW vs. nWo World Tour: Recompiled.
 *
 * STUB. In BMHero this file holds the patched ROM->RAM loader that hooks overlay
 * loading (load_from_rom_to_addr -> recomp_load_overlays) plus boot/thread fixups.
 * Those depend on the game's overlay/segment layout, which is unknown until the ROM
 * is disassembled (CLAUDE.md, Phase 1). Fill this in once overlays are mapped.
 *
 * Patches use the RECOMP_PATCH macro to override an original function by name, and
 * RECOMP_EXPORT to expose a symbol to mods. Both are provided by recomp.h/ultra64.h
 * pulled in through patches.h.
 *
 * Example skeleton (kept compiling but inert until real symbols exist):
 */

// RECOMP_PATCH void some_game_func(void) {
//     // replacement implementation
// }
