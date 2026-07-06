# upstream/ — documentation of general bugs found in the runtime stack

**Decision (2026-07-05): these will NOT be filed upstream.** The drafts are kept purely
as documentation of the bugs and their analysis. Do not treat filing them as a pending
task.

Bugs discovered while porting WCW vs. nWo World Tour that are **not WCW-specific** —
any recomp project can hit them. Each file was written as a GitHub-issue-style writeup.
The corresponding local fixes are applied in `lib/` and checked in as `lib-patches/`.

| Draft | Affected repo |
|-------|---------------|
| `n64modernruntime-message-pump-starvation.md` | https://github.com/N64Recomp/N64ModernRuntime |
| `rt64-zero-viewproj-nan.md` | https://github.com/rt64/rt64 |
| `rt64-present-blit-race.md` | https://github.com/rt64/rt64 |
| `plume-copytextureregion-null-deref.md` | https://github.com/renderbag/plume |

Not drafted (already a known upstream TODO): RT64's frame interpolation assumes
1 workload = 1 game frame (`rt64_workload_queue.cpp`), which breaks games that build a
frame from multiple RSP tasks (all AKI titles).
