# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: The Press (VST3/AU plugin)

JUCE 8 audio plugin. Chain: **Apex Limiter → Even Drive → Velvet Clip** (order swappable). All three DSP algorithms are exact ports of airwindows open-source code (MIT). Plugin name in DAW: "The Press". Manufacturer code: `Zeus / TrPs`.

## Build

JUCE is not a submodule — it must be passed at configure time:

```powershell
# Windows (PowerShell, from repo root)
cmake -B build -DJUCE_PATH="C:\Users\zeuse\Downloads\juce-8.0.12-windows\JUCE"
cmake --build build --config Release
# VST3 output: build\ThePress_artefacts\Release\VST3\ThePress.vst3
```

```bash
# Linux/Mac
cmake -B build -DJUCE_PATH=/path/to/JUCE
cmake --build build --config Release
```

There are no tests. No linter configured.

### Knob film strip (required before build)

`Source/KnobStripData.h` is a generated file (not committed) that embeds the knob PNG as a C++ array. It must be regenerated whenever `Assets/knob_strip.png` changes:

```powershell
# Windows
.\tools\png_to_header.ps1 -Src "Assets\knob_strip.png" -Dst "Source\KnobStripData.h"
```

```bash
# Linux/Mac
python3 tools/png_to_header.py Assets/knob_strip.png Source/KnobStripData.h
```

The PNG is 128×128 px per frame, 200 frames (total: 128×25600). This bypasses `juce_add_binary_data` (juceaide crashes on this file size).

### Two-worktree setup (Windows)

```
C:\Users\zeuse\Claude-VST-2         → branch: feat/knob-assets     (experimental)
C:\Users\zeuse\Claude-VST-2-stable  → branch: claude/vst3-juce-airwindows-UV4JH  (stable)
```

All production commits go to `claude/vst3-juce-airwindows-UV4JH`.

## Architecture

### DSP flow (`PluginProcessor.cpp`)

```
processBlock()
  ├── measure input peaks → meterInL/R (atomic)
  ├── processChain at native sample rate
  └── measure output peaks → meterOutL/R (atomic)

processChain(L, R, numSamples, actualSR)
  ├── read all APVTS params once (never inside per-sample loop)
  ├── [APEX LIMITER or EVEN DRIVE first, depending on swapOrder]
  ├── [the other one second]
  ├── VELVET CLIP (ClipSoftly) — always last
  └── TP clamp at 0.9549925859 (only if hardClip==true)
```

### DSP algorithms (exact airwindows ports)

| Stage | Source algorithm | Key state |
|---|---|---|
| Apex Limiter | Acceleration2 | `sL[34]`, `sR[34]`, `biquadA[11]`, `biquadB[11]`, `m1/m2` |
| Even Drive | Spiral2 | `iirSampleAL/BL`, `prevSampleL_spi`, `flip_spi` |
| Velvet Clip | ClipSoftly | `lastSampleL/R_cs`, `intermediateL/R[17]` |

All three use TPDF dither noise via XORSHIFT PRNGs (`fpdL_xxx`, `fpdR_xxx`). **Do not change the noise values or array sizes** — they match the original algorithms exactly. `intermediateL/R[17]` is intentionally 17 (not 16) so index `csSpacing` (max 16) is always in bounds.

### Parameters (APVTS, version 1)

All parameters use `ParameterID("name", 1)` — version number must stay at 1 to avoid breaking saved presets.

| ID | Type | Default | Notes |
|---|---|---|---|
| `apexLimit` | Float 0–1 | 0.32 | Displayed as % |
| `apexDryWet` | Float 0–1 | 1.0 | Wet/Dry mix |
| `bypassApex` | Bool | false | |
| `evenInput` | Float -24–24 dB | 0 | |
| `evenHighpass` | Float 0–1 | 0.5 | Cubed internally |
| `evenPresence` | Float 0–1 | 0.5 | |
| `evenOutput` | Float -24–24 dB | 0 | |
| `evenDryWet` | Float 0–1 | 1.0 | |
| `bypassEven` | Bool | false | |
| `doubleEffect` | Bool | false | Runs Even Drive twice |
| `bypassVelvet` | Bool | false | |
| `hardClip` | Bool | true | Enables TP clamp at -0.4 dBTP |
| `swapOrder` | Bool | false | Apex first (false) or Even first (true) |

### UI (`PluginEditor.cpp`)

`ThePressLookAndFeel` (defined at top of PluginEditor.cpp, before the anonymous namespace) renders knobs from the film strip PNG and draws an aluminum-style linear slider thumb. It is owned by `AWCascadeEditor` as `std::unique_ptr<ThePressLookAndFeel> laf` — `setLookAndFeel(laf.get())` in constructor, `setLookAndFeel(nullptr)` in destructor.

**ComboBox attachment pattern**: `oversampleAttach` is a `unique_ptr` initialized in the constructor body *after* `addItem()` calls. All other attachments are value-type members initialized in the member initializer list. This difference is intentional — the ComboBox needs items added before `sendInitialUpdate()` fires.

`timerCallback()` runs at 30 Hz and:
- Syncs bypass/TP button text (handles automation and state restore)
- Decays `dispInL/R`, `dispOutL/R` for the VU meters using peak from `processor.meterInL/R/OutL/R`

Layout constants are in the anonymous `namespace` block: `kW=480`, `kH=578`, all section heights derived from `kSecH` and `kClipH`.

### Thread safety

Meter atomics (`meterInL/R`, `meterOutL/R`) are `std::atomic<float>` — written on the audio thread in `processBlock`, read on the UI thread in `timerCallback`. All other processor state is audio-thread-only. Never access plugin state from the UI thread except through APVTS.

## Known design decisions

- **ClipSoftly ceiling**: hardcoded `0.9549925859` (= sin(π/2 - small offset) × constant). This is the exact airwindows value — do not replace with a dB-converted parameter.
- **TP clamp redundancy**: ClipSoftly already soft-clips to ~0.9549, so the TP clamp (same value) only affects inter-sample reconstruction peaks at the oversampled rate. At OS=off it is a belt-and-suspenders sample-level clamp.
- **`tpOS` was tried and removed**: a separate 4x Oversampling stage for TP caused IIR downsample filter ringing *above* the ceiling — worse than no TP at all. The clamp inside `processChain` is the correct location.
- **Oversampling was removed entirely**: it was causing more problems than it solved (inter-sample peaks from the IIR downsample filter exceeded the ceiling). The plugin runs at native sample rate only.
- **`juce_add_binary_data` bypassed**: juceaide crashes on the 3.6 MB knob strip PNG. The `tools/png_to_header.ps1` / `tools/png_to_header.py` scripts generate `Source/KnobStripData.h` directly.
