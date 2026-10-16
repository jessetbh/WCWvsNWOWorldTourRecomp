# Linux builds: audit of Windows-only code (roadmap #15)

Audit only: no code changed. Read on 2026-10-14 from the six game `main` checkouts and the lib
forks as pinned in World Tour's main checkout (rt64 `d451db6`, N64ModernRuntime `84fd793`,
RecompFrontend `7c18624`). Our wcw changes were read as the diff against the upstream bases in
`lib-patches/export.ps1` (rt64 `f647df1`, plume `51b1ad4`, N64ModernRuntime `ca568b6`,
RecompFrontend `b3b7ebb`).

Effort: **S** = under a day, **M** = 1–3 days, **L** = a week or more.

## Summary

- **Most of the code already compiles on Linux.** Every Win32/D3D12/DXGI use in the games'
  `src/` and in our wcw lib changes sits behind `#ifdef _WIN32`. No MSVC-isms in
  `RecompiledFuncs/`, `RecompiledPatches/`, `rsp/` or `include/` (checked WT, Wm2k, NoMercy).
  The only unguarded D3D12 identifiers are in upstream code that is commented out
  (`rt64_framebuffer_renderer.cpp` im3d block).
- **Upstream already supports Linux** (Zelda64Recomp and BMHero ship Linux builds): ultramodern
  threads/timers, librecomp, recompui (Vulkan via SDL, NFD via GTK3, `~/.config/<id>` or
  `portable.txt` app folder), rt64's Vulkan backend, plume Vulkan, vendored `dxc-linux`.
  BMHero's `CMakeLists.txt` has the Linux block we dropped.
- **Someone has already done it once.** Gadge64's No Mercy fork shipped a Linux / Steam Deck
  build (0.2.1, 2026-10-11) from our forks. Their changes were almost all in the game repo:
  a Linux CMake block, one `SDL_syswm.h` guard in `main.cpp`, a build/package script
  (`tools/linux/build_linux.sh`, Debian bookworm chroot for an old glibc) and one RecompFrontend
  fix. Their libs were behind ours (rt64 99 commits behind wcw), so RT, HDR, the CRT
  filter and the venue lighting weren't in their Linux build.
- **What's actually missing:** the CMake Linux branch in all six games, `librashader.so`, a few
  features that are D3D12-only or Windows-only at runtime (ray tracing, HDR detection, beam
  pacing, crash backtraces, the log file, RetroAchievements HTTP), options to hide on Linux,
  CI and packaging.
- **The biggest risk isn't Windows code.** The family has only been played on D3D12. Our rt64
  changes (frame interpolation, present pacing, CRT on Vulkan, venue lighting, HDR compose)
  have run on Vulkan only in spot tests. Running all six games on **Windows with Graphics API =
  Vulkan** first separates Vulkan bugs from Linux bugs, and works with the current build.

## 1. Games (`src/`, the same in all six unless noted)

