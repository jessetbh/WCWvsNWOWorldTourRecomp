// Stage-A link test entry. Referencing register_wcw_overlays() pulls in the whole section
// table -> every recompiled function -> every runtime shim/API symbol they need. If this
// links, the libultra integration resolves against librecomp/ultramodern.
#include <cstdio>
#include "ovl_patches.hpp"

int main() {
    wcw::register_wcw_overlays();
    printf("linked + registered overlays OK\n");
    return 0;
}
