#ifndef __OVL_PATCHES_HPP__
#define __OVL_PATCHES_HPP__

#include "librecomp/sections.h"
#include "librecomp/overlays.hpp"

// Registration entry points implemented in src/main/.
namespace wcw {
    void register_wcw_overlays();
    void register_wcw_patches();
}

#endif
