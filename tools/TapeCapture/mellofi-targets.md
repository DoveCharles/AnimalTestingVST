# Tape MELLO-FI: measured targets

Taken from Arturia Tape MELLO-FI (installed VST3, 48 kHz) with `TapeCapture`, by playing test
signals through it. Nothing here comes from its code. Re-run `tools/TapeCapture/capture-mellofi.sh`
for the full tables in `renders-mellofi/`. Knob positions are normalised 0..1 unless noted.

## Signal path (as far as the numbers show)

Drive (symmetric saturator) → Tone voicing filter → Filter (bipolar LP/HP) → Output. Wow, Flutter and
Wear act on pitch. Noise goes in before the Tone filter (its spectrum takes the Tone shape). Stereo Width
does nothing to a clean signal: it gives each channel its own wow/flutter, so with modulation on, L/R
correlation drops to about 0.

## Drive

- Odd harmonics only (H2 below −130 dB): a symmetric curve.
- Even at Drive 0 it soft-compresses: −0.5 dB at +3 dBFS, −2.5 dB at +12 dBFS.
- Small-signal gain rises with Drive and the clipping ceiling falls, as if the saturator input is
  pushed up and the output pulled back:

| Drive | small-signal gain | output ceiling (fundamental, dBFS) | H3 at −18 dBFS in |
|---|---|---|---|
| 0 | −0.2 dB | +10.7 | −67 dB |
| 0.25 | +1.3 | +9.9 | −59 |
| 0.5 | +3.0 | +5.0 | −50 |
| 0.75 | +4.7 | −1.6 | −39 |
| 1 | +6.4 | −8.5 | −26 |

- Boost adds a lot of drive. At Drive 1 + Boost, small-signal gain is +15 dB with a −10.8 dBFS ceiling; saturated, H3 is −9.7 dB and H5 −14.4 dB, close to a square wave.
- The saturation is the same at 100 Hz and 1 kHz (H3 −49 dB at −18 dBFS, Drive 0.5), so there's no pre-emphasis around the saturator.

## Tone (Tone Enable on is the default voice)

A band-pass with a strong low cut, at Tone 0.5 (relative to 1 kHz): 31 Hz −46, 50 Hz −27, 79 Hz −10,
125 Hz −1, 250 Hz −3.5, 2 kHz +2.4, 4 kHz −1.6, 8 kHz −13.5, 16 kHz −34. The 1 kHz gain is +1.9 dB.
Tone tilts it: at 0 the top closes (8 kHz −32, 16 kHz −62) and the low mids lift; from 0.5 to 1 mainly
the 60–200 Hz region drops a further 2.5 dB, and the 1 kHz gain rises from −3.7 to +4.7 dB across the
range. With Tone Enable off, the path is flat within 0.2 dB from 125 Hz to 8 kHz (−3 dB at 31 Hz,
−1.7 dB at 16 kHz).

## Filter (bipolar, 0.5 = off)

Below 0.5 it's a low-pass, above it a high-pass, with roughly 12 dB/octave slopes. At ±1 the cutoff sits
at the band edge (at −1, 1 kHz is −46 dB; at +1, −41 dB). Per-step values are in `sweep-Filter.txt`.

## Wow and flutter (3150 Hz tone)

- **Wow** is one sine at 0.824 Hz, phase-locked and fully periodic. Depth grows steeply with the knob:

| Wow | speed RMS % | sine depth % |
|---|---|---|
| 0.2 | 0.21 | ~0.29 |
| 0.25 | 0.29 | 0.42 |
| 0.4 | 0.67 | ~0.95 |
| 0.6 | 1.74 | ~2.5 |
| 0.8 | 4.0 | ~5.7 |
| 1 | 9.2 | 12.9 |

- **Flutter** is three fixed sines (no random part), at Flutter 1: 1.99 Hz 2.09 %, 6.57 Hz 2.31 %,
  14.53 Hz 1.91 %, with smaller lines at 3.98 and 13.13 Hz. Speed RMS goes 0.20 / 0.50 / 0.95 / 1.6 / 2.6 %
  at 0.2 / 0.4 / 0.6 / 0.8 / 1.
- **Wear** is random rather than periodic: broadband speed jitter (mostly above 6 Hz), level dropouts
  down to −36 dB, and a growing high cut (16 kHz −12.7 dB at Wear 1) with a small 60–500 Hz lift.
  It also adds both even and odd distortion (Wear 1: H2 −24.5, H3 −30 dB, THD 7.2 %).
- Wow and Flutter add a small pitch offset of a couple of cents at high settings.

## Noise

- **Noise** at 1 (0 dB): −46 dBFS RMS, shaped by Tone (peaking 2–3 kHz, steep fall below 100 Hz and above 8 kHz).
  The knob is in dB: −70 dB at 0 is true silence.
- **Mechanical Noise** at 1: −44 dBFS RMS, centred 300–800 Hz, with a 23 Hz rhythm in its level (about 1 dB), i.e. a turning motor.
- No clicks were detected at any of these settings.

## Output

The knob reads in dB: −70 at 0, about 0 at 0.759 (the default), +10 at 1. The mapping is in `sweep-Output.txt`.

## Not measured yet

Tape Stop (Stop Speed in bars, Catch-up). These depend on host tempo and transport; check them by ear with `render`.