| # | What | Where | Linux | Effort |
|---|---|---|---|---|
| G1 | `#include "SDL_syswm.h"` unguarded. On X11 it pulls in Xlib, whose `None` macro breaks `ultramodern::input::Pak::None` (Gadge64 hit this). `SDL_GetWindowWMInfo` is only needed for the Win32 handle | `src/main/main.cpp:20-21`, `create_window()` | Move the include and the `SDL_SysWMinfo` lines inside `#if defined(_WIN32)` (Gadge64's fix), or `#undef None` after it as BMHero does | S |
| G2 | Log file for windowed launches: `CreateFileW`, `_open_osfhandle`, `_dup2`, `AllocConsole` (`--show-console`) | `wcw_setup_logging`, main.cpp (~384 WT) | Linux GUI launches (Steam, desktop file) lose stderr. Add a POSIX branch: `open()` the same `<app folder>/<Id>.log` and `dup2` over fds 1/2 when stderr isn't a tty. `--show-console` does nothing on Linux | S |
| G3 | Crash backtraces: `std::set_terminate`, `SIGABRT` handler and `SetUnhandledExceptionFilter`, using DbgHelp `StackWalk64` / `SymFromAddr` | main.cpp `wcw_terminate_handler` / `wcw_abort_handler` / `wcw_unhandled_filter` | `sigaction` for SIGSEGV/SIGBUS/SIGILL/SIGABRT + `backtrace()` / `backtrace_symbols_fd()` (glibc `execinfo.h`); build with `-g1` or keep a separate debug file. Needed for bug reports | S–M |
| G4 | Diagnostics: `WCW_SAMPLE` thread sampler (`CreateToolhelp32Snapshot`, `SuspendThread`); NoMercy/Vpw2/Wm2k `WCW2K_WRITEWATCH` (DR0 hardware watchpoint); RenderDoc in-app capture via `GetModuleHandleA("renderdoc.dll")` | main.cpp, all guarded | Leave Windows-only. RenderDoc has a Linux API (`dlopen("librenderdoc.so", RTLD_NOLOAD)`) if it's ever needed | none |
| G5 | Main-thread DXGI + D3D12 device priming (0xC0000409 workaround) | main.cpp | Not needed on Vulkan; already guarded | none |
| G6 | `timeBeginPeriod(1)` and `SDL_AUDIODRIVER=wasapi` | main.cpp | Not needed; Linux sleeps are already fine-grained and SDL picks PipeWire/Pulse | none |
| G7 | RetroAchievements HTTP over WinHTTP (NoMercy, Vpw2) | `src/ra/ra_client.cpp`; `winhttp.lib` in CMake | Already compiles to a stub on non-Windows (`#else` at the end of the file), so RA is off. Port the worker to libcurl later (rcheevos itself is portable) | M (later) |
| G8 | Window icon: `icons/app.rc` is Windows-only | CMakeLists | `SDL_SetWindowIcon` from the embedded `icon_bytes` already in the exe; a `.desktop` file + PNG for packaging | S |
| G9 | `game.save_type`, saves and config | via recompui | Upstream: `~/.config/<program id>/`, or the cwd if `portable.txt` is there. For the Deck, ship `portable.txt` in the package so saves stay in the game folder (Gadge64 did) | none |

## 2. Lib forks: our wcw changes

### rt64

| # | What | Where | Linux | Effort |
|---|---|---|---|---|
| R1 | **Ray-traced shadows + AO** (`WcwRaytracer`): inline `RayQuery` in `cs_6_5` shaders compiled to DXIL only (`build_shader_dxil` inside `if (WIN32)`); `initialize()` requires DXIL + DXR + DXR 1.1 state update, so on Vulkan it logs `UNSUPPORTED` and turns itself off | `CMakeLists.txt` (WcwRt*CS), `src/render/rt64_wcw_raytracer.cpp:28-33, 75-` | **Hide on Linux first** (R7). Porting later: compile the two CS to SPIR-V with `-fspv-extension=SPV_KHR_ray_query`; plume Vulkan already builds acceleration structures (`VK_KHR_acceleration_structure`) but doesn't enable `VK_KHR_ray_query` or report ray-query capability, so it needs that plus a Vulkan check in `initialize()`. Steam Deck (RDNA2) supports ray queries but would be slow | L (later) |
| R2 | Lighting editor (venue rigs) | `rt64_wcw_lighting_editor.cpp`, inside the RT64 inspector (ImGui) | Portable code; useless while RT is off. Upstream's Linux developer-mode input fix (rt64 `2ca7afe`) is already in our fork | none |
| R3 | **HDR detection**: `HdrOutput::queryDisplay` uses DXGI `IDXGIOutput6` + `DisplayConfigGetDeviceInfo` (SDR white level). On other platforms it returns "HDR off", so the game falls back to SDR on its own | `src/render/rt64_hdr_output.cpp:16, 70, 117` | **Hide the option on Linux first** (R7). The swap chain side already has Vulkan support: plume `VK_EXT_swapchain_colorspace` picks `EXTENDED_SRGB_LINEAR` / `HDR10_ST2084` (our plume change). A Linux port would detect HDR from the surface formats (gamescope on the Deck OLED offers them; desktop KDE/Wayland needs the Vulkan HDR layer), take paper white from a setting instead of Windows' SDR slider, and test on hardware | M–L (later) |
| R4 | **CRT filter** (librashader): D3D12 and Vulkan runtimes are both implemented (`LIBRA_RUNTIME_VULKAN`; the Vulkan render-pass and `imageFormat` fixes are in the devlog). Only `librashader.dll` is vendored. `programDirectory()` uses `GetModuleFileNameW` and falls back to the **cwd** elsewhere. `librashader_ld.h` uses `dlopen("librashader.so")`, which searches the RPATH/`LD_LIBRARY_PATH`, not the exe folder | `src/render/rt64_crt_filter.cpp:78, 131`; `src/contrib/librashader/` | Vendor `librashader.so` 0.12.0 (same ABI; prebuilt release or `cargo build`, MPL-2.0); `programDirectory()` from `/proc/self/exe`; set `BUILD_RPATH`/`INSTALL_RPATH` `$ORIGIN` (or dlopen an absolute path); copy `crt/` and the .so in the Linux post-build. Keep the filter on Linux: it already has a Vulkan path | S–M |
| R5 | **CRT beam simulator pacing**: `beamWaitForVBlank` uses DXGI `IDXGIOutput::WaitForVBlank` (or the timer fallback) inside `#if defined(_WIN32)`; elsewhere it returns 0, so beam presents aren't paced at all (the 2500 presents/s case in the devlog) | `src/hle/rt64_present_queue.cpp:862-` | Quick fix: use the existing timer path (`Timer::preciseSleepUntil`) off Windows and **hide Beam Simulator on Linux** until it's measured. Later: Vulkan FIFO / `VK_KHR_present_wait`, or `VK_EXT_display_control` vblank counters. Decision logic belongs in `WcwLogic::decideBeam` with a test | S (hide) / M (port) |
| R6 | `WCW_PRESENT_LUM` swap chain readback (screenshots + luminance log) crashes on Vulkan (`copyTextureRegion` on a swap chain image, devlog 2026-10-10) | `rt64_present_queue.cpp:630` | Blocks AkiTools evidence (`-Ring`, `-ShotSec`, `-Lum`) on Vulkan. Fix by copying the VI output render target instead of the swap chain image, or disable it on Vulkan with a log line. Needed before Vulkan/Linux QA can use aki-shot | M |
| R7 | Graphics menu offers Ray Tracing, RT quality, HDR and HDR Picture on every platform; the HDR description says "Windows HDR" | RecompFrontend `recompui/src/config/ui_config_tab_graphics.cpp` (options, ~549) | `#if !defined(_WIN32)`: hide (`update_option_hidden`) RT, RT quality, HDR, HDR Picture and Beam Simulator. Better still, hide RT whenever the API isn't D3D12 (Windows users on Vulkan get the same dead option) | S |
| R8 | Device-loss report + `WCW_DRED` (DRED breadcrumbs / page faults), Map-null guard | plume `plume_d3d12.cpp` (`wcwReportDeviceRemoved`) | D3D12-only by nature. Vulkan has no direct equivalent: log `VK_ERROR_DEVICE_LOST` with where it happened; `VK_EXT_device_fault` / `VK_NV_device_diagnostic_checkpoints` later if needed | S (log) |
| R9 | Inspector: our fix matches the SDL window by HWND | `rt64_application_window.cpp:59-` | Already falls back to the first SDL window off Windows. `Application::windowMessageFilter` (F1 etc.) is Win32 messages; upstream handles SDL events on Linux | none |
| R10 | Unit tests (`rt64_wcw_tests`, `recompui_wcw_tests`) run as a build step | `CMakeLists.txt` | Plain C++, no GPU; should build and run on Linux unchanged. Must pass in the Linux CI job too | none |

### plume (inside rt64)

| # | What | Linux | Effort |
|---|---|---|---|
| P1 | `RenderSwapChainColorSpace` + `getColorSpace()`: implemented for D3D12, Vulkan (`VK_EXT_swapchain_colorspace` added to `OptionalInstanceExtensions`, so drivers without it still work) and a Metal stub | Done | none |
| P2 | D3D12 device-loss reporting, `setSamplePositions` null fix | D3D12-only, see R8 | none |

### N64ModernRuntime

| # | What | Where | Linux | Effort |
|---|---|---|---|---|
| N1 | `[wcw fix]` turned on native thread priorities (`SetThreadPriority` was commented out upstream). On Linux `set_native_thread_priority` is still an upstream no-op, so the game threads run without the priorities Windows gives them | `ultramodern/src/threads.cpp:63-140` | Find out whether any family fix depends on host priority (the wcw2k starvation valve, audio, timing). If one does, implement it with `setpriority`/`nice` on the thread's TID (no root needed to lower; raising needs `CAP_SYS_NICE` or rtkit) or `SCHED_RR` via rtkit for audio. Otherwise leave it | S–M |
| N2 | `RtlCaptureStackBackTrace` diagnostic under `WCW_TRACE` | `ultramodern/src/mesgqueue.cpp` | Guarded; diag only | none |
| N3 | Everything else (si.cpp raw-SI/PIF, overlays, pi/sp, mesgqueue, events, timers): portable C++ | — | none | none |

### RecompFrontend

| # | What | Linux | Effort |
|---|---|---|---|
| F1 | Graphics menu: hide RT/HDR/Beam (R7) | see R7 | S |
| F2 | `InterfaceHdrPS` (UI composite into scRGB): SPIR-V + Metal blobs already wired | none | none |
| F3 | Framerate list: ours offers the game's smooth-rate options (30/60/120), not display rates, so Gadge64's Linux refresh-rate fix doesn't apply. On a 60 Hz Deck, 120 should be hidden or explained (applies to Windows on 60 Hz panels too) | S (polish) |
| F4 | Steam Deck detection: upstream `5a54745` ("Fix Steam Deck detection") is already in our fork | none | — |

## 3. Build, toolchain and CI

| # | What | Where | Linux | Effort |
|---|---|---|---|---|
| B1 | **No Linux branch in the games' CMakeLists.** SDL2 comes from FetchContent `SDL2-devel-VC.zip`, `DXC` is set only `if (WIN32)`, there's no `find_package(SDL2/Threads/X11/Freetype)`, no `PLUME_SDL_VULKAN_ENABLED`, no `-latomic` / `-ldl`. NoMercy/Vpw2 also link `winhttp.lib` | each repo's `CMakeLists.txt` | Port BMHero's Linux block (or Gadge64's smaller one): `find_package(SDL2)`, `Threads`, `${CMAKE_DL_LIBS}`, `add_compile_definitions(PLUME_SDL_VULKAN_ENABLED RT64_SDL_WINDOW_VULKAN)` **before** `add_subdirectory(rt64)` so recompui agrees on `plume::RenderWindow`; `DXC=LD_LIBRARY_PATH=… dxc-linux`; `$ORIGIN` rpath; copy `librashader.so` + `crt/`; `-static-libstdc++` as BMHero does. Same block in all six (they share the template) | S per game, M total |
| B2 | Windows-only link flags: `/SUBSYSTEM`, `/MAP`, `/STACK:16 MB` (D3D12 device creation stack overflow), `/OPT:NOICF` (mod function patching) | CMakeLists, already inside `if (WIN32)` / `if (MSVC)` | Linux equivalents: `-Wl,-Map=` for crash RVAs; the stack fix isn't needed on Vulkan (main thread 8 MB, std::thread 8 MB default); ICF is off by default in GNU ld/lld (don't add `--icf`) | S |
| B3 | Patches: `PATCHES_MAKE` finds `mingw32-make`/`make`; `zig cc` + `ld.lld`; `./N64Recomp patches.toml` runs a gitignored, Windows-built `N64Recomp.exe` | CMakeLists, `patches/Makefile` | `make`, `zig`/`clang` with the MIPS backend and `ld.lld` are standard on Linux. Build `N64Recomp` + `RSPRecomp` from source at the pinned commit (CI already does this on Windows) | S |
| B4 | Recompile and post-processing: `tools/recompile.ps1`, `env.ps1` (MinGW), `env-msvc.ps1`, `cycle.ps1` are PowerShell + Windows paths. The CI fixup scripts (`fix_stumps`, `fix_switches`, `fix_jraddend`, `fix_backbranches`, `fix_selfentry`, `readd_hand_edits`, `audit_coresidency`) are portable Python (`os.path`, explicit utf-8, `\r?\n`-tolerant) | `tools/` | CI on Linux can run the same Python steps. Check once that a Linux regen + fixup gives byte-identical `RecompiledFuncs/` to the Windows one (line endings: `readd_hand_edits` strips `\r\n`, but the emitted files will be LF). Local dev scripts can stay Windows-only | S |
| B5 | CI: `validate.yml` is Windows-only (the header comment says the Linux job was dropped from BMHero's template) | `.github/workflows/validate.yml` | Add `build-linux` on `ubuntu-22.04` (glibc 2.35, older than SteamOS 3.x's) or in a `debian:bookworm` container like Gadge64. Same ROM fetch, N64Recomp build, Python fixups, `cmake -G Ninja` with clang, package `.tar.gz`. Start non-blocking, make it required once green | M (first game), S each after |
| B6 | Release packaging: `release.yml` zips Windows bits (SDL2.dll, dxil, VC runtime) | `.github/workflows/release.yml`, `release-readme.txt` | A `-linux.tar.gz`: exe, `assets/`, `crt/`, `librashader.so`, `recompcontrollerdb.txt`, `portable.txt`, a launcher script (cd to its folder, `WCW_AUTOBOOT` if the ROM is next to it, SDL2 fallback via `LD_LIBRARY_PATH`), `README_STEAM_DECK.txt`. Flatpak (BMHero has a manifest) and AppImage later | M |
| B7 | `-march=nehalem` on x86_64 | CMakeLists | Fine on the Deck (Zen 2) and any x86-64-v2 CPU. aarch64 needs the sse2neon branch that's already there | none |

## 4. AkiTools / AkiMods / launcher

| # | What | Linux | Effort |
|---|---|---|---|
| T1 | `aki-run.ps1`: PowerShell, `Get-CimInstance Win32_OperatingSystem` (free RAM), `.exe` names in `games.psd1`, `send_input.ps1` / `size_window.ps1` (Win32 input/window), `graphics.json` under `%LOCALAPPDATA%`-style paths | Stays a Windows tool for now. For Linux QA: `pwsh` runs on Linux, but the RAM check, input and window sizing need `/proc/meminfo`, `xdotool`/`ydotool` branches. Until then, Linux runs use `WCW_QUIT_AFTER` and the game's own logs | M (later) |
| T2 | `aki-shot.py`: numpy + PIL only | Portable. Its input (`WCW_PRESENT_LUM` BMPs/CSV) is D3D12-only until R6 is fixed | none |
| T3 | AkiMods: `.nrm` mods are platform-independent (MIPS code + manifest), built once with `zig cc` + RecompModTool; the games install them from `assets/mods/` | The same .nrm files work on Linux. `build.ps1`/`stage.ps1` stay Windows dev tools | none |
| T4 | AKI Launcher (Win32/D3D11/ImGui) | Out of scope (#15 says games only). Linux players start each game directly or from Steam | — |

## 5. Proposed order

1. **Vulkan on Windows first (no Linux needed).** Run each of the six games with Graphics API =
   Vulkan, RT off: boot, a match, menus, 60 fps interpolation, CRT filter on/off, a settings
   change, fullscreen toggle. Fix R6 (Vulkan present readback) first so aki-run / aki-shot work
   on Vulkan. Anything that breaks here is a Vulkan bug in our rt64 changes, not a Linux one.
2. **Shared lib PRs** (one rt64 + one RecompFrontend PR, base `wcw`):
   R7/F1 hide RT, RT quality, HDR, HDR Picture and Beam on non-Windows (and RT when the API isn't
   D3D12); R4 `programDirectory()` via `/proc/self/exe`; R5 timer pacing off Windows; R8 device-lost
   log. Unit tests for any `WcwLogic` change. Then N1 decide on thread priority.
3. **World Tour on Linux, RT off** (WT is the reference):
   G1 `SDL_syswm.h` guard, G2 log file, G3 signal backtraces, G8 icon, B1 CMake Linux block,
   B2 map file, B3 N64Recomp/RSPRecomp from source, `librashader.so` vendored (R4). Build in a
   bookworm chroot or container (Gadge64's `wsl_bookworm_build.sh` shows how from Windows),
   boot on a desktop Linux box, then a Steam Deck (Game Mode via "Add a Non-Steam Game").
4. **CI for WT** (B5): non-blocking `build-linux` job, then the release tarball (B6).
5. **Roll out to Revenge, VPW64, WM2000, No Mercy, VPW2**: the same CMake block and main.cpp edits
   (the six mains share the template); NoMercy/Vpw2 keep RetroAchievements off on Linux (G7).
   Pin bumps in every superproject after the lib PRs merge.
6. **Later, separately ticketed:** RA over libcurl (G7), HDR on Linux via the Vulkan surface
   color spaces / gamescope (R3), beam simulator vblank pacing on Vulkan (R5), ray-query RT on
   Vulkan (R1), Flatpak, AkiTools on Linux (T1). Deck-specific polish from the ticket
   (default Z binding off the click stick, Nintendo-layout pads, 16:10) is game-side and
   platform-neutral: it fits in step 3 or its own ticket.

Rough total for steps 1–5: L (about two weeks of agent time, mostly testing), assuming no deep
Vulkan bugs turn up in step 1.
