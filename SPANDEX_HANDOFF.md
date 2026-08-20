# SPANDEX — Collaborator Handoff

A recap of a working session with Claude Code on SPANDEX, plus the conventions that came out of
it — meant so a collaborator can pick up working on this project with an AI coding assistant and
land in the same groove, not relearn these the hard way.

## The project

SPANDEX is a repitch/warp audio player: JUCE C++, ships as both a Standalone app and a VST3
plugin from one shared codebase. Repo: https://github.com/mirrorrmaze/spandex-repitch-player
(public, `main` branch). The durable technical reference — build commands, architecture, the
self-test harness, packaging steps — lives in [`CLAUDE.md`](CLAUDE.md) in the project root.
**Read that first**; this doc covers what CLAUDE.md doesn't: how this session actually went and
the working habits that came out of it.

There's a sibling project, **Multiband Convolver** (https://github.com/mirrorrmaze/multiband-convolver),
a convolution reverb plugin from the same author/workflow. It gets called out below wherever a
convention applies to both.

## What happened in this session

Roughly in order:

1. **FX chain routing + sampler-style loop controls.** Added a drag-to-reorder strip at the
   bottom of the FX tab (EQ/Reverb/Delay/Shifter/Smudge/Lossy/Gain, any order), and reworked loop
   controls to be more like Ableton's Sampler — a "From Start" toggle (Play always retriggers
   from the Sample Start marker) and "Link" (dragging Sample Start drags Loop Start with it).

2. **Loop controls legibility + drag-to-shift.** A layout bug (a fixed-pixel box that didn't grow
   with the window) was making loop control text illegible even at full screen — fixed by
   correcting the actual root cause, not just bumping font size. Also added dragging the middle
   of the loop region to shift it across the sample without changing its length.

3. **In-app update checker.** SPANDEX now quietly checks GitHub Releases on launch and surfaces a
   passive notice — see "Where the update notice appears" below — matching a feature already
   shipped on Multiband Convolver.

4. **A real process gap, caught proactively, not reported by the user:** SPANDEX had been
   shipping via `gh release upload v0.1.0-macos-test <file> --clobber` — repeatedly overwriting
   the same tag instead of incrementing. Since the update checker compares the build's version
   against the *latest* release tag, this meant it could never detect a future update (nothing
   higher to compare against). Fixed by adopting proper incrementing releases (`gh release create
   vX.Y.Z ...`), matching how Multiband Convolver already did it.

5. **Installer naming + Dropbox structure, standardized across both projects.** Installer
   filenames now bake in the version (`SPANDEX-Setup-Windows-v0.2.1.exe`, not a bare
   `SPANDEX-Setup-Windows.exe`), and Multiband Convolver's Dropbox folder — previously a single
   zipped bundle — was restructured to match SPANDEX's flat, loose-files-plus-`README.txt`
   layout. GitHub Release asset names were renamed to match.

