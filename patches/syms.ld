/*
 * Maps runtime API calls made from patches (recomp_*, *_recomp) to dummy VRAM
 * addresses. The recompiler turns a call to one of these addresses into a call to
 * the matching runtime function. Add an entry here for every runtime API a patch calls.
 *
 * Adapted from BMHero's syms.ld. The exact address values are arbitrary but must be
 * unique and stay consistent with how the runtime registers them. Extend as needed.
 */

__start = 0x80000000;

/* Core recomp runtime API */
recomp_puts                  = 0x8F000000;
recomp_exit                  = 0x8F000004;
recomp_load_overlays         = 0x8F000008;
osInvalDCache_recomp         = 0x8F00000C;
osInvalICache_recomp         = 0x8F000010;
osWritebackDCache_recomp     = 0x8F000014;
osWritebackDCacheAll_recomp  = 0x8F000018;
recomp_load_overlays_by_rom  = 0x8F000020;
osSendMesg_recomp            = 0x8F000024;
osGetCount_recomp            = 0x8F000028;
osCreateMesgQueue_recomp     = 0x8F00002C;
osRecvMesg_recomp            = 0x8F000030;
osDpSetStatus_recomp         = 0x8F000034;
recomp_error                 = 0x8F000060;
memcpy_recomp                = 0x8F000064;
__divdi3_recomp              = 0x8F000068;
__udivdi3_recomp             = 0x8F00006C;
__umoddi3_recomp             = 0x8F000070;
strlen_recomp                = 0x8F000074;
osVirtualToPhysical_recomp   = 0x8F000078;
osPiStartDma_recomp          = 0x8F00007C;
recomp_abort                 = 0x8F000080;
recomp_get_target_aspect_ratio = 0x8F000084;
recomp_xxh3                  = 0x8F00008C;

/* Add more here as patches require them (input, audio, UI callbacks, etc.). */
