# SPANDEX

A repitch/warp player: JUCE C++ desktop audio app, ships as both a Standalone app and a VST3
plugin (Instrument category), built from one shared `MainComponent`/`AudioEngine`. Project
directory is `RepitchDeck` (predates the SPANDEX rename) but the product name everywhere else is
SPANDEX. Full writeup: [README.md](README.md).

GitHub: https://github.com/mirrorrmaze/spandex-repitch-player (public repo, `main` branch).

## Build

Windows (MSVC / Visual Studio 2022 Build Tools):
```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Debug     # fast iteration
cmake --build build --config Release   # for anything shipped
```
`tools/vcpkg` is gitignored, not vendored - bootstrap fresh if missing:
```
git clone https://github.com/microsoft/vcpkg.git tools/vcpkg
./tools/vcpkg/bootstrap-vcpkg.bat
./tools/vcpkg/vcpkg install mp3lame --triplet x64-windows-static-md
```
`COPY_PLUGIN_AFTER_BUILD` auto-installs the VST3 to the system plugin folder on every build - if
Ableton (or the Standalone .exe) is open with the plugin loaded, the copy step fails with a file
lock error. Close it, or build only `SPANDEX_Standalone` if you don't need the VST3 copy.

macOS builds happen via GitHub Actions CI (`.github/workflows/macos-build.yml`), not locally -
no Mac is available for this project. Push to `main` to trigger a build; it produces the raw
VST3/Standalone plus an unsigned installer `.pkg` (`arm64-osx`, Apple Silicon only), all as
downloadable workflow artifacts.

## Verifying changes

There's a hidden CLI self-test: `SPANDEX.exe --selftest <input.wav> <outputDir>` renders offline
under a battery of known settings and logs numeric assertions (RMS levels, peak/level tracking,
frequency detection, dynamic-range ratios, stability under worst-case parameters) to
`%AppData%/SPANDEX/log.txt`, rather than relying on listening or screenshots. **This is the
primary way to verify DSP/logic changes in this project** - add a new assertion block in
`Source/SelfTest.cpp` for any new DSP behavior before considering a change done. A test WAV
(`test_sine_440.wav`, a 440Hz tone) is expected somewhere accessible to pass as the input arg.

## Packaging installers

- **Windows**: Inno Setup script at `installer/SPANDEX.iss`. Build with `ISCC.exe
  installer\SPANDEX.iss` (needs a Release build first) - produces `dist/SPANDEX-Setup.exe`.
- **macOS**: the CI workflow's `Build installer .pkg` step (pkgbuild, unsigned - no Apple
  Developer account set up for this project). Download the `SPANDEX-macOS-arm64-Installer`
  artifact from the triggering run.
- **Versioning matters for the in-app update checker** (`Source/UpdateChecker.cpp`): it compares
  the compiled-in `SPANDEX_VERSION` (from `project(SPANDEX VERSION X.Y.Z)` in `CMakeLists.txt`)
  against the *latest* GitHub Release's tag. Repeatedly uploading to the same tag (the old
  `v0.1.0-macos-test` habit) means already-installed copies can never see a future push as
  newer - there's nowhere higher for the tag to go. So: **every ship that should be detectable
  as an update bumps `project(SPANDEX VERSION ...)` first, then publishes an actual new,
  higher-numbered release** (`gh release create vX.Y.Z <windows-exe> <macos-pkg> --notes-file
  <notes>`), not just `gh release upload ... --clobber` onto an old tag. Matches how
  Multiband Convolver's own update checker is set up (proper incrementing `vX.Y.Z` releases).
  The old `v0.1.0-macos-test` tag is left in place as history, not deleted.
