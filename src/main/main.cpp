// Entry point for WCW vs. nWo World Tour: Recompiled.
// Minimal launcher: sets up SDL + RT64 (via recompui) + the runtime callbacks, registers
// the game, auto-loads the ROM (no UI launcher yet), and calls recomp::start.
// Adapted from Bomberman Hero: Recompiled's src/main/main.cpp.
#include <cstdio>
#include <cassert>
#include <vector>
#include <array>
#include <filesystem>
#include <thread>
#include <chrono>

#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_syswm.h"
#include "nfd.h"

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "recompui/recompui.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "recompui/program_config.h"
#include "recompinput/recompinput.h"
#include "recompinput/profiles.h"
#include "recompinput/input_events.h"
#include "recompinput/players.h"
#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "ovl_patches.hpp"
#ifdef _WIN32
#include "renderdoc_app.h" // [wcw] RenderDoc in-app capture API (diagnostic)
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <dbghelp.h>
#include <exception>
#include <cstdlib>
#pragma comment(lib, "dbghelp.lib")

// DIAGNOSTIC: print one resolved stack frame (module!symbol+off or module+off).
static void wcw_print_frame(HANDLE proc, int i, DWORD64 addr) {
    char symbuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 511;
    char modname[MAX_PATH] = "?";
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &mod) && mod) {
        char full[MAX_PATH];
        if (GetModuleFileNameA(mod, full, MAX_PATH)) {
            const char* b = strrchr(full, '\\'); strncpy(modname, b ? b + 1 : full, MAX_PATH - 1);
        }
    }
    DWORD64 disp = 0;
    if (SymFromAddr(proc, addr, &disp, sym)) {
        fprintf(stderr, "  #%2d  %-20s  %s+0x%llX  (0x%llX)\n", i, modname, sym->Name, (unsigned long long)disp, (unsigned long long)addr);
    } else {
        DWORD64 modbase = mod ? (DWORD64)mod : 0;
        fprintf(stderr, "  #%2d  %-20s  +0x%llX  (0x%llX)\n", i, modname, (unsigned long long)(addr - modbase), (unsigned long long)addr);
    }
}

// DIAGNOSTIC: dump a backtrace + in-flight C++ exception when std::terminate fires.
static void wcw_terminate_handler() {
    fprintf(stderr, "\n==== [wcw] std::terminate called ====\n");
    if (auto ep = std::current_exception()) {
        try { std::rethrow_exception(ep); }
        catch (const std::exception& e) { fprintf(stderr, "[wcw] in-flight exception: %s: %s\n", typeid(e).name(), e.what()); }
        catch (...) { fprintf(stderr, "[wcw] in-flight exception: non-std type\n"); }
    } else {
        fprintf(stderr, "[wcw] no in-flight C++ exception (pure-virtual call / explicit abort?)\n");
    }
    void* frames[64];
    USHORT n = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(proc, nullptr, TRUE);
    for (USHORT i = 0; i < n; i++) wcw_print_frame(proc, i, (DWORD64)frames[i]);
    fprintf(stderr, "==== [wcw] end backtrace ====\n");
    fflush(stderr);
    _exit(2);
}

// DIAGNOSTIC: walk the faulting thread's stack for hard faults (access violations etc.),
// which don't go through std::terminate.
static LONG WINAPI wcw_unhandled_filter(EXCEPTION_POINTERS* ep) {
    fprintf(stderr, "\n==== [wcw] unhandled exception 0x%08lX at %p ====\n",
        (unsigned long)ep->ExceptionRecord->ExceptionCode, (void*)ep->ExceptionRecord->ExceptionAddress);
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        fprintf(stderr, "[wcw]   access %s address 0x%llX\n",
            ep->ExceptionRecord->ExceptionInformation[0] ? "WRITE" : "READ",
            (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
    }
    HANDLE proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(proc, nullptr, TRUE);
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf{};
    sf.AddrPC.Offset = ctx.Rip;    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 64; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, thread, &sf, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
        if (sf.AddrPC.Offset == 0) break;
        wcw_print_frame(proc, i, sf.AddrPC.Offset);
    }
    fprintf(stderr, "==== [wcw] end backtrace ====\n");
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

#ifdef _WIN32
#include <tlhelp32.h>
// DIAGNOSTIC: after a delay, suspend every other thread and print its stack, to find where
// the game threads are parked when the boot stalls (no crash, no progress).
static void wcw_sample_all_threads() {
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, nullptr, TRUE);
    DWORD me = GetCurrentThreadId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te{}; te.dwSize = sizeof(te);
    DWORD pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
            HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
            if (!th) continue;
            SuspendThread(th);
            CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(th, &ctx)) {
                STACKFRAME64 sf{};
                sf.AddrPC.Offset = ctx.Rip;    sf.AddrPC.Mode = AddrModeFlat;
                sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
                sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
                fprintf(stderr, "[wcw][sample] thread %lu:\n", (unsigned long)te.th32ThreadID);
                for (int i = 0; i < 16; i++) {
                    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th, &sf, &ctx, nullptr,
                                     SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
                    if (sf.AddrPC.Offset == 0) break;
                    wcw_print_frame(proc, i, sf.AddrPC.Offset);
                }
            }
            ResumeThread(th);
            CloseHandle(th);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
    fprintf(stderr, "[wcw][sample] done\n");
    fflush(stderr);
}
#endif

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);