6. **FX panel clipping/shrinking bug — the multi-round fix.** Worth walking through since it's a
   good example of how the iteration actually went:
   - Reported: Freq Shifter's knobs clipped at the window's smallest size.
   - Root cause found by reading the layout code (not by guessing from the screenshot): every FX
     card's knob row was hardcoded to a fixed 112px height, regardless of how much space the card
     actually had left. Freq Shifter's mode dropdown ate space its siblings didn't, so it alone
     came up short.
   - Attempt 1 (cap knob height at whatever's left): fixed the clipping, but now Freq Shifter's
     knobs shrank *inconsistently smaller* than every other card after a resize.
   - Attempt 2 (reserve the dropdown's space uniformly across every card): fixed the
     inconsistency, but now *every* card's knobs shrank even when the window had plenty of room —
     visibly worse.
   - Attempt 3 (move the dropdown below the knobs instead of above): every card now computes the
     same knob height as its siblings by construction — but the dropdown itself could still be
     squeezed to zero height at the smallest window size.
   - Attempt 4 (reserve the dropdown a small guaranteed minimum footprint before the knobs claim
     what's left): dropdown always visible, knobs full-size whenever there's room — but knobs
     still shrank on any vertical resize down.
   - **Final fix**: stopped trying to squeeze the FX panel into whatever height the window has at
     all. It now lays out at its natural full size always and lives inside a vertical-scrolling
     `Viewport` — a short window scrolls, nothing ever shrinks or clips.
   - Each round was verified against the *user's own screenshot* of the actual running build, not
     assumed fixed from code review alone — see the testing convention below.

7. **Shipped as v0.2.1**, with a GitHub Release, updated Dropbox installers, and a CHANGELOG
   entry — see "Where the update notice appears" and the versioning convention below.

## Where the update notice appears

On launch, SPANDEX quietly checks GitHub Releases for something newer than the build's own
version. If one exists, the **"..." (Settings) button** in the top-right of the header tints and
gets an "Update available: vX.Y.Z" item at the top of its menu, which opens the release page in
the browser. It's passive only — no popup/dialog, and nothing downloads or installs
automatically. Same mechanism on Multiband Convolver.

## Working conventions established this session

These are the habits worth keeping if you're working on this codebase with an AI assistant:

- **Don't trust synthetic UI-click automation to verify a fix.** It's proved unreliable in this
  environment — even large, easy click targets don't register reliably. The working pattern
  instead: verify everything that's genuinely verifiable non-interactively (the `--selftest` CLI
  harness for DSP, code review, and layout math for UI), then build the change and **ask the user
  to actually click through it and report back** — including asking for a real screenshot rather
  than assuming a fix worked. The FX panel saga above is the clearest example: every round of
  that fix was driven by a real screenshot of the real bug, not a guess.
- **Don't steal window/mouse focus from a running DAW.** Testing happens live in Ableton, often
  mid-session on unrelated work — check whether it's even open before any window-capture attempt,
  and prefer non-focus-stealing capture methods for passive checks.
- **Ask before pushing to `main` or publishing a release**, even when a fix is confirmed working
  locally — a push here triggers a real CI build and a release is a real, public, shared artifact.
  Once trust is established for a specific change, it's fine to say "go ahead" and have the whole
  ship cycle (build → installer → GitHub Release → Dropbox update) happen in one go without
  re-confirming every step.
- **Versioning is load-bearing, not cosmetic.** The update checker compares the compiled version
  against the *latest* GitHub Release tag — so any change worth shipping bumps
  `project(SPANDEX VERSION X.Y.Z)` in `CMakeLists.txt` (and the matching `MyAppVersion` in
  `installer/SPANDEX.iss`, and the `pkgbuild --version` in the macOS CI workflow — these three
  have drifted out of sync before; check all three when bumping) and publishes an actual new,
  higher-numbered release, not a `--clobber` overwrite onto an old tag.
- **Installer filenames carry the version, and only the current version lives in the tester
  Dropbox folder.** `D:\Dropbox\01 Main\06 Devices\VST PROJECT ALPHA INSTALLERS\<Product>\` holds
  loose installer files (no zipping) plus a tester-facing `README.txt`, named like
  `<Product>-Setup-Windows-vX.Y.Z.exe`. When a new version ships, the previous version's files get
  removed from that folder — older builds stay recoverable from GitHub Releases if ever needed,
  they just don't clutter the folder friends actually download from. This structure is meant to
  stay identical across every plugin project, not just SPANDEX.
- **Keep `README.md` and `CHANGELOG.md` current in the same pass as the change**, not as later
  cleanup — new features, DSP behavior changes, and UI redesigns each get a CHANGELOG entry and a
  relevant README update. When a change affects what's on screen, the paired
  `docs/screenshot-*.png` files need refreshing too — ask the user for a real screenshot of the
  actual loaded/populated UI rather than faking it with an empty capture.
- **Feedback here tends to be specific and technically literate** (production references like "OTT
  preset," DAW-specific behavior comparisons like Ableton Sampler's Start/Loop-Start Link). When
  feedback says a control "doesn't do much" or "looks wrong," treat it as a bug report and dig for
  the actual architectural cause rather than a surface tweak — see the FX panel saga, or the
  earlier Compression rework (an instantaneous-peak detector was diluting gain reduction; fixed
  with an RMS-style detector, verified via a before/after dynamic-range-ratio self-test).
- **Git hygiene**: prefer new commits over amending, never force-push without being asked, confirm
  before any destructive operation. Feature branches for larger multi-part work; direct commits to
  `main` are fine for small, well-scoped fixes once the pattern's established.

## Current state (as of this session)

- **SPANDEX**: `v0.2.1` on GitHub, Dropbox tester folder up to date.
- **Multiband Convolver**: `v0.2.0` on GitHub, Dropbox tester folder restructured to match
  SPANDEX's layout (see above).
- Both projects' Dropbox folders now follow the identical loose-files-plus-`README.txt`
  structure, and both installer naming schemes match (`<Product>-Setup-Windows-vX.Y.Z.exe` /
  `<Product>-Installer-macOS-<arch>-vX.Y.Z.pkg`).

## For whoever picks this up

Start with [`CLAUDE.md`](CLAUDE.md) for the technical reference (build commands, architecture,
the self-test harness, full packaging steps). This doc is the "why it's set up this way" and "how
this actually goes" companion to that — worth keeping both current as conventions evolve, the
same way this session updated CLAUDE.md's packaging section when the versioned-filename
convention was adopted.
