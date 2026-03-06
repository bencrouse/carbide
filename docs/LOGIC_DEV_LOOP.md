# Logic Pro Development Loop

Use this loop for rapid local iteration when changing DSP/UI/presets.

## 1) Build and Run Tests
```bash
make test JUCE_DIR=/absolute/path/to/JUCE
```

## 2) Install and Validate AU
```bash
make reload-au JUCE_DIR=/absolute/path/to/JUCE
```

This runs:
- AU install to `~/Library/Audio/Plug-Ins/Components`
- `auval -v aumu CKBD CBDE`

## 3) Reload in Logic
- Quit Logic fully if already open.
- Re-open Logic and load `carbide` on a software instrument track.

## 4) Manual Checks
- Preset browsing works and updates sound.
- Mode toggle (`Soft/Neutral/Hard`) audibly changes character.
- Automation on macro controls is click-safe.
- Output level remains controlled at extreme settings.

## Common Issues
- Logic still using old plugin binary:
  - Ensure Logic is fully restarted after install.
- `cmake not found`:
  - Use Homebrew CMake or run with full path.
- `auval` failure after binary changes:
  - Re-run `make reload-au` and re-open Logic.