static const recomp::Version version { 0, 1, 0, "" };

template <typename... Ts>
[[noreturn]] static void exit_error(const char* str, Ts... args) {
    fprintf(stderr, str, args...);
    assert(false);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

// ---------------- gfx / window ----------------
// These globals are referenced by recompui/recompinput, so they must be non-static.
SDL_Window* window = nullptr;
std::vector<recomp::GameEntry> supported_games;

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) > 0) {
        exit_error("Failed to initialize SDL2: %s\n", SDL_GetError());
    }
    fprintf(stdout, "SDL Video Driver: %s\n", SDL_GetCurrentVideoDriver());
    return {};
}

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    fprintf(stderr, "[wcw] create_window\n");
    uint32_t flags = SDL_WINDOW_RESIZABLE;
#if defined(RT64_SDL_WINDOW_VULKAN)
    flags |= SDL_WINDOW_VULKAN;
#endif
    window = SDL_CreateWindow("WCW vs. nWo World Tour: Recompiled",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 960, flags);
    if (window == nullptr) {
        exit_error("Failed to create window: %s\n", SDL_GetError());
    }
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
#if defined(_WIN32)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#else
    return ultramodern::renderer::WindowHandle{ window };
#endif
}

void update_gfx(void*) {
    recompinput::handle_events();
}

ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    if (controller_num == 0) {
        return { .connected_device = ultramodern::input::Device::Controller,
                 .connected_pak = ultramodern::input::Pak::RumblePak };
    }
    return { .connected_device = ultramodern::input::Device::None,
             .connected_pak = ultramodern::input::Pak::None };
}

// ---------------- audio (minimal: queue as-is, 16-bit stereo) ----------------
static SDL_AudioDeviceID g_audio_device = 0;
static uint32_t g_sample_rate = 32000;
constexpr uint32_t input_channels = 2;
constexpr uint32_t bytes_per_frame = input_channels * sizeof(int16_t);

static bool reset_audio(uint32_t freq) {
    SDL_AudioSpec want{}, have{};
    want.freq = (int)freq;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)input_channels;
    // 512-frame chunks (23ms @ 22050) — with 1024 (46ms) the device drained more per pull
    // than the game's ~25ms audio bursts could cover, guaranteeing periodic underruns.
    want.samples = 512;
    want.callback = nullptr;
    if (g_audio_device) { SDL_CloseAudioDevice(g_audio_device); g_audio_device = 0; }
    g_audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (g_audio_device == 0) { fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError()); return false; }
    g_sample_rate = (uint32_t)have.freq;
    SDL_PauseAudioDevice(g_audio_device, 0);
    return true;
}

void set_frequency(uint32_t freq) { reset_audio(freq); }

void queue_samples(int16_t* audio_data, size_t sample_count) {
    // audio_data is interleaved stereo; swap L/R to correct for endianness handling.
    static std::vector<int16_t> buf;
    buf.resize(sample_count);
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
        buf[i + 0] = audio_data[i + 1];
        buf[i + 1] = audio_data[i + 0];
    }
    uint32_t queued_frames_before = g_audio_device ? SDL_GetQueuedAudioSize(g_audio_device) / bytes_per_frame : 0;
    if (g_audio_device) SDL_QueueAudio(g_audio_device, buf.data(), (Uint32)(sample_count * sizeof(int16_t)));
    // Diagnostic (1/s): audio health. queued_frames_before==0 means the device ran dry and
    // SDL fed silence (an audible pop); minlvl is the low-water mark in frames; maxgap the
    // longest interval between batches (starvation if it exceeds the buffered duration).
    static uint64_t last_ms = 0, report_ms = 0; static uint64_t max_gap = 0;
    static uint32_t underruns = 0, batches = 0, min_lvl = UINT32_MAX; static int peak = 0;
    uint64_t now = SDL_GetTicks64();
    if (last_ms && now - last_ms > max_gap) max_gap = now - last_ms;
    last_ms = now;
    if (queued_frames_before == 0) underruns++;
    if (queued_frames_before < min_lvl) min_lvl = queued_frames_before;
    batches++;
    for (size_t i = 0; i < sample_count; i++) { int v = audio_data[i] < 0 ? -audio_data[i] : audio_data[i]; if (v > peak) peak = v; }
    if (now - report_ms >= 1000) {
        fprintf(stderr, "[audio] t=%llus batches=%u minlvl=%u underruns=%u maxgap=%llums peak=%d rate=%u\n",
            (unsigned long long)(now / 1000), batches, min_lvl, underruns, (unsigned long long)max_gap, peak, g_sample_rate);
        report_ms = now; underruns = 0; min_lvl = UINT32_MAX; max_gap = 0; peak = 0; batches = 0;
    }
}

