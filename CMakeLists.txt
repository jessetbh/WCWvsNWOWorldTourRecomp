# CMakeLists.txt for WCW vs. nWo World Tour: Recompiled
# The real Stage-B build: recompiled game + ultramodern + librecomp + RecompFrontend + RT64.
# Adapted from BMHero. Configure with clang-cl (see tools/env-msvc.ps1):
#   cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-msvc --target WCWRecompiled
cmake_minimum_required(VERSION 3.20)
project(WCWRecompiled LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
    add_compile_definitions(NOMINMAX)
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(NMR ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)
set(RT64DIR ${CMAKE_SOURCE_DIR}/lib/rt64)

# --- RT64 (renderer) ---
set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(HLSL_CPU)
add_subdirectory(${RT64DIR} ${CMAKE_BINARY_DIR}/rt64)
target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# --- Runtime core (ultramodern + librecomp) ---
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${NMR})

set(RECOMP_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${NMR}/ultramodern/include
    ${NMR}/librecomp/include
    ${NMR}/N64Recomp/include
)

# --- RecompiledFuncs: the recompiled game ---
add_library(RecompiledFuncs STATIC)
target_compile_options(RecompiledFuncs PRIVATE -Wno-everything)
target_include_directories(RecompiledFuncs PRIVATE ${RECOMP_INCLUDES})
file(GLOB FUNC_C_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES})

# --- Patches (Phase 4): C patches cross-compiled to MIPS -> patches.elf -> N64Recomp ---
# Mirrors BMHero's pipeline: make builds patches/patches.elf with a MIPS-capable clang,
# N64Recomp (patches.toml) recompiles it into RecompiledPatches/patches.c + patches.bin,
# file_to_c embeds the bin, and PatchesLib links it all into the exe.
# NOTE: BOTH the VS BuildTools and llvm.org WINDOWS clang builds are trimmed to x86/ARM
# backends (no MIPS), so the compiler here is `zig cc` (Zig bundles a full LLVM). lld's
# MIPS support is unconditional, so any ld.lld links fine. Override on the configure
# line (-DPATCHES_C_COMPILER=... -DPATCHES_LD=... -DPATCHES_MAKE=...), put the tools on
# PATH, or set them in an uncommitted local-config.cmake. See BUILDING.md.
include(${CMAKE_SOURCE_DIR}/local-config.cmake OPTIONAL)
if(NOT DEFINED PATCHES_C_COMPILER)
    find_program(ZIG_EXE zig)
    if(ZIG_EXE)
        set(PATCHES_C_COMPILER "${ZIG_EXE} cc")
    else()
        message(FATAL_ERROR "Patches need a MIPS-capable C compiler. Install zig and put it on PATH, "
                            "or configure with -DPATCHES_C_COMPILER=\"<path-to-zig> cc\" (see BUILDING.md).")
    endif()
endif()
if(NOT DEFINED PATCHES_LD)
    find_program(PATCHES_LD_EXE ld.lld)
    if(PATCHES_LD_EXE)
        set(PATCHES_LD "${PATCHES_LD_EXE}")
    else()
        message(FATAL_ERROR "Patches need ld.lld (any LLVM release; MIPS support is unconditional). "
                            "Put it on PATH or configure with -DPATCHES_LD=<path> (see BUILDING.md).")
    endif()
endif()
if(NOT DEFINED PATCHES_MAKE)
    find_program(PATCHES_MAKE_EXE NAMES mingw32-make make)
    if(PATCHES_MAKE_EXE)
        set(PATCHES_MAKE "${PATCHES_MAKE_EXE}")
    else()
        message(FATAL_ERROR "Patches need GNU make (mingw32-make or make). "
                            "Put it on PATH or configure with -DPATCHES_MAKE=<path> (see BUILDING.md).")
    endif()
endif()

add_custom_target(PatchesBin
    COMMAND ${CMAKE_COMMAND} -E env "CC=${PATCHES_C_COMPILER}" "LD=${PATCHES_LD}" "${PATCHES_MAKE}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/patches
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/patches/patches.elf
)

# Recompile patches.elf into patches.c/patches.bin (plus the patch section table).
add_custom_command(OUTPUT
                       ${CMAKE_SOURCE_DIR}/patches/patches.bin
                       ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
                       ${CMAKE_SOURCE_DIR}/RecompiledPatches/recomp_overlays.inl
                       ${CMAKE_SOURCE_DIR}/RecompiledPatches/funcs.h
                   COMMAND ./N64Recomp patches.toml
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                   DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.elf
)

# Generate patches_bin.c from patches.bin (symbol names match src/main/register_patches.cpp).
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c
    COMMAND file_to_c ${CMAKE_SOURCE_DIR}/patches/patches.bin wcw_patches_bin ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.h
    DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.bin file_to_c
)

add_library(PatchesLib STATIC)
target_compile_options(PatchesLib PRIVATE
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
)
target_include_directories(PatchesLib PRIVATE ${RECOMP_INCLUDES})
target_sources(PatchesLib PRIVATE
    ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
    ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c
)

