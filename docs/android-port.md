# Android builds: plan (roadmap #16)

Planning only: no code changed, nothing built or run. Written on 2026-10-14 from the
Linux audit (`docs/linux-audit.md`, PR #8, roadmap #15), the lib forks as pinned in World
Tour's main checkout (rt64 `d451db6`, N64ModernRuntime `84fd793`, RecompFrontend `7c18624`)
and the four public community Android ports of N64: Recompiled games. Audit item numbers
(G1, R4, B1, …) refer to `linux-audit.md`.

Effort: **S** = under a day, **M** = 1–3 days, **L** = a week or more.

## Summary

- **It has been done four times, all on the same stack we use.** Zelda64Recomp, Goemon 64,
  Harvest Moon 64 and DK64 all have working Android ports: SDL2 + RT64's Vulkan backend +
  N64ModernRuntime + RecompFrontend, built with the NDK for `arm64-v8a`, shipped as a
  sideloaded APK that asks for the ROM on first launch. Nobody upstreamed it, so there's no
  Android target in rt64/plume/N64ModernRuntime/RecompFrontend; each port carries its own
  patches. Their notes tell us exactly what breaks.
- **Android is Linux plus four extra problems.** Everything the Linux plan needs (CMake
  non-Windows branch, `SDL_syswm.h` guard, Vulkan-only paths, hidden RT/HDR/Beam options,
  Vulkan screenshots) is needed here too. On top: the NDK cross-build (host tools run on the
  build machine), mobile Vulkan drivers (descriptor limits, swapchain quirks, Mali without
  dual-source blending, Qualcomm driver crashes), the Android app lifecycle (the surface
  goes away when the app is backgrounded, and the process can be killed without warning),
  and scoped storage (ROM import and saves through the system file picker).
- **Two bugs in our tree affect us on ARM64 specifically**, and they'd hit Linux arm64 too:
  the mod function patcher doesn't flush the instruction cache and only unprotects one page
  (`librecomp/src/mods.cpp:233-247, 580`). Every AKI game bundles AkiMods hooks, so this has
  to be fixed before any ARM build ships.
- **Six APKs, one per game, controller-first.** Each game is its own app
  (`… Recompiled`), arm64 only, Android 9+, GitHub Releases. Touch controls come later and
  only if the owner wants them: wrestling games use every button and combos, so a touch
  layout is a design job, not a port job.
- **First milestone:** World Tour boots on a Snapdragon handheld or phone with a Bluetooth
  pad, imports the ROM through the picker, plays an exhibition match to the end, saves, and
  survives background/resume. About 1–2 weeks of agent time after the Linux prerequisites.

## 1. Prior art

| Port | State (Oct 2026) | What's worth copying |
|---|---|---|
| [linkzenic/Zelda64Recomp-Android](https://github.com/linkzenic/Zelda64Recomp-Android) | First one; beta, Adreno handhelds (AYN/Retroid), Android 7+ | The original rt64/plume Android patches the others started from. Uses a public `/Zelda64` folder, which needs broad storage access: **don't copy that part** |
| [ogdanimal/Goemon64Recomp-Android](https://github.com/ogdanimal/Goemon64Recomp-Android) | 1.0.8, active (2026-10-12), Android 9+ | The most finished: SAF ROM import with hash check, validated save import with `.pre-import.bak`, Mali dual-source-blending fallback, Adreno driver-crash workaround, libadrenotools custom driver with self-test, touch overlay that hides when a pad is used, release workflow on `v*` tags. Forks of rt64/N64ModernRuntime on a `goemon-android` branch (same pattern as our `wcw` branches). NDK 27.1.12297006, CMake 3.22.1, minSdk 28 |
| [igawa6/HarvestMoon64Recomp](https://github.com/igawa6/HarvestMoon64Recomp) | Android 9+, Vulkan 1.1+ | `docs/PORT_NOTES.md` is the best write-up: the full list of rt64/plume/N64ModernRuntime/RecompFrontend changes, the surface-ready handshake, the descriptor workaround, nine upstreamable fixes. Keeps its changes as patch files over pinned upstream commits; they call that the most fragile part. **We use our forks instead** |
| [deivid22srk/dk64-recomp-android](https://github.com/deivid22srk/dk64-recomp-android) | CI-built debug APK, Android 8+ | CI that pulls the build ROM from a private repo through a secret (same idea as our `wcw-rom-secret`); notes a Qualcomm crash in `vkGetRefreshCycleDurationGOOGLE` on the RT64 present thread |

All four are GPL-3.0, like our six repos, so we can reuse their code with credit.

## 2. Prerequisites

### 2a. From the Linux work (#15)

Android needs these audit items first; doing them for Linux means they're already tested
on a desktop GPU before they meet a phone driver.

| Audit item | Why Android needs it |
|---|---|
| Step 1: **Vulkan on Windows** pass of all six games | Every Android bug is otherwise a mix of "our rt64 changes on Vulkan" and "mobile driver". Do this first |
| R6 `WCW_PRESENT_LUM` readback on Vulkan | Without it there are no AkiTools screenshots or luminance logs on Vulkan; on Android we'd only have `adb exec-out screencap` |
| R7/F1 hide RT, RT quality, HDR, HDR Picture, Beam off Windows | Same options to hide on Android (section 4). Build it as one capability check, not scattered `#ifdef`s |
| G1 `SDL_syswm.h` guard | Same compile problem; on Android `SDL_GetWindowWMInfo` isn't needed either |
| G2/G3 log file and crash backtraces | On Android: stdout/stderr to logcat (`__android_log_write` or a pipe thread), plus the same log file in the app folder. Backtraces via `_Unwind_Backtrace` (no `execinfo.h` in bionic) |
| B1 CMake non-Windows branch | The Android branch is a variant of it (2b, L3) |
| B3 N64Recomp/RSPRecomp built from source | Codegen runs on the host for Android builds too |
| R4 librashader paths | Only if the CRT filter ever comes to Android (section 4: not in v1) |
| N1 thread priorities | Same question; on Android `setpriority` on the TID works without root |

### 2b. Android-only lib changes

Checked against our tree where noted. Every change goes on the forks' `wcw` branches behind
`__ANDROID__`, so Windows builds don't change; the six-game rebuild check still applies.

**Gotcha for all of these:** the NDK defines `__linux__` as well as `__ANDROID__`. Every
`#elif defined(__linux__)` in our code and upstream's also runs on Android, so Android
branches must come **before** the Linux ones.

| # | What | Where | Fix | Effort |
|---|---|---|---|---|
| L1 | **`RenderWindow` type mismatch.** plume picks `ANativeWindow*` on Android before it checks `PLUME_SDL_VULKAN_ENABLED`, but ultramodern's `WindowHandle` on Android is `SDL_Window*` and `rt64_render_context.cpp:229` passes it straight through. Won't compile | plume `plume_render_interface_types.h:20, 40` (checked) | Test `PLUME_SDL_VULKAN_ENABLED` first, as HM64 does; surface creation (`plume_vulkan.cpp:2100`) already prefers `SDL_Vulkan_CreateSurface` | S |
| L2 | `static_assert(false && "Android unimplemented")` in rt64's app window, compiled into the rt64 lib | `rt64/src/hle/rt64_application_window.cpp:130, 174` (checked) | SDL video init + `SDL_GetCurrentDisplayMode`, fullscreen (HM64's fix) | S |
| L3 | Cross-build CMake: DXC/SPIRV-Cross and `file_to_c` must be the **host** binaries (`CMAKE_HOST_SYSTEM_*`, a `RT64_HOST_FILE_TO_C` cache var); `Linux` → `Linux\|Android` for `PLUME_SDL_VULKAN_ENABLED`/`RT64_SDL_WINDOW_VULKAN` and SDL include dirs (rt64, recompinput, recompui); nfd has no Android backend (stub); `set(DXC "LD_LIBRARY_PATH=…")` passes the assignment as argv[0] (use `cmake -E env`) | rt64, RecompFrontend CMakeLists | As in HM64 PORT_NOTES §2 | M |
| L4 | **Mobile descriptor limits.** rt64's texture cache uses 8192-entry boundless descriptor arrays; mobile drivers don't allow that. HM64 uses 1024 on Android (`RT64_TEXTURE_CACHE_SIZE`, shared by the shaders and C++) and makes the set `UPDATE_AFTER_BIND \| PARTIALLY_BOUND` when the device supports it | rt64 `rt64_descriptor_sets.h`, `FbRendererCommon.hlsli`; plume descriptor set layout/pool | Port HM64's version, **add a log line** saying which path was taken (theirs is silent). A venue with many textures may hit 1024: test No Mercy's busiest venue | M |
| L5 | **Android swapchain and surface lifecycle.** The surface is destroyed when the app goes to the background. Without a handshake, the render thread uses a dead `ANativeWindow` (driver crash) or never builds a swapchain (black screen, no error). Plus: `maxImageCount == 0` means unlimited, surface-format fallbacks, `currentExtent` is authoritative, `preTransform`, image usage masked to what's supported | plume `plume_vulkan.cpp` swapchain; SDL Java `SDLSurface.java` | HM64's surface-ready flag + 500 ms bounded drain, surface recreation by generation counter. Our present queue and interpolation threads must stop presenting while the flag is down | M–L |
| L6 | **Mod patching on ARM64.** `patch_func` writes `ldr x2, #8; br x2` but never calls `__builtin___clear_cache`, so the CPU can keep running the old instructions; `unprotect`/`protect` change exactly one page, so a 16-byte patch that crosses a page boundary faults | N64ModernRuntime `librecomp/src/mods.cpp:233-247, 580-588` (checked: both present in our fork) | HM64's fix: round the range up to every page touched; `__builtin___clear_cache` after patch/unpatch on ARM64. Needed by every game (bundled AkiMods hooks). Linux arm64 and macOS too, so it can go upstream | S |
| L7 | App and program paths: bionic reports a read-only HOME; rt64's user paths and recompui's `get_program_path` / `get_app_folder_path` need an `__ANDROID__` branch (before `__linux__`) that reads the folder the Java side passes in | rt64 `rt64_user_paths.cpp`; RecompFrontend `recompui/src/util/file.cpp` | Java sets `APP_FOLDER_PATH` / `APP_PROGRAM_PATH` before `SDL_main` | S |
| L8 | ROM picker: recompui opens the ROM through nfd. On Android it needs the Storage Access Framework (`ACTION_OPEN_DOCUMENT`) via JNI, with the result delivered back on the UI thread that owns RmlUi | RecompFrontend `file.cpp`, `ui_state.cpp` draw hook | HM64's async SAF dialog | M |
| L9 | Vulkan device quirks: accept Vulkan 1.2/1.3 core in place of the descriptor-indexing / scalar-block-layout / buffer-device-address extension strings; `VK_EXT_debug_utils` optional (with the null-function guard); graphics queues count as transfer-capable; null-pipeline tolerance; device-diagnostics file; on non-Adreno turn off sample locations, present-wait and display timing | plume `plume_vulkan.cpp` | Port from HM64/Goemon | M |
| L10 | **Mali: no dual-source blending.** White screen, missing textures. Goemon 1.0.3 picks a fallback from the device caps; coverage effects are approximated | rt64 shaders/pipeline | Port Goemon's fallback | M |
| L11 | Qualcomm driver crashes: `vkGetRefreshCycleDurationGOOGLE` on the present thread (DK64, moto g34), a framebuffer-effects call a few seconds in (Goemon, Adreno 630) | plume refresh-rate query (`plume_vulkan.cpp:~2480`); rt64 | Don't call display-timing on Android unless it's been tested on the device class; keep a Framebuffer Effects setting reachable from the menu before the game renders | S |
| L12 | Header and libc++ breakage: json 3.9.1 doesn't compile with NDK r29's libc++ (`char_traits<unsigned char>` removed). NDK 25 lacks `<cmath>` transitively for rt64. `aligned_alloc` needs API 28 | rt64 + N64ModernRuntime `json.hpp` | **Pin NDK 27.1.12297006** (what Goemon/HM64 use). Bump json to 3.11.3 when moving to r29 | S |
| L13 | Main-loop sleep: librecomp's 1 ms loop wakeup costs battery and buys nothing on Android (input comes through SDL's queue) | `librecomp/src/recomp.cpp` | HM64 uses 16 ms on Android. Check it doesn't touch our pacing first | S |
| L14 | **Thread stacks.** Game threads are `std::thread` (`ultramodern/src/threads.cpp:610`). bionic's default stack is about 1 MB; glibc's is 8 MB and our Windows link sets 16 MB for the main thread. AKI call chains through recompiled C have never run on a 1 MB stack | ultramodern threads | Measure first (guard-page crash = stack). If needed, create game threads with `pthread_attr_setstacksize(8 MB)` on Android | S |

Where to start on L3–L11: Goemon's rt64 and N64ModernRuntime forks (`goemon-android`
branch) carry them as commits on forks of the same upstream bases. Our `wcw` branches
have moved further from upstream (interpolation, pacing, CRT, venue lighting, HDR), so
port them by hand: read their diff, don't merge it.

## 3. Build setup

**Host:** Linux, in WSL2 Ubuntu locally or `ubuntu-22.04` in CI. The NDK runs on Windows,
but rt64's shader tools (dxc-linux, spirv-cross) and our Linux CI make Linux the simpler
host. Codegen (N64Recomp, RSPRecomp, the Python fixups, the `zig cc` patches build) runs on
the host exactly as for the Linux build.

**Toolchain pins:** Android Gradle Plugin + Gradle wrapper, **NDK 27.1.12297006**, CMake
3.22.1+ (from the SDK), JDK 17, `compileSdk`/`targetSdk` 34 (35 if we ever go near the Play
Store), `minSdk 28`, `abiFilters 'arm64-v8a'`, `ANDROID_STL=c++_shared`, `-std=c++20`.

**Layout in each game repo:**

```
android/
  settings.gradle, build.gradle, gradle/ gradlew   Gradle wrapper
  app/build.gradle        applicationId, names, versionCode from the tag, CMake args
  app/src/main/AndroidManifest.xml
  app/src/main/res/       adaptive icon from icons/
CMakeLists.txt            gains an `if (ANDROID)` branch: SDL2 from source, the game as a
                          shared library (libmain.so), host file_to_c, no nfd
```

The Java side (vendored SDL 2.30.x Java layer with the surface-ready calls, the launcher
activity, ROM import, save import/export, asset installer, later touch and the custom
driver loader) is the same for all six games. **Put it in one shared repo**
(`jessetbh/AkiAndroid`, a submodule at `android/common`) rather than six copies. Per-game
`android/app` is then only manifest, IDs, strings and icons. Keep SDL's Java files on the
same SDL version as the native SDL, and mark every local edit with a `[wcw android]`
comment: HM64's worst trap is unmarked edits in vendored SDL Java that vanish on an SDL bump.

**Native deps:** SDL2 2.30.3 (the version the Windows build uses) from source via
`add_subdirectory`; FreeType for RmlUi built for arm64 (HM64's `setup_android_deps.sh`);
no librashader, no DXC at runtime (rt64 embeds SPIR-V built on the host); libadrenotools
later (section 8).

**Recompiled C on ARM64 (ticket question):**
- The generated C is architecture-independent. The same `RecompiledFuncs/` (17 MB WT to
  38 MB No Mercy), with the same fixups and hand-edits, compiles for arm64. No x86
  intrinsics or inline asm in the games' `src/` (checked all six) or WT's `include/`;
  librecomp's SSE paths go through the vendored `sse2neon`. `-march=nehalem` is x86-only:
  leave it out on Android.
- Size: the Windows exes are 14 MB (WT) to 16 MB (No Mercy). Expect `libmain.so` around
  15–25 MB stripped, APK 15–30 MB with assets. Keep an unstripped copy per release for
  crash symbols.
- Compile time: the same order as the Linux clang build. Use `-j 2`–`4` on a 16 GB
  machine: the big recompiled files take a lot of memory each.

**CI:** an `android.yml` per game on `ubuntu-22.04`: fetch the ROM from `wcw-rom-secret`
the same way `validate.yml` does (it never enters the APK), build N64Recomp/RSPRecomp, run
codegen + fixups, `./gradlew assembleDebug`, upload the APK as an artifact. Non-blocking at
first. Releases: `assembleRelease` on `v*` tags, signed with a keystore from a GitHub
secret.

**Signing:** one release keystore for the whole family, backed up off-machine. Android
won't install an update signed with a different key, so losing it strands every user.

## 4. What to hide or change on Android

Do this as one platform-capability check in RecompFrontend's graphics tab (shared with the
Linux R7 work): a small `PlatformCaps` (has RT, has HDR, has CRT, has Beam, smooth-rate
choices) decided in `framerate_rules`-style logic with unit tests in `recompui_wcw_tests`.

| Feature | Android v1 | Why |
|---|---|---|
| Ray-traced shadows + AO, RT quality, venue lighting editor | **Hidden** | D3D12 + DXR only (audit R1). Ray query on mobile is possible on new Adreno/Mali but far too slow for this |
| HDR, HDR Picture | **Hidden** | DXGI-only detection (R3). Phone HDR output is a separate project |
| CRT filter (librashader) | **Hidden** | No Android build of librashader vendored, and the presets are heavy for phone GPUs. Could come later through librashader's Vulkan runtime |
| Beam Simulator | **Hidden** | No vblank source off Windows (R5); phone panels are variable-rate |
| 120 fps | **Hidden** | Offer the original rate and 60. Default: original rate (battery, heat), 60 opt-in. Revisit 90/120 on 120 Hz handhelds after measuring |
| Resolution | Default 2x internal, not "match window" | Phone panels are 1080p–1440p; window-matching would render at 4–6x |
| MSAA | Default off | Bandwidth on tile-based GPUs |
| Framebuffer Effects | Kept, but reachable before the game renders | Workaround for the Adreno driver crash (L11) |
| Fullscreen toggle, window size, `--show-console`, RenderDoc, `WCW_SAMPLE`, write-watch, developer inspector | Hidden / not built | Desktop- or Windows-only |
| RetroAchievements (No Mercy, VPW2) | Off (the existing non-Windows stub) | WinHTTP worker (G7); revisit with the libcurl port |
| Return-to-launcher gesture, `WCW_AUTOBOOT` | Not used | No launcher on Android (#16 is the games only). Each APK boots its own game |
| Mods menu + bundled AkiMods | **Kept** | `.nrm` files are platform-independent. Needs L6 first |

## 5. ROMs, saves and config under scoped storage

- **No storage permissions.** No `MANAGE_EXTERNAL_STORAGE`, no public folder (Zelda's
  `/Zelda64` needs broad access and is the part not to copy).
- **App folder** = `getExternalFilesDir(null)` (`Android/data/<package>/files/`), falling
  back to `getFilesDir()`. Java passes it to native code as `APP_FOLDER_PATH` before
  `SDL_main` (L7). Inside it, the same layout as the desktop app folder (`saves/`,
  `config/`, `mods/`, the imported ROM), so saves and configs copy between PC and phone
  unchanged.
- **ROM import:** first launch opens the system picker (`ACTION_OPEN_DOCUMENT`, L8). The
  file is copied into the app folder, byte-swapped if needed (`.v64`/`.n64`) and checked
  against the game's SHA1 (the launcher already has all six). Nothing is copied when the
  hash is wrong, and the message names the expected ROM (the launcher's per-game ROM notes
  already have the wording).
- **Saves:** all six are file-backed (SRAM for WT, Revenge, WM2000, VPW64, VPW2; FlashRAM
  for No Mercy; Controller Pak where a game uses it), so nothing changes in the formats.
  What changes: **Android can kill a backgrounded app without closing it.** The family rule
  "graceful shutdown = close the window" doesn't hold. Save writes must already be on disk
  when the app goes to the background: check librecomp's save thread flushes promptly and
  force a flush on `SDL_APP_WILLENTERBACKGROUND`. Part of milestone 1.
- **Save import/export** in the settings menu through the picker (Goemon's design: check
  the size and that it's this game's save, keep the old one as `.pre-import.bak`, only
  before the game starts). `Android/data` isn't reachable from most file managers on
  Android 11+, so this is how players move saves. Later: a `DocumentsProvider` so the
  Files app can see the folder.
- **Assets:** `assets/` (RmlUi UI, fonts, bundled `.nrm` mods) ships inside the APK and is
  extracted to the app folder on first run and whenever the version changes (Goemon's
  `AssetInstaller`). `install_bundled_mods` then works unchanged.
- **Uninstall deletes the app folder**, saves included. Say so in the README and on the
  import screen.

## 6. Input

- **Milestone 1 is controller-only:** handheld built-in controls, Bluetooth and USB pads
  through SDL's GameController API. Ship our `recompcontrollerdb.txt` plus SDL's database;
  Android sometimes hands newer pads over as generic HID with dead buttons (Goemon added a
  DualSense Edge entry for Android ≤12).
- **Multiple pads matter more for us than for the other ports:** these are 4-player
  wrestling games. Test two and four pads, and reconnecting a pad mid-match. WM2000's
  multi-controller crash is a known issue: check it on Android before calling WM2000 done.
- **Menus:** SDL maps touch to mouse, so recompui menus work by touch. Trap the Back
  button (`SDL_HINT_ANDROID_TRAP_BACK_BUTTON`) so it opens the menu instead of quitting.
- **Touch controls: later, and the owner decides.** Grapples, strikes, run, C-button
  taunts and target switching, plus combos, need a real layout per game. If we do it, use
  Goemon's approach: a Java overlay feeding a virtual SDL controller, so it goes through the
  normal bindings, hidden as soon as a physical pad is used. Not in milestone 1.

## 7. Performance targets

**Device tiers:**
1. Snapdragon 8 Gen 2 / Gen 3 handhelds and phones (AYN Odin 2 / Thor, Retroid Pocket 5).
   What every community port tested on. **Our test device should be one of these.**
2. Adreno 6xx mid-range (SD 865, Retroid Pocket 4 Pro / Mini, older flagships).
3. Mali (Dimensity, Exynos, Tensor). Needs L10. Best effort.

**Targets (to confirm by measuring, not assumed):**

| | Tier 1 | Tier 2 |
|---|---|---|
| Original game rate, 2x internal, 4-man match in the busiest venue | locked, no drops | locked at 1x–2x |
| 60 fps interpolation | steady, framebuffer effects on | optional, may drop |
| 30-minute session | no thermal throttling below target | original rate holds |
| Background/resume, 10 times | no crash, audio back | same |

The recompiled game code is light on CPU; the cost is rt64's GPU work. Android swapchains
are effectively FIFO-only and panels run at 60/90/120 Hz, so our present pacing
(`WcwLogic` even-present targets, built and tested on D3D12) needs checking on FIFO; any
rule change goes in `rt64_wcw_logic` with a test.

**How to measure:** presents/s from the game log (logcat), `adb shell dumpsys gfxinfo` /
Android GPU Inspector for GPU time, `adb exec-out screencap -p` for screenshots until R6
works. AkiTools stays Windows-only (audit T1); write the numbers into the ticket in the same
fields `run.json` uses (presents/s, settings, git heads).

## 8. Packaging and distribution

- **Six APKs**, one per game, matching the six repos. One combined app would be six
  recompiled images in one process and six times the download.
- **IDs and names:** application IDs like `io.github.jessetbh.wcwrecompiled` (one per
  game, never changed after the first release, since a new ID is a new app). Display names
  are the locked "… Recompiled" names. Adaptive icons from each repo's `icons/`.
- **Channel:** GitHub Releases, `-android-arm64.apk` next to the Windows zip, versionCode
  derived from the tag. Works with Obtainium. No Play Store (ROM-required apps, targetSdk
  churn) and no F-Droid (the build needs the ROM).
- **Release README section:** requirements (Android 9+, arm64, Vulkan 1.1+), which ROM,
  where saves live, uninstall warning, the Framebuffer Effects workaround, the driver
  option.
- **Custom GPU driver (later):** all four ports ended up adding libadrenotools so Adreno
  users can load a Turnip driver when the Qualcomm one crashes. Plan it for milestone 3, with
  Goemon's self-test and fall back to the system driver.
- **Launcher:** not ported (out of scope for #16).

## 9. Milestones

**M0, prerequisites** (from #15): Vulkan-on-Windows pass of World Tour; R6 Vulkan
readback; R7 hide options; G1; B1/B3. Then the Android lib work L1–L3, L6, L7, L12, L14 on
the forks' `wcw` branches (PRs on the forks, as for any lib change).

**M1, World Tour boots on a test device with a controller.** World Tour first: it's the
family reference, the smallest codebase (17 MB of recompiled C) and SRAM-only.

Work: `android/` Gradle module and the shared `AkiAndroid` Java layer (vendored SDL Java +
surface-ready calls, launcher activity, ROM import with SHA1 check, asset installer);
WT `CMakeLists.txt` Android branch; L4, L5, L8, L9, L11, L13; logcat logging; save flush on
background. Debug APK built in WSL; CI comes after.

Done when, on a tier-1 device with a Bluetooth pad:
1. The debug APK installs and opens to the recompui start screen.
2. ROM import through the picker accepts the right ROM and refuses a wrong one.
3. Title → exhibition 1-on-1 → match to the finish, with audio, at the original rate.
4. A save survives force-stopping and reopening the app.
5. Ten background/resume cycles mid-match: no crash, no black screen, audio comes back.
6. The bundled camera mod loads and its hook works (proves L6).
7. Log file and logcat output attached to the ticket, with presents/s and the device/driver.

Estimate: 1–2 weeks of agent time after M0, mostly L4/L5 and on-device testing. Needs a
physical device: the emulator doesn't run rt64's Vulkan path well enough to prove anything.

**M2, hardening:** the Mali fallback (L10) and a Mali device; 2–4 pad testing; the
performance table across tiers; settings defaults from section 4; 60 fps on FIFO.

**M3, the other five games:** with the shared layer, each is an `android/app` folder, the
CMake branch and testing. Revenge and VPW64 next (closest to WT), then WM2000, No Mercy
(FlashRAM, biggest code, busiest venues for L4) and VPW2. Custom driver option, release
workflow and signing.

**M4, decisions and polish:** touch controls (yes/no, per-game layouts), save
`DocumentsProvider`, 90/120 Hz, any upstream PRs (L6 and the plume fixes are generic).

## 10. Decisions for the owner

1. **Test device:** which tier-1 device to test on (M1 can't finish without one).
2. **Touch controls:** controller-only (recommended for v1) or a touch layout later.
3. **Shared Android layer:** a new `jessetbh/AkiAndroid` repo (recommended) or a folder in
   RecompFrontend.
4. **Application IDs:** confirm the `io.github.jessetbh.<name>recompiled` pattern before
   the first public APK; they can't change afterwards.
5. **Distribution:** GitHub Releases only (recommended), or also Obtainium/other listings.