size_t get_frames_remaining() {
    if (!g_audio_device) return 0;
    return SDL_GetQueuedAudioSize(g_audio_device) / bytes_per_frame;
}

// ---------------- RSP ucode (audio) ----------------
// The audio ucode (located at runtime: ucode=0x80029C50, ucode_data=0x80037530 — see
// rsp/README.md) is recompiled by RSPRecomp (rsp/wcw_audio.toml -> rsp/wcw_audio.cpp);
// it is byte-identical to BMHero's stock aspMain. Never return nullptr here: that makes
// recomp::rsp::run_task return false, which task_thread_func (events.cpp) treats as FATAL
// (ULTRAMODERN_QUICK_EXIT). For unexpected task types, hand back a no-op ucode that
// reports a clean RSP break so the task "completes" and the game keeps running.
static RspExitReason wcw_null_ucode(uint8_t* rdram, uint32_t ucode_addr) {
    (void)rdram; (void)ucode_addr;
    return RspExitReason::Broke; // pretend the task finished so the runtime continues
}

extern RspUcodeFunc wcw_audio_ucode;

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    static int n = 0;
    if (n++ < 4) {
        fprintf(stderr, "[rsp] task type=%u ucode=0x%08X ucode_data=0x%08X ucode_size=0x%X\n",
            (unsigned)task->t.type, (unsigned)task->t.ucode, (unsigned)task->t.ucode_data,
            (unsigned)task->t.ucode_size);
    }
    // Only non-gfx tasks reach here — gfx tasks go through the renderer path.
    if (task->t.type == M_AUDTASK) {
        return &wcw_audio_ucode;
    }
    fprintf(stderr, "[rsp] unknown task type=%u — running no-op ucode\n", (unsigned)task->t.type);
    return &wcw_null_ucode;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    fprintf(stderr, "[wcw] main start\n");
#ifdef _WIN32
    std::set_terminate(wcw_terminate_handler);
    SetUnhandledExceptionFilter(wcw_unhandled_filter);

    // [wcw] DIAGNOSTIC: when running under RenderDoc (renderdoc.dll injected), trigger a
    // one-frame capture ~8s in (title screen rendering steadily by then). The capture is the
    // ground truth for why game draws rasterize nothing (see disasm/libultra.md deep dive).
    {
        HMODULE rdoc = GetModuleHandleA("renderdoc.dll");
        if (rdoc != nullptr) {
            pRENDERDOC_GetAPI getApi = (pRENDERDOC_GetAPI)GetProcAddress(rdoc, "RENDERDOC_GetAPI");
            static RENDERDOC_API_1_1_2* rdocApi = nullptr;
            if (getApi != nullptr && getApi(eRENDERDOC_API_Version_1_1_2, (void**)&rdocApi) == 1) {
                int rdocDelay = 8;
                if (const char* d = getenv("WCW_RDC_T")) { int v = atoi(d); if (v > 0) rdocDelay = v; }
                fprintf(stderr, "[wcw][renderdoc] in-app API active; capture will trigger at t+%ds\n", rdocDelay);
                std::thread([rdocDelay]() {
                    std::this_thread::sleep_for(std::chrono::seconds(rdocDelay));
                    fprintf(stderr, "[wcw][renderdoc] triggering capture NOW\n");
                    // Multi-frame: game workloads land in ~27 of 60 present intervals, so a
                    // single-frame capture can contain only the VI blit. 12 frames guarantees
                    // several captures with real game draws.
                    rdocApi->TriggerMultiFrameCapture(12);
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    fprintf(stderr, "[wcw][renderdoc] captures so far: %u\n", rdocApi->GetNumCaptures());
                }).detach();
            }
        }
    }