- **Distribution**: finished installers (both platforms) get copied to
  `D:\Dropbox\01 Main\06 Devices\VST PROJECT ALPHA INSTALLERS\SPANDEX` - the user's standing
  location for installers they hand to friends for testing (the parent `VST PROJECT ALPHA
  INSTALLERS` folder holds one subfolder per product - MultibandConvolver's is a sibling
  project's, at `...\Multiband Convolver`, kept in the same loose-files-plus-`README.txt`
  structure, no zipping). Filenames there bake in the version - `SPANDEX-Setup-Windows-vX.Y.Z.exe`
  / `SPANDEX-Installer-macOS-arm64-vX.Y.Z.pkg` - so testers can tell at a glance which build they
  have; **remove the previous version's files when dropping a new one** (only the current release
  lives in the tester folder - older builds stay available via GitHub Releases if ever needed,
  they just aren't cluttering the handoff folder). Rename the GitHub Release assets to match the
  same versioned pattern before/after `gh release create` (`gh release upload`/`delete-asset` if
  fixing an already-published release). That folder also holds a brief `README.txt` for testers
  (what SPANDEX is, install steps per platform - keep the referenced filenames in sync with
  whatever's actually in the folder, quick-start usage, what feedback is useful) - update it when
  install steps, major features, or the version changes.

## Architecture

```
Source/
  Audio/       StretchAudioSource (Re-Pitch/Warp/Paulstretch playback engines), EffectsChain
               (Reverb/GranularDelay/FreqShifter/SmudgeProcessor/Drive+Compression bus glue/EQ),
               AudioEngine (owns the graph), AudioFileLoader
  Export/      offline render pipeline (ExportEngine)
  UI/          waveform, transport/loop/pitch/speed/warp controls, FX/EQ panels,
               AppLookAndFeel + Theme.h (runtime-switchable palettes: Default/Matrix/Amber
               Terminal), IconButton, LoopControls
  PluginProcessor/Editor   VST3 wrapper around AudioEngine/MainComponent
  MainComponent            top-level layout shared by Standalone and VST3
  SelfTest                 --selftest CLI harness
installer/     Inno Setup script (Windows)
.github/workflows/  macOS CI build
```

Rendering has three paths in `StretchAudioSource`: **Re-Pitch** (linked, bypasses Rubber Band,
direct resampled playback), **Warp modes** (Rubber Band streaming, a different transient/window/
formant preset per mode), **Paulstretch** (a separate hand-rolled FFT phase-randomizing
stretcher, for extreme slow-motion). The loop region's crossfade is exact per-sample in Re-Pitch,
approximated at chunk/block granularity in the Warp path.

## Working conventions established this project

- **Don't attempt UI-interaction verification via synthetic input automation** (SendInput/
  mouse_event coordinate-based clicking) - proved unreliable in practice (even a wide, easy
  target like a slider didn't respond to a well-targeted synthetic click) and wastes turns
  chasing a false signal. Verify via the self-test harness, code review, and static layout math
  instead; ask the user to click through and confirm anything that genuinely needs interactive
  testing.
- **Don't steal window/mouse focus from the user's actively-running DAW** (they test live in
  Ableton, often mid-session on unrelated work) - check `tasklist` before any window-capture or
  focus-grabbing attempt, and prefer `PrintWindow`-based capture (doesn't require focus) over
  `SetForegroundWindow`+screenshot for passive visual checks.
- The user gives specific, technically literate feedback (e.g. "should be more like turning up
  dry/wet on an OTT preset," "can use repitch mode" for delay-time changes) - match the DSP
  reasoning to what they're describing, not just a surface-level knob tweak. When feedback says a
  control "doesn't do much," treat it as a real bug report and dig for the actual detector/
  architecture flaw (see the Compression rework: an instantaneous-peak detector was diluting
  achieved gain reduction far below what the ratio implied - fixed with an RMS-style detector +
  upward compression, verified via a before/after dynamic-range-ratio self-test) rather than just
  turning numbers up.
- **Keep `README.md` and `CHANGELOG.md` current with big changes** - new/reworked features, DSP
  behavior changes, and UI redesigns get a `CHANGELOG.md` entry and a `README.md` update
  (Features list, usage instructions, "How it works" section) in the same pass as the code change,
  not as a separate later cleanup. When a change affects what's on screen (new controls, a
  redesigned panel, a new tab/section), the `docs/screenshot-*.png` files it's paired with in the
  README need refreshing too - ask the user for updated screenshots rather than faking it with an
  empty/unloaded-file capture, since the existing ones show a real loaded track and populated
  controls.
