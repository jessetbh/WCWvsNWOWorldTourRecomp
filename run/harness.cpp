// Minimal standalone harness to TRY executing the recompiled WCW boot code.
//
// This is NOT the real runtime (no RT64, no ultramodern OS, no DMA/threads). It exists
// only to answer "what happens if we just run it?". It:
//   * allocates an 8 MB simulated RDRAM,
//   * loads the boot segment from the ROM into RDRAM (byte-swapped to host order),
//   * builds a vram->function lookup from the generated section_table,
//   * stubs the handful of runtime hooks recomp.h declares,
//   * calls recomp_entrypoint and watches how far execution gets.
//
// Expected outcome: it runs real recompiled code until it hits something this toy
// runtime doesn't provide (an OS service, hardware poll, overlay load, or bad access),
// then halts with a report. That divergence point is the interesting result.

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <csetjmp>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "recomp.h"
#include "funcs.h"
}
#include "recomp_overlays.inl"   // defines section_table, num_sections

// --- function-entry instrumentation (the funcs are compiled with -finstrument-functions)
static std::unordered_map<void*, uint32_t> g_fnmap;   // func ptr -> vram
static std::atomic<void*>    g_cur_fn{nullptr};
static std::atomic<long long> g_enters{0};

extern "C" __attribute__((no_instrument_function))
void __cyg_profile_func_enter(void* this_fn, void*) {
    g_cur_fn.store(this_fn, std::memory_order_relaxed);
    g_enters.fetch_add(1, std::memory_order_relaxed);
}
extern "C" __attribute__((no_instrument_function))
void __cyg_profile_func_exit(void*, void*) {}

__attribute__((no_instrument_function))
static uint32_t fn_vram(void* fn) {
    auto it = g_fnmap.find(fn);
    return it != g_fnmap.end() ? it->second : 0;
}

// Periodically report the currently-executing function so we can see where an infinite
// loop is spinning (a tight poll loop makes no calls, so the lookup trace can't show it).
__attribute__((no_instrument_function))
static void watchdog() {
    for (int i = 0; i < 9; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        printf("[watchdog +%ds] func entries=%lld, currently in vram 0x%08X\n",
               (i + 1) * 2, (long long)g_enters.load(), fn_vram(g_cur_fn.load()));
    }
}

#define RDRAM_SIZE 0x800000u     // 8 MB; memory macros mask & 0x7FFFFF

static uint8_t* g_rdram = nullptr;
static recomp_context g_ctx;
static jmp_buf g_jmp;
static const char* g_reason = "?";
static uint32_t g_vram = 0;
static long g_lookups = 0;
static long g_unresolved = 0;

// recomp.h expects this symbol (used by RELOC_* macros; unused for fixed-addr code).
extern "C" { int32_t* section_addresses = nullptr; }

static void halt(const char* reason, uint32_t vram) {
    g_reason = reason; g_vram = vram;
    longjmp(g_jmp, 1);
}

// ---- runtime hooks declared in recomp.h ----
extern "C" {
void cop0_status_write(recomp_context* ctx, gpr value) { ctx->status_reg = (uint32_t)value; }
gpr  cop0_status_read(recomp_context* ctx)             { return (gpr)(int32_t)ctx->status_reg; }
void switch_error(const char* func, uint32_t vram, uint32_t jtbl) {
    printf("[switch_error] in %s vram=0x%08X jtbl=0x%08X\n", func, vram, jtbl);
    halt("switch_error (unhandled jump table)", vram);
}
void do_break(uint32_t vram)                                       { halt("break instruction", vram); }
void recomp_syscall_handler(uint8_t*, recomp_context*, int32_t v)  { halt("syscall", (uint32_t)v); }
void pause_self(uint8_t*)                                          { }
}

// Fallback when a looked-up vram isn't in a loaded section (e.g. an overlay function
// that the real runtime would have DMA'd in, or genuinely unknown code).
static void unmapped_func(uint8_t*, recomp_context*) {
    halt("called function in an unloaded section (overlay?)", g_vram);
}

