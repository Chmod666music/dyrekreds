# Dyrekreds

[![Build](https://github.com/Chmod666music/dyrekreds/actions/workflows/build.yml/badge.svg?branch=dyrekreds)](https://github.com/Chmod666music/dyrekreds/actions/workflows/build.yml)

**Dyrekreds** is a free and open-source synthesizer plugin maintained by
**Fuimadane**. It is derived from
[Galdr](https://github.com/andreademurtas/galdr), originally created by
Andrea De Murtas.

Dyrekreds is currently available as VST3, CLAP, AU on macOS, and a standalone
application. Development began on 21 August 2026.

## Project status

Dyrekreds 0.9 Beta 15 is a public beta. The plugin is built, tested and
validated automatically on Linux, Windows and macOS, but additional testing
across DAWs, hardware and real-world projects is welcome before the 1.0
release.

Dyrekreds is derived from Galdr, while establishing its own product identity,
plugin IDs, preset directory and expanding sound engine. Dyrekreds and Galdr
can be installed side by side.

Beta 15 replaces the withdrawn Beta 14 release. Beta 14 accidentally omitted
parts of the newer sample workflow; please use Beta 15 instead. Beta 15 restores
the complete sample experience, including MP3 import, waveform zoom and
pan/scroll, precise Sample Start and End controls, time stretching, granular
features and saved editor size.

Beta 15 is the first public release with the complete macOS release path
verified in CI: universal Intel and Apple Silicon bundles, VST3 and AU
validation, Developer ID signing, Apple notarisation and stapling. macOS builds
target macOS 10.15 or newer on Intel and macOS 11.0 or newer on Apple Silicon.
Release packages contain plug-ins and documentation only — no demo MP3 files.

## Features

- 12-voice engine with poly, mono and legato modes
- Two band-limited oscillators with unison, detune and stereo spread
- FM, hard sync, sub oscillator and white or pink noise
- Morphing wavetable mode with 32 mip-mapped frames
- Multimode filter with drive, envelope and keytracking
- Eight-slot modulation matrix and a third envelope
- Tempo-synchronised arpeggiator, LFOs, tremolo and delay
- MPE-friendly pitch bend and pressure
- Scala `.scl` microtuning
- WAV, AIFF and MP3 sample import with pitch-tracked playback
- Sample browser remembers the last folder used
- Pitch-aware sample time stretching with independent Speed and Pitch controls (-24 to +24 semitones)
- Smooth, Transient and Percussive stretch modes
- Draggable Sample Start/End controls
- Granular and freeze modes with five grain-motion patterns
- Interactive sample waveform with realtime playhead, scrubbing, zoom up to 64x, panning and scrolling
- Improved Sample Start, Sample End and Granular Position controls
- Project restoration of loaded samples and interface size; VST3 remembers its resized window
- Distortion, bitcrusher, ring modulation, chorus, delay and reverb
- Granular “Blizzard” texture layer
- Preset browser with 27 factory presets inherited from Galdr
- MIDI learn, undo and redo
- Sample waveform, spectrum analyser and on-screen keyboard
- Resizable dark-fantasy interface

## Downloads and testing

Download Beta 15 for Linux, Windows and macOS:

https://github.com/Chmod666music/dyrekreds/releases/tag/v0.9.0-beta.15

Packages: `Dyrekreds-v0.9.0-beta.15-Linux.tar.gz`,
`Dyrekreds-v0.9.0-beta.15-Windows.zip` and
`Dyrekreds-v0.9.0-beta.15-macOS.tar.gz`. The macOS package contains signed and
notarised VST3, CLAP, AU and standalone builds. These packages do not include
demo MP3 files; sample import still supports MP3.

Testing feedback is also welcome on the Dyrekreds Discord:

https://discord.gg/Pfvkux2bG

## Building

Requirements:

- Git
- CMake 3.22 or newer
- A C++20 compiler
- JUCE’s platform-specific build dependencies

To build the exact Beta 15 release, clone its tag and submodules:

```sh
git clone --branch v0.9.0-beta.15 --recurse-submodules https://github.com/Chmod666music/dyrekreds.git
cd dyrekreds
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

On Linux, install the JUCE dependencies listed in
`.github/workflows/build.yml` before configuring the build.

Build artifacts are written to `build/Dyrekreds_artefacts/Release/`:

| Artifact | Install location |
|---|---|
| `VST3/Dyrekreds.vst3` | Windows: `C:\Program Files\Common Files\VST3` · macOS: `~/Library/Audio/Plug-Ins/VST3` · Linux: `~/.vst3` |
| `CLAP/Dyrekreds.clap` | Windows: `C:\Program Files\Common Files\CLAP` · macOS: `~/Library/Audio/Plug-Ins/CLAP` · Linux: `~/.clap` |
| `AU/Dyrekreds.component` | macOS only: `~/Library/Audio/Plug-Ins/Components` |
| `Standalone/Dyrekreds` | Run directly without plugin installation |

## Testing

Configure and build the project, then run the test executable.

On Linux:

```sh
xvfb-run --auto-servernum ./build/DyrekredsTests_artefacts/Release/DyrekredsTests
```

## Origin and attribution

Dyrekreds is based on Galdr:

- Original author: Andrea De Murtas
- Original project: https://github.com/andreademurtas/galdr
- Dyrekreds modifications and maintenance: Jon Skarin / Fuimadane
- Dyrekreds development began: 21 August 2026

The original copyright notices, project history and third-party attributions are
preserved. See `NOTICE.txt` for details.

## License

The Galdr-derived source code and Dyrekreds modifications are distributed under
the **GNU General Public License, version 3 or later**. See
[LICENSE](LICENSE).

The project also includes third-party components under their respective
licenses:

- JUCE framework — GNU Affero General Public License v3
- VST3 SDK interfaces — MIT License
- CLAP and clap-juce-extensions — MIT License
- UnifrakturMaguntia and IM Fell English fonts — SIL Open Font License 1.1

Binary distributions must include the applicable license texts, `NOTICE.txt`
and access to the complete corresponding source code for the exact released
version.

*VST is a registered trademark of Steinberg Media Technologies GmbH.*
