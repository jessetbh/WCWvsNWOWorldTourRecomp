# Present blit races the live render target when interpolation is enabled (periodic black/torn presents)

**File**: `src/hle/rt64_present_queue.cpp`
**Found in**: WCW vs. nWo World Tour recomp, but the race is structural.

## Symptom

With frame interpolation active and a game running below display rate, presented frames
are periodically pure black or partially drawn. Measured via a swapchain readback: at
~20 fps game rate, 1 of every 3 presents sampled black.

## Root cause

In the present path, the **base frame** (`i == 0`) present samples the **live** render
target — the same target `threadRenderFrame` concurrently re-renders on another GPU
queue under `workloadMutex`. The present queue only acquires `workloadMutex` when
interpolation is **disabled**:

```cpp
if (presentFb->interpolationEnabled) {
    framesToPresent = frameCounters.count;
}
else {
    lockedWorkloadMutex = true;
    ext.sharedResources->workloadMutex.lock();
}
```

So in the interpolation path the base-frame blit can sample the target mid-re-render —
cleared or partially drawn content goes to the swapchain.

## Fix that worked for us

Take the lock in the interpolation path too, releasing it right after the `i == 0`
present. MSAA multi-frame presents must be excluded (their base frame uses an
interpolated copy and waits on the workload queue — taking the lock there deadlocks):

```cpp
if (!lockedWorkloadMutex && !(usingMSAA && (framesToPresent > 1))) {
    lockedWorkloadMutex = true;
    ext.sharedResources->workloadMutex.lock();
}
// ... release right after the i == 0 present in the loop below.
```

With the lock, the black presents at the base-frame slots disappear (verified by GPU
readback of every presented frame).

Note: a related but separate issue — for games that build one visual frame from multiple
RSP workloads (e.g. AKI titles), interpolated frames re-render only a slice of the frame
because frame matching assumes 1 workload = 1 frame (the existing TODO in
`rt64_workload_queue.cpp`). The lock fixes the race; the multi-workload assumption is why
such games still can't use interpolation.
