#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

// Game-side runtime API callable from MIPS patches (adapted from BMHero's
// src/game/recomp_api.cpp). Every function here needs a matching address entry in
// patches/syms.ld and is exposed to patches via the generated manual_patch_symbols
// table — only implement (and list in syms.ld) what patches actually use.

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stdout);
    }
    // Redirected stdout is fully buffered; without a flush, output sits in the CRT
    // buffer and is lost if the process is killed (this is a debugging API — flush).
    fflush(stdout);
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}

// Aspect ratio a patch could build its 3D projection with. Same semantics as BMHero's:
// Original → the game's own ratio unchanged; Expand → the window's ratio, never below the
// original. WCW itself no longer uses it: RT64's Expand mode already widens the camera's
// full-width perspective projection (and stretches its viewport), so a game-side widening
// on top doubled it (issue #2: geometry squeezed by 4/3 / aspect, 0.57x at 21:9). Kept for
// mods (syms.ld 0x8F000084).
extern "C" void recomp_get_target_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    float original = _arg<0, float>(rdram, ctx);
    const ultramodern::renderer::GraphicsConfig& config = ultramodern::renderer::get_graphics_config();

    if (config.ar_option == ultramodern::renderer::AspectRatio::Expand) {
        int width = 0, height = 0;
        recompui::get_window_size(width, height);
        if (width > 0 && height > 0) {
            _return(ctx, std::max(static_cast<float>(width) / height, original));
            return;
        }
    }
    _return(ctx, original);
}

namespace {
    float word_float(uint32_t w) {
        float f;
        memcpy(&f, &w, sizeof(f));
        return f;
    }

    uint32_t float_word(float f) {
        uint32_t w;
        memcpy(&w, &f, sizeof(w));
        return w;
    }

    // Gauss-Jordan inverse of a row-major 4x4; false when singular.
    bool invert4(const float *m, float *out) {
        float a[4][8];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                a[r][c] = m[r * 4 + c];
                a[r][c + 4] = (r == c) ? 1.0f : 0.0f;
            }
        }

        for (int c = 0; c < 4; c++) {
            int pivot = c;
            for (int r = c + 1; r < 4; r++) {
                if (fabsf(a[r][c]) > fabsf(a[pivot][c])) {
                    pivot = r;
                }
            }

            if (fabsf(a[pivot][c]) < 1e-12f) {
                return false;
            }

            for (int k = 0; k < 8; k++) {
                std::swap(a[c][k], a[pivot][k]);
            }

            const float inv = 1.0f / a[c][c];
            for (int k = 0; k < 8; k++) {
                a[c][k] *= inv;
            }

            for (int r = 0; r < 4; r++) {
                if ((r != c) && (a[r][c] != 0.0f)) {
                    const float f = a[r][c];
                    for (int k = 0; k < 8; k++) {
                        a[r][k] -= f * a[c][k];
                    }
                }
            }
        }

        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                out[r * 4 + c] = a[r][c + 4];
            }
        }

        return true;
    }
}

// Ray tracing (patches/raytracing.c): view = camera matrix (D_80042C80, V x P as the game or a
// mod left it) x P^-1, so RT64's V x P always equals the matrix the game draws with. Done on the
// host because the patch toolchain puts floats in odd FPU registers, which N64Recomp's FR=0
// model splits from the arithmetic that reads them. Leaves `view` alone when P is singular.
extern "C" void recomp_wcw_rt_camera_view(uint8_t* rdram, recomp_context* ctx) {
    constexpr gpr CAMERA_MATRIX = (gpr)(int32_t)0x80042C80u;
    const gpr view = ctx->r4;
    const gpr proj = ctx->r5;
    float p[16], p_inv[16], c[16];
    for (int i = 0; i < 16; i++) {
        p[i] = word_float(MEM_W(i * 4, proj));
        c[i] = word_float(MEM_W(i * 4, CAMERA_MATRIX));
    }

    if (!invert4(p, p_inv)) {
        return;
    }

    for (int r = 0; r < 4; r++) {
        for (int k = 0; k < 4; k++) {
            float sum = 0.0f;
            for (int j = 0; j < 4; j++) {
                sum += c[r * 4 + j] * p_inv[j * 4 + k];
            }

            MEM_W((r * 4 + k) * 4, view) = float_word(sum);
        }
    }
}
