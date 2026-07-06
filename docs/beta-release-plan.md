# Public beta release plan

Goal: open-source this repo and publish a downloadable beta, matching the structure and
release conventions of the established N64Recomp ports. This file is the working
checklist; delete or archive it at release.

## Reference projects surveyed (2026-07-05)

| Project | Notes |
|---|---|
| [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) | The flagship; GPLv3; org-owned; Windows/Linux/macOS/Flatpak releases; mods via Thunderstore |
| [BMHeroRecomp](https://github.com/RevoSucks/BMHeroRecomp) | Our direct template (local at `C:\Users\selki\depot\BMHeroRecomp`); uses RecompFrontend like us; GPLv3; full CI |
| [Goemon64Recomp](https://github.com/klorfmorf/Goemon64Recomp) | Same shape; personal-account repo; Windows + Linux zips |

Key conventions all of them share:
- **GPLv3**, license file named `COPYING`.
- README opens with a bold banner: *"This repository and its releases do not contain
  game assets. The original game is required to build or run this project."*
- README sections (BMHero order): Title/intro → banner → Table of Contents → System
  Requirements → Features (Plug and Play first) → FAQ (what is static recomp, savefile
  location, how to choose a ROM, portable mode) → Known Issues → Building (one line,
  points at BUILDING.md, states building is NOT required to play) → Libraries Used and
  Projects Referenced → Special Thanks.
- **ROM intake is "Plug and Play"**: the app launches to a menu where the user provides
  their ROM; it validates and remembers it. Nobody uses drop-a-file-named-x.z64.
- **Symbols live in a separate repo** added as a root submodule (`Zelda64RecompSyms`,
  `BMHeroSyms`).
- **Submodules point at upstream** (`N64Recomp/N64ModernRuntime`, `rt64/rt64`,
  `N64Recomp/RecompFrontend`) — works for them because their maintainers land fixes
  upstream. Our `[wcw fix]` set was declined upstream, so WE need forks (see B1).
- **CI has the ROM**: a *private* GitHub repo holds the ROM; workflows check it out with
  a PAT stored in Actions secrets (`secrets.BMHERO_REPO_WITH_PAT` + token). CI then
  builds N64Recomp at a pinned commit, runs the recompile, builds the port, and packages.
  Fork PRs run a `validate-external` variant without the secret.
- **Release assets**: `<ExeName>-<tag>-<OS>.zip` (e.g.
  `Zelda64Recompiled-v1.2.2-Windows.zip`). Windows zip = exe + `assets/` +
  `recompcontrollerdb.txt` + runtime DLLs (SDL2, dxil, dxcompiler). PDBs are separate CI
  artifacts, not in the release zip. Release notes = changelog bullets + evergreen notes
  (save-data compatibility, GPU driver advisories).
- Both keep `icons/` (app icon) and `flatpak/` (Linux packaging) at root; Zelda64Recomp
  also has `docs/` and `.github/` issue/PR templates.

Corrections to the earlier draft of this plan:
- CI release builds ARE feasible (private ROM repo + PAT) — adopt it, don't hand-build.
- BMHero's `assets/` also still contains the Banjo-era art (`Banjo.svg`, `Jiggy*.svg`,
  Suplexmentary font...) — it's lineage cruft, referenced by the shared `recomp.rcss`
  theme. Trim with care and verify the menus still render; don't assume it's unused.
- BMHero's Windows CI cross-compiles patches with the plain
  `LLVM-19.1.3-Windows-X64.tar.xz` release archive (NOT the `-pc-windows-msvc` one we
  tried) — that artifact evidently includes the MIPS backend. Worth one test; if it
  works we can drop the zig dependency and match BMHero's CI verbatim.

---

## Phase A — Legal & licensing

- [ ] `COPYING` = GPLv3 (ecosystem standard; also the pragmatic choice given GPL
      dependencies in the stack).
- [ ] README banner + FAQ language mirroring BMHero's (no assets distributed; ROM
      required; "only accepts one specific ROM: the US version").
- [ ] Third-party credits section ("Libraries Used and Projects Referenced"): N64Recomp,
      N64ModernRuntime, RecompFrontend, RT64, SDL2, RmlUi, moodycamel, dxc, nfd, zstd,
      plume, fonts (Inter/OFL, Noto Emoji/OFL, promptfont), splat/spimdisasm, and
      BMHero + Zelda64Recomp as reference projects.
- [ ] Decide the public name + exe name now (pattern: `WCWRecompiled` /
      "WCW vs. nWo World Tour: Recompiled"). Trademark exposure is the owner's call;
      the ecosystem precedent is nominative use of the full game title.

## Phase B — Repo restructure (target = BMHero layout)

Target root tree after restructure:

