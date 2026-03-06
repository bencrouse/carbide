# carbide

`carbide` is a minimal, macro-driven kick drum synthesizer plugin for Logic Pro.
It is designed for fast iteration: load, tweak a few controls, and get production-ready synthesized kicks with character.

This is not a general drum machine or modular synth. The project intentionally focuses on one job: powerful kick synthesis with low UI complexity and strong default sound.

## Highlights
- AUv2 instrument + Standalone app
- Built for macOS 15+ (Sequoia), Apple Silicon
- Mono one-shot voice (MIDI note-on retrigger model)
- 9 macro controls:
  - `pitch`, `decay`, `punch`, `click`, `material`, `drive`, `tone`, `sub`, `output`
- Character mode toggle: `Soft / Neutral / Hard`
- Internal 2x render path with nonlinear stages and output safety clipping
- Minimal fixed-size dark UI with preset selector, mode selector, output meter, and waveform preview
- 25 factory presets organized by category:
  - `Core`, `Click`, `Weight`, `Edge`, `Soft`

## Sound Design Direction
`carbide` is tuned for modern synthesized kick design:
- fast transient control
- controllable harmonic density
- mix-aware low-end reinforcement
- mode-based character shaping without adding lots of controls

The project currently favors electronic/experimental kick aesthetics, but includes softer voicing via mode + preset mapping.

## Architecture
High-level signal flow:
1. MIDI note-on retriggers a mono voice
2. Core oscillator body with pitch envelope + amp envelope
3. Transient layer (noise/tone blend)
4. Material morph (harmonic shaping)
5. Drive stage
6. Tone shaping
7. Mode shaping (`Soft/Neutral/Hard`)
8. Safety output stage (soft clip + ceiling)

Implementation choices:
- JUCE CMake project (AU + Standalone only)
- APVTS for host automation/state
- Sample-by-sample parameter smoothing for click-safe automation
- Choice parameter for mode, with explicit preset default mode assignment
- Internal test binary for deterministic DSP sanity checks

## Preset System
Factory presets are declared in code and loaded at runtime.
- Presets include:
  - name
  - 9 macro values
  - default mode
- Categories are encoded in preset names (`Category / Name`) for fast browsing in the compact dropdown.

## Build
Prerequisites:
- Xcode 26+
- CMake 3.24+
- JUCE checkout on disk

Configure:
```bash
cmake -S . -B build -DJUCE_DIR=/absolute/path/to/JUCE
```

Build:
```bash
cmake --build build --config Debug
```

## Makefile Workflow
For local iteration, you can use the provided `Makefile`:

```bash
make configure JUCE_DIR=/absolute/path/to/JUCE
make build
make test
make reload-au
```

Key targets:
- `make configure` - CMake configure
- `make build` - build AU + Standalone + tests
- `make test` - run DSP tests
- `make install-au` - copy AU to user Components folder
- `make validate-au` - run `auval`
- `make reload-au` - install + validate in one command

Portable overrides:
- `JUCE_DIR=/path/to/JUCE`
- `CONFIG=Debug` or `CONFIG=Release`
- `AU_INSTALL_DIR=/custom/components/path`

## Install in Logic Pro
Copy built AU component:
```bash
cp -R /absolute/path/to/carbide/build/carbide_artefacts/AU/carbide.component ~/Library/Audio/Plug-Ins/Components/
```

Validate:
```bash
auval -v aumu CKBD CBDE
```

If Logic is running, restart it after updating the component.

## Testing
Build includes `carbide_dsp_tests` (when `CARBIDE_BUILD_TESTS=ON`, default).

Run:
```bash
./build/carbide_dsp_tests_artefacts/carbide_dsp_tests
```

Current automated checks:
- finite sample safety (no NaN/Inf)
- output ceiling sanity
- monotonic macro sanity (decay/punch behavior)
- retrigger consistency
- mode separation sanity
- factory preset sanity (all presets render, level checks)

## Project Scope and Non-Goals
In scope:
- AUv2 + Standalone workflow
- focused kick synthesis
- minimal UX with broad macros

Out of scope (current):
- VST3 distribution
- Intel macOS targets
- notarization/signing/distribution pipeline
- advanced modulation matrix or deep synth editing UI

## Roadmap Ideas
- category filter UI (instead of name-prefix browsing only)
- optional eco/high quality mode switch
- richer preset metadata and audition tools
- mode-specific transient models for stronger sonic contrast

## Documentation
- [CONTRIBUTING](./CONTRIBUTING.md)
- [ARCHITECTURE](./ARCHITECTURE.md)
- [Tuning Guide](./docs/TUNING_GUIDE.md)
- [Logic Dev Loop](./docs/LOGIC_DEV_LOOP.md)
- [Changelog](./CHANGELOG.md)