#endif
#ifdef _WIN32
    // WORKAROUND: the first DXGI factory + D3D12 device creation must happen on the main
    // thread, or they fast-fail (0xC0000409) when RT64 first touches them on the game
    // thread. (BMHero avoids this because its launcher UI creates a render context on the
    // main thread first.) Prime DXGI + D3D12 here so RT64's later calls on the game thread
    // succeed.
    {
        IDXGIFactory4* tf = nullptr;
        fprintf(stderr, "[wcw] priming DXGI + D3D12 on main thread...\n");
        if (SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&tf))) && tf) {
            IDXGIAdapter1* ad = nullptr;
            for (UINT i = 0; tf->EnumAdapters1(i, &ad) != DXGI_ERROR_NOT_FOUND; i++) {
                ID3D12Device8* dev = nullptr;  // match RT64/plume: it requests ID3D12Device8
                bool ok = SUCCEEDED(D3D12CreateDevice(ad, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)));
                fprintf(stderr, "[wcw]   prime adapter %u D3D12CreateDevice(Device8) ok=%d\n", i, (int)ok);
                if (dev) dev->Release();
                ad->Release();
                if (ok) break;
            }
            tf->Release();
        }
        fprintf(stderr, "[wcw] priming done\n");
    }
#endif
#ifdef _WIN32
    timeBeginPeriod(1);
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif
    SDL_SetMainReady();
    NFD_Init();

    // Program identity (used for the config/save folder name and window/app metadata).
    recompui::programconfig::set_program_name("WCW vs. nWo World Tour: Recompiled");
    recompui::programconfig::set_program_id(u8"WCWNWOWorldTourRecompiled");

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    reset_audio(32000);

    // Register the game. The entry is stored in the global `supported_games` vector because
    // recompui's launcher reads supported_games[0] (e.g. default_launcher_init_callback) to
    // build the game-options menu; leaving it empty access-violates during boot.
    recomp::GameEntry game{};
    game.rom_hash = 0xACFCB4B5E49C6B0DULL;          // XXH3_64bits of the NTSC-U ROM
    game.internal_name = "WCW vs. nWo World Tour";
    game.display_name = "WCW vs. nWo World Tour";
    game.game_id = u8"wcw.nwo.worldtour.us";
    game.mod_game_id = "wcwnwoworldtour";
    // Sram = a 32 KB save buffer — the exact size of a Controller Pak, which is WCW's only
    // save medium (no cart EEPROM/SRAM). The raw-SI PIF emulation (librecomp si.cpp) maps
    // joybus pak reads/writes onto this buffer, so pak contents persist via librecomp's
    // standard save file (saves/<game id>.bin).
    game.save_type = recomp::SaveType::Sram;
    game.is_enabled = true;
    // Must be SIGN-EXTENDED: entrypoint_address is a gpr (int64) and librecomp's MEM_*
    // macros compute `rdram + (addr - 0xFFFFFFFF80000000)`. The bare literal 0x80000400 is
    // unsigned and zero-extends to 0x0000000080000400, which yields a bogus +4GB rdram offset
    // in the initial boot DMA. Cast through int32_t so it sign-extends to 0xFFFFFFFF80000400.
    game.entrypoint_address = (gpr)(int32_t)0x80000400;
    game.entrypoint = recomp_entrypoint;
    supported_games.push_back(game);
    recomp::register_game(supported_games[0]);

    recomp::register_config_path(std::filesystem::current_path());
    wcw::register_wcw_overlays();

    // Register UI fonts (loaded from ./assets during render-context init). Without a primary
    // font, recompui throws during boot. Matches BMHero's font registration.
    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");
    recompui::register_extra_font("NimbusSansNarrow-Bold.ttf");

    // Single-player input mode (matches BMHero): all connected controllers drive player 1.
    // Without this, recompinput defaults to multiplayer mode, where controller reads require
    // a pad explicitly assigned to the player via the player-assignment UI (which we never
    // run) — so gamepads are silently ignored while keyboard still works.
    recompinput::players::set_single_player_mode(true);

    // Create the standard config tabs (general/graphics/controls/sound) and load them from
    // disk. The input/render/audio paths read these configs during boot; without the tabs,
    // the gfx thread throws "General config has not been created yet" -> std::terminate.
    // (We skip the launcher UI but still need the config state. Mirrors BMHero init_config.)
    recompui::config::GeneralTabOptions general_options{};
    recompui::config::create_general_tab(general_options);
    recompui::config::create_graphics_tab();
    recompui::config::create_controls_tab();
    recompui::config::create_sound_tab();
    recompui::config::finalize();

    // Auto-load the ROM and request a boot (no launcher UI yet).
    std::u8string game_id = u8"wcw.nwo.worldtour.us";
    auto rom_err = recomp::select_rom("wcw.z64", game_id);
    if (rom_err != recomp::RomValidationError::Good) {
        exit_error("ROM validation failed (err=%d). Put a valid wcw.z64 in the working dir.\n", (int)rom_err);
    }

    // Callbacks.
    recomp::rsp::callbacks_t rsp_callbacks{ .get_rsp_microcode = get_rsp_microcode };
    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = [](uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode)
            -> std::unique_ptr<ultramodern::renderer::RendererContext> {
            fprintf(stderr, "[wcw] create_render_context (RT64)...\n");
            try {
                // Console presentation mode, NOT PresentEarly (which BMHero uses). PresentEarly
                // fires a present the moment a workload writes any previously-displayed
                // framebuffer — correct for one-gfx-task-per-frame games, but WCW builds each
                // frame from MULTIPLE tasks and the last task of a frame PRE-CLEARS the next
                // buffer (fbPair1). PresentEarly presented that freshly-cleared buffer -> one
                // pure black frame per game frame (verified by swapchain readback: a steady
                // [content, content, BLACK] present pattern = constant visible flicker).
                auto ctx = recompui::renderer::create_render_context(rdram, window_handle,
                    ultramodern::renderer::PresentationMode::Console, developer_mode);
                fprintf(stderr, "[wcw] create_render_context done (ctx=%p)\n", (void*)ctx.get());
                return ctx;
            } catch (const std::exception& e) {
                fprintf(stderr, "[wcw] render context EXCEPTION: %s\n", e.what());
                throw;
            } catch (...) {
                fprintf(stderr, "[wcw] render context UNKNOWN exception\n");
                throw;
            }
        },
    };
    ultramodern::gfx_callbacks_t gfx_callbacks{ .create_gfx = create_gfx, .create_window = create_window, .update_gfx = update_gfx };
    ultramodern::audio_callbacks_t audio_callbacks{ .queue_samples = queue_samples, .get_frames_remaining = get_frames_remaining, .set_frequency = set_frequency };
    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = recompinput::poll_inputs,
        .get_input = recompinput::profiles::get_n64_input,
        .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = get_connected_device_info,
    };
    ultramodern::events::callbacks_t events_callbacks{ .vi_callback = nullptr, .gfx_init_callback = nullptr };
    ultramodern::error_handling::callbacks_t error_callbacks{ .message_box = recompui::message_box };
    ultramodern::threads::callbacks_t threads_callbacks{ .get_game_thread_name = [](const OSThread*) -> std::string { return "game"; } };

    // Kick off the boot once the runtime is up.
    std::thread starter([game_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::u8string gid = game_id;
        recomp::start_game(gid, "");
    });
    starter.detach();

