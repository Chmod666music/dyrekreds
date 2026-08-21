# Dyrekreds

[![Build](https://github.com/Chmod666music/galdr/actions/workflows/build.yml/badge.svg?branch=dyrekreds)](https://github.com/Chmod666music/galdr/actions/workflows/build.yml)

**Dyrekreds** is a free and open-source synthesizer plugin maintained by
**Fuimadane**. It is derived from
[Galdr](https://github.com/andreademurtas/galdr), originally created by
Andrea De Murtas.

Dyrekreds is currently available as VST3, CLAP, AU on macOS, and a standalone
application. Development began on 21 August 2026.

## Project status

Dyrekreds is in early development. The initial version establishes a separate
product identity, manufacturer identity, plugin IDs and user preset directory,
allowing Dyrekreds and Galdr to be installed side by side.

The current sound engine, factory presets and much of the interface originate
from Galdr. Future Dyrekreds releases will introduce substantial interface,
sound-engine and workflow changes.

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
- Distortion, bitcrusher, ring modulation, chorus, delay and reverb
- Granular “Blizzard” texture layer
- Preset browser with 27 factory presets inherited from Galdr
- MIDI learn, undo and redo
- Oscilloscope, spectrum analyser and on-screen keyboard
- Resizable dark-fantasy interface

## Building

Requirements:

- Git
- CMake 3.22 or newer
- A C++20 compiler
- JUCE’s platform-specific build dependencies

Clone the Dyrekreds development branch and its submodules:

```sh
git clone --branch dyrekreds --recurse-submodules https://github.com/Chmod666music/galdr.git
cd galdr
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