extern "C" recomp_func_t* get_function(int32_t vram) {
    uint32_t v = (uint32_t)vram;
    g_vram = v;
    g_lookups++;
    // Only sections 0 (entry) and 1 (main) are "resident"; overlays aren't loaded.
    for (size_t s = 0; s < num_sections && s < 2; s++) {
        SectionTableEntry& sec = section_table[s];
        if (v >= sec.ram_addr && v < sec.ram_addr + sec.size) {
            uint32_t off = v - sec.ram_addr;
            for (size_t i = 0; i < sec.num_funcs; i++) {
                if (sec.funcs[i].offset == off) {
                    if (g_lookups <= 60)
                        printf("[lookup %2ld] 0x%08X -> %s[%zu]\n", g_lookups, v,
                               s == 0 ? "entry" : "main", i);
                    return sec.funcs[i].func;
                }
            }
        }
    }
    g_unresolved++;
    if (g_unresolved <= 20)
        printf("[lookup] UNRESOLVED 0x%08X (overlay/unmapped)\n", v);
    return unmapped_func;
}

static void on_segv(int) {
    g_reason = "SIGSEGV (out-of-bounds memory access)";
    longjmp(g_jmp, 1);
}

static uint8_t* load_file(const char* path, long* size_out) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("ERROR: cannot open %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc(n);
    fread(buf, 1, n, f); fclose(f);
    if (size_out) *size_out = n;
    return buf;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);   // unbuffered: see trace right up to a crash
    const char* rom_path = argc > 1 ? argv[1] : "wcw.z64";

    long romsize = 0;
    uint8_t* rom = load_file(rom_path, &romsize);
    printf("Loaded ROM %s (%ld bytes)\n", rom_path, romsize);

    g_rdram = (uint8_t*)calloc(RDRAM_SIZE, 1);

    // The IPL3 boot loader copies the boot segment ROM[0x1000..0x393F0) to vram
    // 0x80000400 (rdram offset 0x400). N64 RAM is stored byte-swapped vs. the
    // big-endian ROM, so swap each 32-bit word as we copy.
    const uint32_t rom_start = 0x1000, rom_end = 0x393F0, ram_off = 0x400;
    for (uint32_t i = 0; i + 4 <= (rom_end - rom_start); i += 4) {
        uint8_t* s = rom + rom_start + i;
        uint8_t* d = g_rdram + ram_off + i;
        d[0] = s[3]; d[1] = s[2]; d[2] = s[1]; d[3] = s[0];
    }
    printf("Loaded boot segment (0x%X bytes) to vram 0x80000400\n", rom_end - rom_start);

    // Build the func-ptr -> vram reverse map for the watchdog/instrumentation.
    for (size_t s = 0; s < num_sections; s++) {
        SectionTableEntry& sec = section_table[s];
        for (size_t i = 0; i < sec.num_funcs; i++)
            g_fnmap[(void*)sec.funcs[i].func] = sec.ram_addr + sec.funcs[i].offset;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));
    signal(SIGSEGV, on_segv);
    signal(SIGILL, on_segv);
    signal(SIGFPE, on_segv);

    printf("\n--- phase 1: calling recomp_entrypoint (boot) ---\n");
    if (setjmp(g_jmp) == 0) {
        recomp_entrypoint(g_rdram, &g_ctx);
        printf("\n=== boot RETURNED normally ===\n");
    } else {
        printf("\n=== boot HALTED: %s (vram 0x%08X) ===\n", g_reason, g_vram);
    }
    printf("    indirect lookups: %ld (unresolved: %ld)\n", g_lookups, g_unresolved);

    // Phase 2: the boot created (but the stubbed scheduler never dispatched) the game
    // thread, whose entry is func_800004AC. Invoke it directly on a fresh stack to see
    // how far the actual game logic runs before it needs something we don't provide.
    printf("\n--- phase 2: invoking game thread entry func_800004AC ---\n");
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.r29 = (gpr)(int32_t)0x80380000;   // fresh thread stack top (within 8 MB RDRAM)
    g_lookups = 0; g_unresolved = 0;
    std::thread(watchdog).detach();   // reports where execution is every 2s
    if (setjmp(g_jmp) == 0) {
        func_800004AC(g_rdram, &g_ctx);
        printf("\n=== game thread RETURNED normally ===\n");
    } else {
        printf("\n=== game thread HALTED: %s (vram 0x%08X) ===\n", g_reason, g_vram);
    }
    printf("    indirect lookups: %ld (unresolved: %ld)\n", g_lookups, g_unresolved);
    return 0;
}

extern "C" void func_800004AC(uint8_t* rdram, recomp_context* ctx);
