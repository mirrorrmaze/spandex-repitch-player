# Changelog

Notable changes to SPANDEX, newest first. Not every commit gets an entry -
this tracks user-facing behavior and feature changes, not internal
refactors or doc tweaks.

## 2026-08-14 (v0.2.1)

- **Fix**: FX tab knobs could get clipped or overflow their card at smaller
  window sizes - most visibly on Freq Shifter, whose mode dropdown ate
  vertical space its sibling cards didn't need. The FX panel now lays out
  every card at its natural, full-size dimensions and scrolls vertically
  instead of shrinking or hiding controls when the window's too short to
  show everything at once.

## 2026-08-14

- **Fix**: spacebar Play/Pause wasn't honoring the "From Start" toggle the
  way the Play/Pause button did - both now share one code path.
- **Add**: in-app update notification. On launch, SPANDEX quietly checks
  GitHub for a newer release; if one exists, the settings ("...") button
  tints and a menu item opens the release page. Check-only - never
  downloads or installs anything automatically.

## 2026-08-13

- **Add**: FX chain routing. A new strip at the bottom of the FX tab lets
  you drag EQ/Reverb/Delay/Shifter/Smudge/Lossy/Gain into any processing
  order. Input/Output trim stay fixed first/last.
- **Add**: sampler-style loop refinements - "From Start" (Play always
  seeks to the Sample Start marker first), "Link" (dragging Sample Start
  drags Loop Start along with it), and dragging the loop region's
  interior to shift the whole loop across the file. Loop In/Out are now
  bracket symbols and Loop Mode is a 3-way segmented control instead of a
  cramped single row.
- **Add**: draggable Sample Start/End trim markers and a draggable loop
  brace directly on the waveform (Ableton Sampler style), replacing
  button-only loop point setting.
- **Change**: the Gain card's Compression knob is now Clip - a
  frequency-domain hard clipper (per-bin magnitude capped to an Amount-
  controlled ceiling) for a harsher, more distorted character than
  broadband compression gave.
- **Change**: Lossy rebuilt as a spectral codec-artifact effect
  (Goodhertz Lossy style: magnitude quantization + phase jitter + a
  refresh-hold rate) instead of a time-domain bitcrusher.
- **Change**: swapped the Consolas monospace look for the platform's own
  modern system UI font (Segoe UI / Helvetica Neue).
- **Fix**: Warp-mode looping was stopping instead of repeating past
  several cycles; added a true source-domain crossfade at the loop wrap
  and a Loop Mode selector (Forward / Ping-Pong / Reverse).

## Earlier

- Initial release: repitch/warp playback (Re-Pitch, Warp modes, and
  Paulstretch for extreme slowdown), waveform view, loop in/out, FX chain
  (Reverb, Granular Delay, Frequency Shifter, Smudge, Lossy, Drive/
  Compression), 8-band parametric EQ with live spectrum analyzer, and
  WAV/AIFF/FLAC/MP3 export. Ships as both a Standalone app and a VST3
  plugin.
