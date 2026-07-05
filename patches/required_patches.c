#include "patches.h"

/*
 * Required boot/runtime patches for WCW vs. nWo World Tour: Recompiled.
 *
 * Unlike BMHero, WCW needs no overlay-loader patch: librecomp's PI DMA path detects
 * DMAs into the two registered fixed-address overlay sections and loads them itself,
 * which is how the port has run since Phase 3. So this file is currently empty.
 *
 * Patches use RECOMP_PATCH to override an original function by name (names come from
 * syms/dump.toml; data symbols resolve against syms/data_dump.toml), and RECOMP_EXPORT
 * to expose a symbol to mods. See patches.h.
 *
 * (The two pipeline-verification patches that used to live here — func_80000644 and
 * func_80013BF0/sqrtf, each with a one-shot stdout marker — were removed once the
 * first real patch landed: patches/widescreen.c, verified in-game 2026-07-05.)
 */
