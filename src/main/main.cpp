// Entry point / launcher glue for WCW vs. nWo World Tour: Recompiled.
//
// STUB. The real launcher wires up the N64ModernRuntime + RecompFrontend (window,
// input, RT64 renderer, config, ROM selection) and then starts the recompiled game,
// after registering this game's overlays and patches. See BMHero's src/main/main.cpp
// for the full pattern; the exact runtime API is provided by lib/RecompFrontend and
// lib/N64ModernRuntime (added as submodules in Phase 3).

#include "ovl_patches.hpp"

// Forward declarations for the registration glue (defined in this folder).
// These run before the recompiled game starts so the runtime can resolve sections,
// overlays, and patched functions.
static void register_game() {
    wcw::register_wcw_overlays();
    wcw::register_wcw_patches();
}

// TODO(wcw): implement the actual launcher entry once lib/RecompFrontend is present.
// It will look roughly like BMHero: set up game metadata (name, ROM hash, save folder),
// call the registration above, then hand control to the frontend's main loop.
//
// int main(int argc, char** argv) {
//     register_game();
//     return recomp::frontend::run(...);
// }

// Keep the symbol referenced so the stub compiles in isolation during early scaffolding.
void wcw_main_placeholder() {
    register_game();
}
