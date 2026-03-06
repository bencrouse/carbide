# Architecture

`carbide` is a focused kick synthesizer with a mono one-shot voice and macro control surface.

## High-Level Components
- Plugin host integration and state: `AudioProcessorValueTreeState` in `PluginProcessor`.
- DSP voice engine: `KickSynthVoice`.
- UI: compact macro editor with preset + mode selectors, waveform preview, and meter.
- Preset catalog: in-code preset table with per-preset mode defaults.
- Test harness: standalone DSP sanity executable.

## Signal Flow
1. MIDI note-on retriggers mono voice.
2. Pitch envelope drives oscillator sweep.
3. Amp envelope shapes body decay.
4. Transient layer adds attack click/noise.
5. Material morph blends sine body with harmonic content.
6. Drive stage adds controlled nonlinearity.
7. Tone shaping tilts low/high balance.
8. Mode shaping (`Soft/Neutral/Hard`) applies character-dependent transient/harmonic treatment.
9. Damping + safety clipper constrain harshness and output ceiling.

## Parameter Model
Continuous macros:
- `pitch`, `decay`, `punch`, `click`, `material`, `drive`, `tone`, `sub`, `output`

Discrete character parameter:
- `mode` (`Soft`, `Neutral`, `Hard`)

Design constraints:
- Stable parameter IDs for host automation compatibility.
- Sample-by-sample parameter smoothing on continuous macros.
- Mode is discrete and read as a choice index from APVTS.

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

