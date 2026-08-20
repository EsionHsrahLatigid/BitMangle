# Design

## Source of truth
- Status: Active
- Last refreshed: 2026-08-12
- Primary surfaces: YUP Standalone, VST3, AUv2 editor

## Product
- Goal: expose quantization topology as an aggressive, repeatable performance instrument for incoming audio.
- Non-goals: transparent mastering dither, codec emulation, MIDI instrument behavior.
- Signal path: reduction-aware low-pass -> deterministic quantizer/error feedback -> sample hold -> dry/wet -> bounded output.
- Main controls: Bits, Reduce, Curve, Dither, AntiAlias, Motion, Mix.

## Unified visual system
- 640x360 resizable canvas with preserved aspect ratio, a 4px base grid, and 8px group spacing.
- Seven-column single-row parameter grid with textual values and native host gestures.
- Fixed EHL palette: ink `#050505`, low `#2A2A2A`, mid `#8A8A86`, paper `#F2F2F0`.
- Square `fillRect` geometry, scanlines, grid bars, and clockwise 16-step parameter indicators; no gradients, glow, rounded cards, or flashing.
- Standalone-only audition buttons and 24-step input/output meters; hosted editors expose no generator controls.

## Interaction and accessibility
- High-contrast text and values remain visible at all times.
- Meter motion is functional and limited to a 30 Hz decaying display.
- Hosted silence stays silent even with dither and motion; Standalone audition is runtime-only and never serialized.

## Implementation contract
- C++20/YUP; no new runtime dependency or external asset.
- Audio thread performs no allocation, locks, I/O, logging, or UI calls.
- Seven stable parameter IDs; state magic `BMN1`.
- App/plugin ID `jp.ehl.bitmangle`; vendor `ehl_`; AU `BtMn` / `EHL1`.
- Tests cover resolution, sample hold, anti-alias attenuation, dry mix, deterministic dither, extreme values, hosted silence/state, and Standalone audition/meter isolation.

## Open questions
- [ ] Tune moving-grid rate ranges after the first public listening round.
