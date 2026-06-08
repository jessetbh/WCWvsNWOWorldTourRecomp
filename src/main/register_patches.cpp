#include "ovl_patches.hpp"
#include "../../RecompiledPatches/recomp_overlays.inl"

#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"

extern "C" {
    extern const char wcw_patches_bin[];
    extern const size_t wcw_patches_bin_size;
}

// Registers the recompiled C patches (and any exports/events) with the runtime.
// recomp_overlays.inl here is GENERATED from patches.toml (Phase 2/4).
void wcw::register_wcw_patches() {
    recomp::overlays::register_patches(wcw_patches_bin, wcw_patches_bin_size, section_table, ARRLEN(section_table));
    recomp::overlays::register_base_exports(export_table);
    recomp::overlays::register_base_events(event_names);
    recomp::overlays::register_manual_patch_symbols(manual_patch_symbols);
}
