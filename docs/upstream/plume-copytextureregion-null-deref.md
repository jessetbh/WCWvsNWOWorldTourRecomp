# D3D12: `copyTextureRegion` crashes on texture→buffer copies (null deref in `setSamplePositions`)

**File**: `plume_d3d12.cpp`
**Found in**: WCW vs. nWo World Tour recomp while adding a swapchain readback, but any
texture→buffer copy through this path crashes.

## Symptom

Immediate crash (null pointer dereference) on any `copyTextureRegion` call whose
destination is a buffer (`PlacedFootprint`), e.g. reading back a render target or
swapchain image to CPU-visible memory.

## Root cause

`D3D12CommandList::copyTextureRegion` calls `setSamplePositions(dstLocation.texture)`
unconditionally, but for buffer destinations `dstLocation.texture` is null:

```cpp
const D3D12_TEXTURE_COPY_LOCATION copyDstLocation = toD3D12(dstLocation);
const D3D12_TEXTURE_COPY_LOCATION copySrcLocation = toD3D12(srcLocation);
setSamplePositions(dstLocation.texture);   // <-- null for PlacedFootprint destinations
d3d->CopyTextureRegion(...);
```

## Fix

```cpp
if (dstLocation.texture != nullptr) {
    setSamplePositions(dstLocation.texture);
}
```

(If sample positions could matter for the source in the texture→buffer case, using
`srcLocation.texture` there may be more correct still — but the null guard alone makes
texture→buffer copies work.)