```
.github/            workflows (validate*, update-pr-artifacts), issue/PR templates
WCWSyms/            submodule -> new WCWSyms repo (dump.toml, data_dump.toml, README)
assets/             tracked runtime UI assets (trimmed, license-audited)
docs/               devlog.md (CLAUDE.md history), research notes, this plan
disasm/             splat project (our "decomp substitute" — keep, it's the syms source)
icons/              app icon (original art only)
include/ lib/ patches/ rsp/ src/ tools/
BUILDING.md CMakeLists.txt CONTRIBUTING.md COPYING README.md
wcw.toml patches.toml recompcontrollerdb.txt .gitmodules .gitignore
```

- [ ] **B1: lib/ submodules.** Fork `N64ModernRuntime`, `RecompFrontend`, `rt64` to the
      owner's GitHub; commit the `[wcw fix]` sets on a `wcw` branch of each (source:
      `lib-patches/*.patch` + local trees); pin as submodules. Keep `lib-patches/` as
      the diff-vs-upstream record or retire it. (Upstream-pointing submodules à la
      BMHero are not an option for us — our fixes were declined upstream.)
- [ ] **B2: WCWSyms repo.** Move `syms/dump.toml` + `syms/data_dump.toml` (+ README) to
      a new `WCWSyms` repo, submodule at root (matches BMHeroSyms/Zelda64RecompSyms).
      The checked-in TOMLs make builds independent of splat/gen_symbols.py;
      `disasm/` + `tools/gen_symbols.py` remain as the regeneration tooling.
- [ ] **B3: assets/.** Create tracked `assets/` from BMHero's, TRIMMED: keep
      `recomp.rcss`, `InterVariable.ttf`, `NotoEmoji-Regular.ttf`, `promptfont/`,
      `icons/`; replace/remove Banjo-era art (`Banjo.svg`, `Jiggy*.svg`, `Kazooie.svg`,
      `BMHeroLogo.svg`, clouds/embers, `Suplexmentary Comic NC.ttf`,
      `NimbusSansNarrow-Bold.ttf`, `Logo.svg`/`RecompiledLogo.svg`) with WCW-appropriate
      originals where the launcher theme needs them. `recomp.rcss` references some of
      these — adjust it and verify launcher + config menus render. Add CMake post-build
      copy so a fresh clone works.
- [ ] **B4: recompcontrollerdb.txt.** BMHero/Zelda ship SDL's controller db at root and
      in the zip; we don't have it — check whether recompinput loads it, add it.
- [ ] **B5: de-personalize the build.** CMakeLists PATCHES_* defaults → `find_program`
      + clear errors (test BMHero's portable-LLVM approach first, see corrections
      above); sweep `rg -i selki` across CMakeLists/BUILDING/tools/patches Makefile.
- [ ] **B6: docs shuffle.** CLAUDE.md's historical log → `docs/devlog.md`; CLAUDE.md
      becomes lean contributor instructions. `upstream/` → `docs/upstream/`. Keep
      `run/` (standalone harness) and `cmake/coretest/` or fold their READMEs into docs
      and drop the code — owner's preference.
- [ ] **B7: history scrub + publish prep.** `git filter-repo` the three MinGW DLL blobs
      out (or accept); confirm author-email choice; repo description/topics; issue
      template (asks GPU, driver, log, ROM SHA1) + PR template; `CONTRIBUTING.md`.

## Phase C — Runtime polish (match "Plug and Play")

- [ ] **C1: launcher ROM intake.** Enable the recompui launcher flow we currently
      bypass (supported_games is populated; nfd file dialog already links): first run
      shows the menu → user supplies ROM → validated (SHA1
      `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`) → remembered. This is the single
      biggest structural gap vs. the ecosystem ports. Includes the "different ROM
      path?" FAQ behavior.
