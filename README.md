# NATURE for the M-VAVE FM-1

![NATURE](assets/nature/card.png) ![screen](assets/nature/screen.png)

**NATURE** turns the FM-1 into a generative soundscape machine: **wind, sea, rain, fire, birds and thunder**, all
synthesised in real time (no samples), mixing and changing by themselves so the scene never repeats. A pixel-art
landscape on the screen follows the sound: waves break when the surf hisses, lightning strikes with the thunder,
sparks fly when the fire crackles, a bird on the pine sings when you hear it, and the person on the hill opens an
umbrella when it rains.

NATURE is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments): every
Felucca engine, the sequencer, effects, presets and MIDI stay as they are, NATURE is one more engine (and its screen).

## Install

Install the `.fwsc` from the [releases](../../releases) (or from [The Firmware Hub](https://thefirmwarehub.com)) in
Chrome or Edge over USB. The installer can always take you back to M-VAVE's official V15. Back up your projects first.

## Playing NATURE

Pick the NATURE engine (PRESETS: ENG knob, or hold EDIT and press E5) and one of its scenes:
**SHORE, STORM, FOREST, CAMPFIRE, RAINY, DRIFT**.

| Control | What it does |
| --- | --- |
| **White keys** | Latch: a white key starts the scene, it plays on with no key held. Another white key retunes it (lower: darker and closer, higher: brighter). A short light tap on the same key fades it out. |
| **Black keys** | Events: F#3 THUNDER, G#3 BIG WAVE, A#3 GUST, C#4 DOWNPOUR, D#4 CRACKLE, F#4 BIRD, G#4 BIRD CHORUS, A#4 CALM, C#5 STORM, D#5 SHUFFLE (new mix now), F#5 THUNDER. A layer that is off comes in for a while. |
| **Key pressure** | (F3..C5, aftertouch: switched on for NATURE, EDIT > VOICE 3 > AT) Hold a white key and press: the weather leans into a storm, and calms when you let go. Hold a black key and press: the event plays live (louder thunder, bigger wave, harder gust, heavier rain, more crackles, more birdsong). |
| **HOME knobs 1-4** | MOOD (calm .. storm), EVOL (how much the scene lives by itself), TONE (dark .. bright), REL (fade-out) |
| **EDIT page 1 (MIX)** | WIND, SEA, RAIN, FIRE levels |
| **EDIT page 2 (SCENE)** | BIRD, MOOD, EVOL, TONE |
| **ENV ATK / REL** | the scene's fade in / fade out |
| **OCT- / OCT+** | move the keys (the events stay on their black keys) |

Everything else is Felucca's: FX (reverb and delay suit NATURE well), LFO and the mod matrix (try LFO -> E6 MOOD for
weather that comes and goes), SEQ (black-key notes on steps play thunder and waves in time), ARP, SAVE, MIDI.
MIDI CC74 moves TONE, CC71 MOOD.

## Building

As Felucca ([README.felucca.md](README.felucca.md), [BUILDING.md](BUILDING.md)). GitHub builds every push
(`.github/workflows/build.yml`) and every `v*` tag as a release with the FM-1 Firmware Hub's
[fm1-build](https://github.com/zvenson/fm1-build) workflow. Identity: `FM-1_6283XY` (release X.Y), `FM-1_628300` for
test builds.

The engine is `firmware/src/eng_nature.c` (MIT), its screen `firmware/src/ui_nature.c` (GPL-3.0-only).

## Licence and credits

GPL-3.0-only, as Felucca (see [LICENSING.md](LICENSING.md), [CREDITS.md](CREDITS.md)). Felucca (C) 2026 Leo Kuroshita,
Hügelton Instruments. NATURE engine and screen (C) 2026 Gorgonzola777. Felucca's sample pack: CC0 (VSCO-2 CE, VCSL by
Versilian Studios). M-VAVE and FM-1 are trademarks of their owners; NATURE is not affiliated with M-VAVE or Felucca.
