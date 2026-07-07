# v0.1.0 release notes — DRAFT

Paste the changelog below into the draft release that CI creates on tag push
(replacing the template's TODO section; the evergreen notes are already in the
template). Delete this file after publishing.

---

## First public beta

**WCW vs. nWo World Tour: Recompiled** is a native PC port of the N64 original,
produced by statically recompiling the game's MIPS code into C — not an emulator,
not a decompilation. This is the first public release.

- **Fully playable**: boots, renders, and runs complete matches with sound in all
  modes — Exhibition, League, League Challenge, Tournament, and WCW vs. nWo.
- **Plug and play**: launch, pick your ROM (US release) once, play. The launcher
  validates and remembers it.
- **Local multiplayer for up to 4 players**: controllers are auto-assigned to
  players as you plug them in; keyboard drives player 1 by default.
- **High-resolution rendering** through RT64 with the game's original visual
  effects intact; widescreen via the Expand aspect-ratio option (default).
- **Saves and rumble without pak juggling**: the port emulates a hybrid
  Controller/Rumble Pak, so saves always persist and rumble works after answering
  the in-game prompt — no swapping, no risk to save data.
- **Controls tailored to the game**: World Tour's d-pad movement and analog taunt
  are mapped sensibly to modern pads (move on stick or d-pad, taunt on right
  stick, mirrored trigger defense); everything is rebindable in the in-game menu
  (Esc / Back button).

### Known issues

- Framerate is locked to the original (frame interpolation warps geometry in this
  engine; high-framerate support is planned).
- Rumble is player 1 only (matches the game's own single-pak handling).
- Overlays (MSI Afterburner) and software like Wallpaper Engine can interfere
  with rendering.
