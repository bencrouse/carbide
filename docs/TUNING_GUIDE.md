# Preset Tuning Guide

This guide describes a consistent method for tuning `carbide` presets.

## Goals
- Preserve category intent.
- Keep level switching predictable.
- Avoid accidental collapse into one tonal profile.

## Category Targets
- `Core`: balanced, mix-ready, general purpose.
- `Click`: high transient definition, short body.
- `Weight`: stronger low-end support, less top-end bite.
- `Edge`: deliberate aggression and harmonic density.
- `Soft`: rounded transient and reduced harshness.

## Tuning Workflow
1. Start from the intended mode (`Soft`, `Neutral`, `Hard`).
2. Set `decay` and `punch` first to establish envelope identity.
3. Set `click` for transient intensity.
4. Shape density with `material` and `drive`.
5. Use `tone` to place brightness; avoid compensating with only `output`.
6. Set `sub` for low-end role.
7. Finalize `output` to keep preset switching controlled.

## Consistency Rules
- Avoid many presets clustering in the same `click/material/drive` zone.
- Keep at least one clearly low-brightness option per category where musically valid.
- Keep output conservative; rely on channel gain for final loudness staging.
- Do not use mode as a cosmetic flag; mode should match sonic intent.

## Validation
After edits:
```bash
make test JUCE_DIR=/absolute/path/to/JUCE
```

In Logic:
1. A/B adjacent presets in each category.
2. Confirm category identity remains obvious at equal channel gain.
3. Check that `Soft` presets remain useful without external EQ.

