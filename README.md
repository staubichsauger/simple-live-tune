# Simple Live Tune

A real-time pitch-correction (autotune) plugin with **zero added latency**,
built for live use. AU + VST3 (via JUCE), macOS first.
Published as **Simple Live Tune** by **staubichsauger** (AU codes:
manufacturer `Stau`, subtype `Sltn`).

Instead of buffering audio for analysis (the usual source of autotune latency),
Simple Live Tune detects pitch on a trailing window of already-played samples
and corrects pitch with a pitch-synchronous delay-line shifter — the audio path
is never delayed. See [ARCHITECTURE.md](ARCHITECTURE.md) for the full design.

## Status

Early development — see the implementation phases in
[ARCHITECTURE.md](ARCHITECTURE.md).

## Acknowledgements

Simple Live Tune's approach is inspired by prior open-source work on
low-latency pitch correction:

- **[Autotalent](http://web.mit.edu/tbaran/www/autotalent.html)** by Tom Baran
  (GPL-2) — a LADSPA pitch-correction plugin pioneering the
  pitch-synchronous delay-line shifting technique in open source.
- **[zita-at1](https://kokkinizita.linuxaudio.org/linuxaudio/)** by
  Fons Adriaensen (GPL-3) — a high-quality low-latency autotuner for Linux.

Simple Live Tune is an independent clean-room implementation; no code from
these projects is included.

## Host integration (remote control)

All controls are plain AU parameters (global scope), so hosts can automate
Simple Live Tune with `AudioUnitSetParameter` — no string-to-value conversion
needed. Raw values are the real units shown below.

AU parameter IDs are hashes of the internal JUCE parameter IDs and are
**stable across releases** as long as the internal IDs never change (they
won't). Verified with `auval -v aufx Sltn Stau`:

| AU Param ID | Name | Type / Unit | Range (Default) |
|---|---|---|---|
| 106079 | Key | indexed, 0–11 = C..B (semitones from C) | 0–11 (0 = C) |
| 109250890 | Scale | indexed: 0 = Chromatic, 1 = Major, 2 = Minor | 0–2 (0) |
| 1213086891 | Retune Speed | ms | 0–400 (20) |
| 423325013 | Note Transition | ms | 0–200 (60) |
| 733630552 | Correction Amount | 0–1 | 0–1 (1) |
| 451310969 | Vibrato | 0–1 | 0–1 (0) |
| 1033072653 | Tune Tolerance | cents | 0–50 (0) |
| 829251210 | Confidence Threshold | 0.5–0.95 | 0.5–0.95 (0.95) |
| 3165387 | Gate | dBFS (RMS) | -80 to -20 (-45) |
| 773352680 | Bypass | boolean, 0/1 | 0/1 (0) |

Notes for host authors:
- Enharmonics: Db/C# etc. are the same Key index (1); there is no separate
  flat/sharp parameter.
- Prefer the `Bypass` parameter over `kAudioUnitProperty_BypassEffect`: the
  engine keeps running and its buffers stay warm, so un-bypassing is
  click-free (the AU property also works).
- Key/Scale changes take effect at the next control hop (~1.3 ms) and glide
  at the Note Transition speed — safe to change live mid-phrase.

## Third-party components

- **[JUCE](https://juce.com)** — application framework, used under the terms
  of the GNU AGPLv3.
- **[VST3 SDK](https://github.com/steinbergmedia/vst3sdk)** — used under the
  GPLv3 option of its dual license.
  *VST is a registered trademark of Steinberg Media Technologies GmbH.*
- **Audio Unit SDK** (Apple) — Apache-2.0.

## License

Simple Live Tune is free software, licensed under the
**GNU General Public License v3.0 or later** (GPL-3.0-or-later).
See [LICENSE](LICENSE) for the full text.
