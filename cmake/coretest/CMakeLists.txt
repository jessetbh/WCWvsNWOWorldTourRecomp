# Stage-A build: compile the recompiled game + the runtime core (ultramodern + librecomp)
# and LINK them. No RT64 yet. Purpose: prove the recompiled output links against the
# runtime (every osXxx_recomp shim, the recomp runtime API, the section table) — the real
# validation of the libultra integration. Configure with clang-cl (see tools/env-msvc.ps1).
cmake_minimum_required(VERSION 3.20)
project(wcw_coretest LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(WCW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(NMR "${WCW_ROOT}/lib/N64ModernRuntime")

if (WIN32)
    add_compile_definitions(NOMINMAX _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
endif()

# Runtime core (defines targets `ultramodern` and `librecomp`).
add_subdirectory(${NMR} ${CMAKE_BINARY_DIR}/n64mr)

# The recompiled game functions.
add_library(RecompiledFuncs STATIC)
file(GLOB FUNC_SRCS ${WCW_ROOT}/RecompiledFuncs/*.c)
target_sources(RecompiledFuncs PRIVATE ${FUNC_SRCS})
target_include_directories(RecompiledFuncs PRIVATE
    ${WCW_ROOT}/include
    ${NMR}/N64Recomp/include
    ${NMR}/librecomp/include
    ${NMR}/ultramodern/include
)
target_compile_options(RecompiledFuncs PRIVATE -Wno-everything)
# clang-cl: don't let it interpose; match the recomp build expectations.
target_compile_definitions(RecompiledFuncs PRIVATE)

# Minimal exe: force the section table (and thus every recompiled function + every runtime
# shim it references) to link.
add_executable(wcw_coretest
    ${CMAKE_CURRENT_SOURCE_DIR}/coretest_main.cpp
    ${WCW_ROOT}/src/main/register_overlays.cpp
)
target_include_directories(wcw_coretest PRIVATE
    ${WCW_ROOT}/include
    ${NMR}/N64Recomp/include
    ${NMR}/librecomp/include
    ${NMR}/ultramodern/include
)
target_link_libraries(wcw_coretest PRIVATE RecompiledFuncs librecomp ultramodern)
