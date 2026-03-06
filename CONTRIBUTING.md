# Contributing

Thanks for contributing to `carbide`.

## Project Principles
- Keep UI complexity low.
- Prefer sound quality and musical behavior over feature count.
- Preserve parameter IDs and automation compatibility.
- Avoid regressions in preset behavior unless explicitly intended.

## Development Setup
Prerequisites:
- macOS 15+ (Apple Silicon)
- Xcode 26+
- CMake 3.24+
- JUCE checkout

Typical workflow:
```bash
make test JUCE_DIR=/absolute/path/to/JUCE
make reload-au JUCE_DIR=/absolute/path/to/JUCE
```

## Coding Guidelines
- C++20.
- Keep DSP changes measurable and testable.
- Keep comments concise and focused on non-obvious logic.
- Do not introduce breaking parameter ID changes.

## Testing Requirements
Before opening a PR:
- Run `make test`.
- If plugin behavior changed, run `make reload-au` and validate in Logic.
- Confirm `auval` passes.

Expected DSP tests:
- kick render smoke
- monotonicity checks
- retrigger consistency
- mode separation
- factory preset sanity

## PR Checklist
- Describe user-facing behavior changes.
- Note any preset changes clearly.
- Mention whether `Neutral` mode behavior changed.
- Include testing commands and outcomes.

