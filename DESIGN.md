# Design

## Source of truth
- Status: Active
- Last refreshed: 2026-08-12
- Primary product surfaces: YUP Standalone, VST3, AUv2 editor

## Brand
- Personality: hostile diagnostic terminal; precise rather than decorative.
- Trust signals: visible values, stable parameter order, honest input/output meters.
- Avoid: color accents, gradients, glow, rounded cards, fake telemetry, flashing.

## Product goals
- Turn input into moving metallic comb structures while remaining bounded and automatable.
- Make all seven controls visible at once.
- Preserve hosted silence and isolate Standalone audition state.
- Non-goals: conventional lush chorus, MIDI instrument behavior, photo-real hardware.

## Personas and jobs
- Job: reach unstable-sounding but repeatable comb textures quickly during automation.

## Information architecture
- Header: `TEETHMATRIX` and one-line warning.
- Standalone-only runtime strip: audition source and quantized input/output meters.
- Main surface: Tune, Feedback, Damping, Dispersion, Motion, Drive, Mix in one row.

## Design principles
- Monochrome means state through inversion, density, and outline—not hue.
- Pixel character comes from integer geometry, grid, scanlines, and stepped meters.
- Host automation semantics take precedence over decorative control replacement.

## Visual language
- Color tokens: ink `#050505`, low `#2A2A2A`, mid `#8A8A86`, paper `#F2F2F0`.
- Typography: YUP native fallback; uppercase labels and stable numeric value labels.
- Layout: 640x360 logical canvas on a 4px base grid, with 8px group spacing, square edges, and no elevation.
- Motion: 30 Hz decaying meters only; no stochastic or full-panel animation.

## Components
- ParameterGridEditor with square pixel sliders, host gestures, and a clockwise 16-step perimeter indicator.
- Seven-column single-row parameter grid.
- Standalone-only audition buttons plus 24-step rectangular meters.

## Accessibility
- White or muted-gray text on black with high contrast.
- No flashing; meter motion is functional and low amplitude.
- Values remain textual; controls retain native YUP interaction behavior.

## Responsive behavior
- Preferred size: 640x360, resizable with aspect ratio preserved.
- Seven controls remain in one row; control diameter shrinks before labels disappear.

## Interaction states
- Hosted silence stays silent.
- Standalone audition is runtime-only and is not serialized.
- Disabled audition produces silence; meters show processor atomics only.

## Content voice
- Short uppercase parameter names and terse system-warning copy.
- Never imply random or unstable state where rendering is deterministic.

## Implementation constraints
- C++20 and YUP; no new runtime dependency or external asset.
- Integer-aligned `fillRect` rendering; no ellipse, gradient, shadow, or rounded meter.
- Audio thread performs no allocation, locks, I/O, logging, or UI calls.
- App/plugin ID `jp.ehl.teethmatrix`; vendor `ehl_`; AU `TtMx` / `EHL1`.

## Verification
- Seven stable parameter IDs and state magic `TTM1`.
- Deterministic impulse response, tune arrival, tail, dispersion/motion, extremes, denormals.
- Hosted silence/state bridge and Standalone audition/meter isolation.
- Standalone/VST3/AU build and staged artifacts on macOS; Standalone/VST3 on Windows CI.

## Open questions
- [ ] Evaluate the two polarity regions in multiple hosts after the first public listening round.
