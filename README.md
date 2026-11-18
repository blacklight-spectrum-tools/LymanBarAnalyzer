# Bar Spectrum Analyzer (VST3)

*[Leer en español](README.es.md)*

A reference spectrum analyzer in the spirit of SPAN, but showing energy per
frequency band as **bars** (like a graphic EQ). It is meant to sit right
**before a compressor** so you can decide more precisely which bands need
compression and where to set the threshold.

- 28 bars, logarithmically spaced from 20 Hz to 20 kHz.
- Fast attack / slow release metering, behaving like a real VU meter.
- Automatable "Reference (dB)" slider: draws a horizontal line and paints
  any band above it orange, so you can see at a glance which frequencies
  need compression and which don't.
- It never touches the audio: it is a meter only, fully transparent to the
  signal.

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
build/BarSpectrumAnalyzer_artefacts/Release/VST3/Bar Spectrum Analyzer.vst3
```

## Installing

The build tries to copy the plugin automatically to the standard Windows
VST3 folder:

```
C:\Program Files\Common Files\VST3\
```

That folder requires administrator rights. If the build ends with a
`Permission denied` error at that step, the plugin **was** built; just copy
the `Bar Spectrum Analyzer.vst3` folder to that path by hand (Windows will
ask for administrator confirmation), or run the build from a terminal
opened as administrator.

## Using it in FL Studio

1. Open FL Studio → **Options → Manage Plugins**.
2. Click **Find plugins** so it gets detected.
3. It shows up as **"Bar Spectrum Analyzer"** in the effects list.
4. Insert it on the mixer channel you want to measure, **right before** the
   slot where your compressor goes.
5. Move the **"Reference (dB)"** slider to the level where you want the
   compressor to start working. Bars in **orange** are above that
   threshold: those are the bands that need compression right now. Bars in
   **green** are below it.

> Note: the plugin interface is currently in Spanish (the slider reads
> "Referencia de compresion"). An English interface is planned.