# --- DXC setup for the frontend's shaders ---
# build_vertex_shader/etc. (defined in rt64's CMakeLists) resolve ${DXC} and ${DXC_*_OPTS}
# in the CALLER's scope. rt64 sets them in its own subdir scope, so recompui (a sibling
# subdir) sees them empty -> the DXC command becomes malformed. Set them here in the parent
# scope so they propagate into RecompFrontend. (Mirrors BMHero's CMakeLists.)
set(DXC_COMMON_OPTS "-I${RT64DIR}/src")
set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set(DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set(DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set(DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")
if (WIN32)
    set(DXC "${RT64DIR}/src/contrib/dxc/bin/x64/dxc.exe")
endif()

# --- SDL2 (recompui/recompinput expect sdl2_SOURCE_DIR on Windows) ---
if (WIN32)
    include(FetchContent)
    set(SDL2_VERSION "2.30.3")
    FetchContent_Declare(sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip)
    FetchContent_MakeAvailable(sdl2)
endif()

# --- RecompFrontend (recompui + recompinput) ---
set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${NMR})
set(RECOMP_FRONTEND_RT64_PATH ${RT64DIR})
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RecompFrontend)

# --- Main executable ---
add_executable(WCWRecompiled)

# App icon: exe resource (Windows title bar/taskbar/Explorer) + embedded PNG bytes for
# the launcher thumbnail (GameEntry.thumbnail_bytes).
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
    COMMAND file_to_c ${CMAKE_SOURCE_DIR}/icons/app.png icon_bytes ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
    DEPENDS ${CMAKE_SOURCE_DIR}/icons/app.png file_to_c
)
target_sources(WCWRecompiled PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c)
if (WIN32)
    enable_language(RC)
    target_sources(WCWRecompiled PRIVATE ${CMAKE_SOURCE_DIR}/icons/app.rc)
endif()
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/main/main.cpp
    ${CMAKE_SOURCE_DIR}/src/main/register_overlays.cpp
    ${CMAKE_SOURCE_DIR}/src/main/register_patches.cpp
    ${CMAKE_SOURCE_DIR}/src/game/recomp_api.cpp
    ${CMAKE_SOURCE_DIR}/rsp/wcw_audio.cpp
)
target_include_directories(WCWRecompiled PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${NMR}/N64Recomp/include
    ${NMR}/thirdparty/sse2neon
    ${RT64DIR}/src
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompinput/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
if (WIN32)
    target_include_directories(WCWRecompiled PRIVATE ${sdl2_SOURCE_DIR}/include)
    target_link_directories(WCWRecompiled PRIVATE ${sdl2_SOURCE_DIR}/lib/x64)
    target_link_libraries(WCWRecompiled PRIVATE SDL2 Winmm.lib)
    # Release is a windowed app (no console flash on double-click); stdio goes to a log
    # file in the app folder unless launched with --show-console or redirected handles
    # (see wcw_setup_logging in src/main/main.cpp). Debug keeps the console.
    set_target_properties(WCWRecompiled PROPERTIES
        LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
    if (MSVC)
        target_link_options(WCWRecompiled PRIVATE /OPT:NOICF)
    endif()
    # Bump the default thread stack reserve: ultramodern's gfx_thread (a std::thread, which
    # uses this PE default) reaches D3D12CreateDevice deep in its call stack, and that call
    # is very stack-hungry — on the default 1 MB it overflows (0xC0000409). 16 MB is safe.
    target_link_options(WCWRecompiled PRIVATE /STACK:16777216)
    # Copy SDL2 + dxc DLLs next to the exe.
    add_custom_command(TARGET WCWRecompiled POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${RT64DIR}/src/contrib/dxc/bin/x64/dxil.dll"
            "${RT64DIR}/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:WCWRecompiled>)
endif()

# Copy the runtime UI assets and SDL controller db next to the exe so a fresh build runs
# from the build dir (recompui loads ./assets relative to the program dir).
add_custom_command(TARGET WCWRecompiled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:WCWRecompiled>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt" "$<TARGET_FILE_DIR:WCWRecompiled>")

# PatchesLib MUST come before RecompiledFuncs: patched functions share their C symbol
# name with the base recompiled copies, and the patch takes effect by winning static-lib
# symbol resolution order (mirrors BMHero).
target_link_libraries(WCWRecompiled PRIVATE
    PatchesLib
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    nfd
)
target_sources(WCWRecompiled PRIVATE ${SOURCES})
set_property(TARGET WCWRecompiled PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# librecomp's RSP vector-unit header (rsp_vu_impl.hpp, pulled in by rsp/wcw_audio.cpp)
# uses SSSE3/SSE4.1 intrinsics; -march=nehalem enables them (mirrors BMHero).
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(WCWRecompiled PRIVATE
        -march=nehalem
        -fno-strict-aliasing
        -fms-extensions
    )
else()
    target_compile_options(WCWRecompiled PRIVATE
        -fno-strict-aliasing
        -fms-extensions
    )
endif()
