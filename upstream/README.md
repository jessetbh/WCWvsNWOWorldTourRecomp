# upstream/ — issue drafts for bugs found in the runtime stack

Bugs discovered while porting WCW vs. nWo World Tour that are **not WCW-specific** —
any recomp project can hit them. Each file is a ready-to-paste GitHub issue body.
The corresponding local fixes are applied in `lib/` and checked in as `lib-patches/`.

| Draft | Target repo | Status |
|-------|-------------|--------|
| `n64modernruntime-message-pump-starvation.md` | https://github.com/N64Recomp/N64ModernRuntime | ⬜ not filed |
| `rt64-zero-viewproj-nan.md` | https://github.com/rt64/rt64 | ⬜ not filed |
| `rt64-present-blit-race.md` | https://github.com/rt64/rt64 | ⬜ not filed |
| `plume-copytextureregion-null-deref.md` | https://github.com/renderbag/plume | ⬜ not filed |

Update the Status column when an issue is filed (add the issue link).

Not drafted (already a known upstream TODO): RT64's frame interpolation assumes
1 workload = 1 game frame (`rt64_workload_queue.cpp`), which breaks games that build a
frame from multiple RSP tasks (all AKI titles). Worth a comment on any existing issue
if one appears.
