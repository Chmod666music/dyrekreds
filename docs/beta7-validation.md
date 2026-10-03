# Beta.7 validation checklist

The automated build now covers the bundle-level conditions below. Complete the
host checks on a signed/notarised tag build before publishing beta.7.

1. On an Apple-Silicon Mac running Sonoma 14.7.6 or later, unpack the macOS
   archive and install the complete `Dyrekreds.vst3` directory in
   `~/Library/Audio/Plug-Ins/VST3`, the complete `Dyrekreds.component` directory
   in `~/Library/Audio/Plug-Ins/Components`, and the standalone `.app`.
2. Verify `codesign --verify --deep --strict` and `spctl --assess --type
   execute` on each installed bundle. Do not remove quarantine with
   `xattr` during this check: the purpose is to exercise the real Gatekeeper
   path.
3. In Ableton Live 12.4.6, rescan plug-ins. Confirm that the VST3 is listed,
   the AU opens without a Gatekeeper alert, and the standalone opens normally.
4. Resize the AU editor at its lower-right corner at default, minimum and
   maximum sizes. The keyboard must leave the host resize region uncovered.
5. Load a clean WAV at native pitch and at an octave up/down in Sample,
   Granular and Freeze modes. Record the exact preset and settings if noise is
   reproducible; a report without that signal path is not enough to change the
   audio engine safely.

## Current sample-noise diagnosis

The ordinary Sample playback path uses linear interpolation. It is stable and
bounded, but it has no anti-alias filter when a sample is transposed upward.
That can produce aliases perceived as high-frequency noise. Granular/Freeze
playback additionally changes the source through grain windows and overlap.
No sound-engine change is included in beta.7 until the report identifies which
mode, sample rate, pitch and preset reproduce the issue. The existing DSP tests
cover finite/bounded output, imported WAV playback and all three sample modes;
they do not establish subjective sound quality for an unspecified report.
