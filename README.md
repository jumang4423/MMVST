# MMVST

Two compact stereo audio effects for macOS and Windows:

- **MMC** — modulated 2 x 3-tap stereo chorus
- **MMD** — stereo-linked dynamics processor with peak/RMS detection and soft clipping

Both VST3 plug-ins expose normalized `0..1` parameters and support mono or
stereo processing. Matching SuperCollider/SuperDirt patches are included as
optional extras in [`supercollider/`](supercollider/).

## Parameters

MMC: `DEL DEP SPD MIX FB WID LP INP`

MMD: `ATK REL THRS MIX RAT GAIN RMS INP`

MMC quantises every control to 128 steps and uses 2048-sample circular delay
lines at its 44.1 kHz reference rate, three linearly interpolated taps per
channel, a one-pole filter inside the inverted feedback path, and a linear
dry/wet crossfade. Delay time is rate-corrected in hosts running above 44.1
kHz. Its performance defaults are `DEL=64/127`, `DEP=.3`, `SPD=0`, `MIX=1`,
`FB=1`, `WID=1`, `LP=1`, and `INP=1`.

The core smoke test includes a signed Q1.23 oracle vector in addition to
finite-output and feedback stability tests at 44.1–192 kHz.

## Build locally

Install CMake and clone JUCE 8, then configure each plug-in separately:

```sh
cmake -S plugins/MMC -B build/MMC -DJUCE_PATH=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build/MMC --config Release --target MMC_VST3

cmake -S plugins/MMD -B build/MMD -DJUCE_PATH=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build/MMD --config Release --target MMD_VST3
```

## Windows builds

Every push to `main` builds x86_64 Windows VST3 bundles. Download the
`MMVST-Windows-x64` artifact from the latest Actions run and copy the bundles
to:

```text
C:\Program Files\Common Files\VST3\
```

## SuperCollider

Copy the optional `.scd` files into a SuperDirt effects directory and load them
after SuperDirt has started. Add matching `pF` controls to the Tidal boot file.

## License

MIT