#ifdef _WIN32
    // DIAGNOSTIC watchdog (opt-in): set WCW_SAMPLE=<seconds> to dump all thread stacks that
    // many seconds in (WCW_SAMPLE=1 keeps the old ~4s default), to locate a stall.
    if (const char *wcwSampleEnv = getenv("WCW_SAMPLE")) {
        int wcwSampleDelay = atoi(wcwSampleEnv);
        if (wcwSampleDelay <= 1) wcwSampleDelay = 4;
        std::thread([wcwSampleDelay]() {
            std::this_thread::sleep_for(std::chrono::seconds(wcwSampleDelay));
            wcw_sample_all_threads();
        }).detach();
    }
#endif

    // NOTE: graphics-API init (both D3D12 and Vulkan) aborts in a headless/virtual-display
    // session even with a GPU present; it should initialize on a normal local desktop.
    // Leave api_option at its default (Auto -> D3D12 on Windows). To override for testing:
    //   auto gcfg = ultramodern::renderer::get_graphics_config();
    //   gcfg.api_option = ultramodern::renderer::GraphicsApi::Vulkan;  // or D3D12
    //   ultramodern::renderer::set_graphics_config(gcfg);

    fprintf(stderr, "[wcw] calling recomp::start...\n");
    recomp::start(version, {}, rsp_callbacks, renderer_callbacks, audio_callbacks,
        input_callbacks, gfx_callbacks, events_callbacks, error_callbacks, threads_callbacks);
    fprintf(stderr, "[wcw] recomp::start returned\n");

    NFD_Quit();
    return 0;
}
