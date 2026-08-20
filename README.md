# TeethMatrix


## Identity and formats

- App/plugin ID: `jp.ehl.teethmatrix`
- Vendor: `ehl_`; AU manufacturer: `EHL1`; AU subtype: `TtMx`
- Version: `0.1.0`
- macOS: Standalone, VST3, AUv2
- Windows: Standalone, VST3
- Stereo effect, no MIDI

## Parameters

- `Tune`: fractional comb length; its two halves also select opposing loop polarity.
- `Feedback`: bounded regeneration below unity.
- `Damping`: one-pole filtering inside the loop.
- `Dispersion`: stable allpass-like phase spreading.
- `Motion`: shallow fractional-delay modulation.
- `Drive`: saturating loop injection and wet emphasis.
- `Mix`: dry/wet blend.

## Research basis

The implementation follows the feedback-comb stability and spectral-spacing model in [Physical Audio Signal Processing: Comb Filters](https://www.dsprelated.com/freebooks/pasp/Comb_Filters.html), uses the phase-dispersion principle described in [Allpass Filters](https://www.dsprelated.com/freebooks/pasp/Allpass_Filters.html), and uses interpolated delay reads consistent with [Fractional Delay Filters](https://www.dsprelated.com/freebooks/pasp/Fractional_Delay_Filters.html). TeethMatrix deliberately adds opposite stereo polarity, bounded saturation, and slow motion as product synthesis; those choices are not claims made by the references.

## Build and artifacts

Clone with `--recurse-submodules`, or initialize the shared [yup-ehl-design-module](https://github.com/EsionHsrahLatigid/yup-ehl-design-module) before configuring:

```sh
git submodule update --init
```

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

Human-facing products are staged under `artifacts/plugin-release/<platform-arch>/` in `standalone/`, `vst3/`, and macOS `au/`. For local macOS non-CI `plugin-release` builds, the staged VST3 and AU bundles are also physically copied into `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`; the Standalone app stays under `artifacts/`. Configure with `-DEHL_COPY_PLUGIN_AFTER_BUILD=OFF` to disable the local plugin copy. `build/` is internal compiler state.

## CI and release

The small caller workflows pin `EsionHsrahLatigid/yup-actions` to a full commit SHA. CI runs deterministic tests and Release product staging on macOS arm64 and Windows x64, then creates checksummed latest ZIP artifacts. A `v*` tag promotes artifacts from the successful `main` CI run for that exact commit without rebuilding.

## Safety contract

The audio callback allocates no memory and performs no locks, I/O, logging, or UI work. Parameters and non-finite input are sanitized; feedback is bounded; output stays finite within `+/-0.98`; deterministic impulse, extreme-value, hosted-silence, state, and Standalone-audition tests cover the contract.
