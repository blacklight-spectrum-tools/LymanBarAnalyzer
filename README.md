# Bar Spectrum Analyzer (VST3)

*[Leer en español](README.es.md)*

A reference spectrum analyzer that shows energy per frequency band as
**bars** (like a graphic EQ), built to sit right **before a compressor** so
you can see which bands need compression and where to set the threshold.

It never touches the audio: it is a meter only, fully transparent to the
signal.

## Compatibility

VST3 plugin for **Windows**, compatible with any DAW that loads VST3
plugins: FL Studio, Ableton Live, Cubase, Studio One, Reaper, Bitwig,
Cakewalk, Reason 12+ and others.

Tested so far in FL Studio. Pro Tools (AAX) and macOS (AU / VST3) builds
are planned.

## Features

**Spectrum tab**
- 28 bars, logarithmically spaced from 20 Hz to 20 kHz.
- Compression reference line: bars above it light up and show by how many
  dB they exceed it (e.g. `+3.2`), the number you need for the threshold.
- Peak hold on every bar.
- Tilt (0 / 3 / 4.5 dB per octave) to judge tonal balance. Use 0 when
  setting a compressor threshold.
- L/R level meter with Peak, RMS and short-term LUFS (ITU-R BS.1770)
  readouts, plus a latching CLIP light (click to reset).
- Sum or M/S view: in M/S the Side channel is drawn in cyan over each bar,
  so you can see which frequencies are wide and which sit in the center.

**Phase tab**
- Goniometer showing the stereo image, with Auto scale (the shape fills the
  circle) or Level scale (distance from the center is the real level,
  the edge is 0 dBFS).
- Phase correlation meter from -1 to +1.
- Warning light: green (mono-safe), yellow (very wide, check in mono),
  red (parts of the mix cancel out when summed to mono).
- Minimum / maximum correlation reached while playing, with reset.
- Low end check: correlation of everything below 120 Hz, with its own
  light (Mono / Slightly wide / Stereo bass). Tells you whether the width
  is in the bass, where it hurts, or only in the highs.
- L/R balance meter: shows if the mix leans to one side over time.

The window is resizable and remembers its size and last open tab.

## Requirements

- **Git**
- **CMake** 3.15 or newer: https://cmake.org/download/
- **Visual Studio 2022** (or its Build Tools) with the
  "Desktop development with C++" workload.

JUCE is included as a git submodule, so there is no need to download it
separately.

## Getting the code

Clone the repository **with its submodules** so JUCE is downloaded too:

```
git clone --recurse-submodules https://github.com/velivaneski/BarSpectrumAnalyzer.git
```

If you already cloned without that option and the `JUCE/` folder is empty,
run this inside the project:

```
git submodule update --init
```

## Building

From the project folder, in PowerShell or Command Prompt:

```
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

The first build takes several minutes because it compiles all of JUCE.
When it finishes, the plugin is at:

```
build/BarSpectrumAnalyzer_artefacts/Release/VST3/Bar Spectrum Analyzer Dev.vst3
```

The current development build is named **"Bar Spectrum Analyzer Dev"** so
it can live next to version 1.0 without replacing it. Version 1.0 is
available under the `v1.0` git tag.

## Installing

The build tries to copy the plugin automatically to the standard Windows
VST3 folder:

```
C:\Program Files\Common Files\VST3\
```

That folder requires administrator rights. If the build ends with a
`Permission denied` error at that step, the plugin **was** built; just copy
the `.vst3` folder to that path by hand (Windows will ask for administrator
confirmation), or run the build from a terminal opened as administrator.

## Using it

1. Rescan plugins in your DAW. In FL Studio: **Options → Manage Plugins →
   Find plugins**.
2. Insert **"Bar Spectrum Analyzer Dev"** on the track or bus you want to
   measure, **right before** the slot where your compressor goes.
3. Move the **compression reference** slider to the level where you want
   the compressor to start working. Highlighted bars are above it, and the
   number on top tells you by how much.
4. Open the **Phase** tab to check stereo width and mono compatibility.
