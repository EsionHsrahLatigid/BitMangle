# BitMangle

BitMangle is a YUP stereo bitcrusher and sample-rate reducer for Digital Harsh Noise. It combines linear-to-log quantizer curvature, nonlinear sample hold, deterministic TPDF dither with bounded error feedback, adjustable prefiltering, a moving stereo quantization grid, and dry/wet blend. Dither and grid motion are signal-gated so hosted silence remains silent; Standalone adds its audition source and meters only at compile time.

## Identity and formats

- App/plugin ID: `jp.ehl.bitmangle`
- Vendor: `ehl_`; AU manufacturer: `EHL1`; AU subtype: `BtMn`
- Version: `0.1.0`
- macOS: Standalone, VST3, AUv2
- Windows: Standalone, VST3
- Stereo effect, no MIDI

## Parameters

- `Bits`: 2–16-bit quantization grid.
- `Reduce`: nonlinear 1–96-sample hold length.
- `Curve`: linear-to-log magnitude companding around quantization.
- `Dither`: deterministic TPDF level and bounded error-feedback amount.
- `AntiAlias`: reduction-aware prefilter strength; zero is a hard bypass.
- `Motion`: deterministic moving-grid depth and rate.
- `Mix`: dry/wet blend.

## Research basis

Quantization, dither, and error-shaping practice was surveyed through [AES E-Library 5482](https://secure.aes.org/forum/pubs/conferences/?elib=5482) and [AES E-Library 5139](https://secure.aes.org/forum/pubs/journal/?elib=5139), with practical bit-depth/reduction behavior cross-checked against [FFmpeg's official audio-filter documentation](https://ffmpeg.org/ffmpeg-filters.html). BitMangle's companding curve, deterministic moving grid, signal-gated dither, and reduction-aware low-pass are deliberately audible product synthesis rather than transparent requantization.

## Build and artifacts

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

Human-facing products are staged under `artifacts/plugin-release/<platform-arch>/` in `standalone/`, `vst3/`, and macOS `au/`. `build/` is internal compiler state.

## CI and safety

Caller workflows pin `EsionHsrahLatigid/yup-actions` to a full commit SHA. CI tests and packages macOS arm64 and Windows x64, producing checksummed latest ZIPs; `v*` tags promote exact-SHA CI artifacts without rebuilding. The audio callback allocates no memory and performs no locks, I/O, logging, or UI work. Quantizer resolution, sample hold, anti-alias response, mix, deterministic dither, extremes, hosted silence/state, and Standalone audition are tested.
