# Architecture

`carbide` is a focused kick synthesizer with a mono one-shot voice and macro control surface.

## High-Level Components
- Plugin host integration and state: `AudioProcessorValueTreeState` in `PluginProcessor`.
- DSP voice engine: `KickSynthVoice`, with trigger-latched Soft, Neutral, and Hard render paths.
- UI: compact macro editor with preset + mode selectors, waveform preview, and meter.
- Preset catalog: in-code preset table with per-preset mode defaults.
- Test harness: standalone DSP sanity executable.

## Signal Flow
1. MIDI note-on sets the equal-tempered fundamental pitch and retriggers the mono voice.
2. The selected character engine is latched for the complete hit.
3. Engine-specific pitch and amplitude envelopes drive the body.
4. Engine-specific transient generation adds filtered excitation, tonal click, or high-passed noise.
5. `material` controls the engine's harmonic topology.
6. Engine-specific drive and damping establish character.
7. Tone shaping tilts low/high balance.
8. Damping + safety clipper constrain harshness and output ceiling.

## Character Engines
- `Soft`: sine-led body with restrained pitch sweep, low-passed noise excitation, an independent low tonal transient, and gentle asymmetric saturation.
- `Neutral`: compatibility path for the original carbide oscillator, transient, drive, and damping topology.
- `Hard`: phase-distorted multi-harmonic body, independent high-frequency tonal transient, high-passed noise, and aggressive asymmetric saturation.

All paths render internally at 2x and share a continuous 33-tap low-pass decimator. Its eight-sample latency is reported to the host and its history is preserved across retriggers and mode changes.

## Parameter Model
Continuous macros:
- `pitch`, `decay`, `punch`, `click`, `material`, `drive`, `tone`, `sub`, `output`
- `pitch` applies a continuous +/-12-semitone transpose to the incoming MIDI note.

Discrete character parameter:
- `mode` (`Soft`, `Neutral`, `Hard`)

Design constraints:
- Stable parameter IDs for host automation compatibility.
- Sample-by-sample parameter smoothing on continuous macros.
- Mode is discrete, read as a choice index from APVTS, and latched on note-on. Automation affects the next hit.

## Preset Model
Each preset includes:
- display name (`Category / Preset`)
- 9 macro values
- default mode

Current categories:
- `Core`, `Click`, `Weight`, `Edge`, `Soft`

## Build Targets
- `carbide_AU` (AUv2 component)
- `carbide_Standalone` (app)
- `carbide_dsp_tests` (DSP regression checks)

The DSP executable is also registered with CTest.
