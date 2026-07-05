/*
 * Maps runtime API calls made from patches (recomp_*, *_recomp) to dummy VRAM
 * addresses. The recompiler turns a call to one of these addresses into a call to
 * the matching runtime function, and every entry ends up referenced by name in the
 * generated manual_patch_symbols table — so ONLY list symbols that actually exist
 * in the linked runtime (librecomp/ultramodern), or the exe won't link.
 *
 * BMHero's syms.ld additionally lists its own game-side API (recomp_load_overlays,
 * recomp_error, memcpy_recomp, recomp_xxh3, ...) implemented in its src/game glue;
 * add such entries here only once we implement the matching function in src/.
 * Address values are arbitrary but must be unique.
 */

__start = 0x80000000;

/* Core recomp runtime API (librecomp) */
recomp_puts                  = 0x8F000000;
recomp_exit                  = 0x8F000004;
osInvalDCache_recomp         = 0x8F00000C;
osInvalICache_recomp         = 0x8F000010;
osWritebackDCache_recomp     = 0x8F000014;
osWritebackDCacheAll_recomp  = 0x8F000018;
osSendMesg_recomp            = 0x8F000024;
osGetCount_recomp            = 0x8F000028;
osCreateMesgQueue_recomp     = 0x8F00002C;
osRecvMesg_recomp            = 0x8F000030;
osDpSetStatus_recomp         = 0x8F000034;
__divdi3_recomp              = 0x8F000068;
__udivdi3_recomp             = 0x8F00006C;
__umoddi3_recomp             = 0x8F000070;
osVirtualToPhysical_recomp   = 0x8F000078;
osPiStartDma_recomp          = 0x8F00007C;

/* Add more here as patches require them (input, audio, UI callbacks, etc.). */