- [ ] **C2: config/save location** → `%LOCALAPPDATA%\WCWRecompiled\` (Zelda FAQ
      documents theirs the same way) + portable-mode note in FAQ. Saves survive
      updates by construction; say so in release notes like Zelda does.
- [ ] **C3: app identity.** Original-art app icon in `icons/` (no game/WWE assets),
      window title + version, exe version resource.
- [ ] **C4: subsystem + logging.** Release `/SUBSYSTEM:WINDOWS`; stderr/stdout → log
      file for bug reports; add `--show-console` flag (Zelda64Recomp added exactly
      this). KEEP the set_terminate/SEH symbolized-backtrace handlers; archive the
      .map/PDB per release as CI artifacts (BMHero uploads PDBs separately).
      Demote always-on `[audio]`/`[present]` 1/s lines to env-gated; keep bounded
      one-shot markers.
- [ ] **C5: CRT/redist check** on a clean machine (static CRT or document the VC++
      redist requirement).
- [ ] **C6: first-run defaults**: aspect ratio Expand, Framerate locked Original (done),
      audio buffering, fullscreen behavior.
- [ ] **C7: known-limitation list for the beta** (documented, not fixed): no frame
      interpolation (locked Original; geometry warps — devlog has the full analysis);
      rumble requires answering the in-game prompt and is P1-only (the emulated pak
      lives in port 1). Multiplayer is DONE (2026-07-05: plug-and-play pad→player
      auto-assignment, up to 4 players, user-verified) — list it as a headline feature.

## Phase D — CI + packaging (mirror BMHero's workflows)

- [ ] **D1: private ROM repo.** New PRIVATE repo (e.g. `wcw-rom-secret`) containing
      `wcw.z64`; fine-grained PAT with read access; Actions secrets like BMHero's
      (`*_REPO_WITH_PAT` + token). NEVER referenced outside workflow secrets.
- [ ] **D2: adapt `validate.yml`** (BMHero's is the template, workflow_call + inputs
      for SDL2_VERSION / pinned N64RECOMP_COMMIT): checkout w/ submodules → fetch ROM
      from secret repo → build N64Recomp/RSPRecomp at pinned commit → `N64Recomp
      wcw.toml` + `RSPRecomp rsp/wcw_audio.toml` → cmake build → package. Windows job
      first (beta is Windows-only); keep the Linux/mac/flatpak lattice of BMHero's file
      as commented scaffolding for later. Plus `validate-internal.yml` /
      `validate-external.yml` (fork PRs, no secrets) and `update-pr-artifacts.yml`.
- [ ] **D3: package step**: `WCWRecompiled-<tag>-Windows.zip` = `WCWRecompiled.exe`,
      `assets/`, `recompcontrollerdb.txt`, `SDL2.dll`, `dxil.dll`, `dxcompiler.dll`,
      `COPYING`, short `README.txt`. Assert no `*.z64`/`saves/`/`*.json`/`*.map` in the
      zip. PDB/map as a separate CI artifact. SHA256SUMS on the release.
- [ ] **D4: versioning + release notes template.** Tag `v0.1.0` (ecosystem starts at
      1.0.0 when "done"; a beta tag like `v0.1.0` or `v1.0.0-beta.1` signals status) —
      owner picks. Notes structure per Zelda: changelog bullets, then evergreen notes
      (save data location/compat, GPU driver advisories, SmartScreen "unsigned exe"
      note).

## Phase E — Docs rewrite (match the template)

- [ ] **README.md** in BMHero's exact section order (see conventions above), with
      screenshots of the running port; status framed as beta with known issues.
      "Building is not required to play — see Releases."
- [ ] **BUILDING.md**: any-machine instructions (prereqs incl. the portable-LLVM-or-zig
      patches toolchain, clone + submodules, provide ROM, three commands, where the exe
      lands). Validate by following it verbatim on a clean checkout.
- [ ] **FAQ entries** (copy the ecosystem's): what is static recompilation / relation
      to decomp (none exists for WCW — we made our own symbols) / where are saves /
      how to change ROM / portable mode.

## Phase F — QA gate (beta)

- [ ] Fresh-machine test from the actual release zip (no dev tools): unzip → launcher
      ROM intake → title → full match → save → relaunch → save persists.
- [ ] Wrong/absent ROM → friendly launcher error (not console vomit).
- [ ] Input matrix: keyboard-only, XInput pad, hotplug, rebinding persists, rumble
      end-to-end on release build.
- [ ] Graphics: windowed/fullscreen/alt-tab/monitor switch; NVIDIA D3D12 primary; AMD/
      Intel if obtainable (note Zelda's AMD-RDNA3-defaults-to-Vulkan precedent).
- [ ] Soak: attract loop for an extended period on the release build.
- [ ] Ship with no json configs in the zip (the controls.json.bak fallback gotcha).

## Publish sequence

1. Phases A–B (repo shape) → push to GitHub as PRIVATE first; wire CI (D) against it.
2. Phase C polish + E docs; QA (F) from a CI-built zip.
3. Flip repo public; tag; GitHub release with zip + SHA256SUMS + template notes.
4. Post-release: watch issues; hotfix branch policy.

## Decisions needed from the project owner

| Decision | Options | Recommendation |
|---|---|---|
| License | — | GPLv3 / `COPYING` (settled by ecosystem survey) |
| Repo home | Personal account vs org | Personal is fine (Goemon, BMHero); org if a team forms |
| lib/ strategy | Forks + submodules vs patch script | Forks (our fixes were declined upstream) |
| Syms | Separate WCWSyms repo vs keep syms/ in-tree | Separate repo (both templates do it) |
| Version scheme | v1.0.0-beta.1 vs v0.x | v0.1.0 reads clearly as beta |
| Name | Full game title vs safer variant | Owner's call (precedent: full title) |
| Author email in history | Keep vs rewrite | Owner's call |
| run/ + coretest | Keep vs docs-only | Keep (cheap, documented) |
