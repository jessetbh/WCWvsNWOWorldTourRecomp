# Games that never load a projection matrix (G_FORCEMTX-only) render nothing — NaN positions from inverting a zero matrix

**File**: `src/hle/rt64_rsp.cpp`
**Found in**: WCW vs. nWo World Tour recomp. Applies to the whole AKI wrestling family
(WCW/WWF titles) and any other game whose ucode submits fully composed MVPs via
`G_FORCEMTX` without ever loading a projection matrix.

## Symptom

Completely black screen while the game is demonstrably drawing (RDP command streams show
full scenes: tris + texrects with real textures). RenderDoc capture shows every game
draw's vertex-shader output position is NaN.

## Root cause

AKI-engine games never issue a projection-matrix load — they compose model-view-projection
on the CPU and submit it with `G_FORCEMTX` only. `viewProjMatrixStack`'s top therefore
stays at its all-zero reset value. RT64 then computes:

- `invViewProjMatrixStack[top] = hlslpp::inverse(viewProjMatrixStack[top])` — the inverse
  of a singular all-zero matrix — which returns NaN, and
- `world = MVP × inv(VP)` — NaN — poisoning every world transform, and the uploaded
  viewProj is zero.

Every vertex position becomes NaN and nothing rasterizes.

## Fix that worked for us

Treat a never-loaded (all-zero) view-projection as identity at its two consumers, so
`world = MVP` and `viewProj = identity` — preserving `viewProj × world = MVP`:

```cpp
// helper
static bool isMatrixZero(const hlslpp::float4x4 &m) {
    const float *f = (const float *)&m;
    for (int i = 0; i < 16; i++) {
        if (f[i] != 0.0f) return false;
    }
    return true;
}

// consumer 1: where invViewProjMatrixStack is lazily computed
if (isMatrixZero(viewProjMatrixStack[projectionMatrixStackSize - 1])) {
    invViewProjMatrixStack[projectionMatrixStackSize - 1] = hlslpp::float4x4::identity();
} else {
    invViewProjMatrixStack[projectionMatrixStackSize - 1] = hlslpp::inverse(...);
}

// consumer 2: where view/proj/viewProj transforms are appended to drawData
if (isMatrixZero(viewProjMatrixStack[projectionMatrixStackSize - 1])) {
    drawData.viewTransforms.emplace_back(hlslpp::float4x4::identity());
    drawData.projTransforms.emplace_back(hlslpp::float4x4::identity());
    drawData.viewProjTransforms.emplace_back(hlslpp::float4x4::identity());
} else {
    // existing code
}
```

Affects only games that never load a projection matrix (a loaded matrix is never all-zero
in practice), so it is behavior-neutral for everything else. With this guard the game
renders correctly (title, cinematics, full 3D matches).
