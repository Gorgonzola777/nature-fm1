# NATURE: the manual

NATURE turns the M-VAVE FM-1 into a generative nature machine: wind, sea, rain, fire, birds, crickets, wind chimes
and thunder, all synthesised in real time and changing by themselves, with a pixel-art scene on the screen that
follows the sound. This page explains every control. A one-page cheat sheet: [NATURE-cheatsheet.pdf](NATURE-cheatsheet.pdf).
Try it first in your browser: <https://gorgonzola777.github.io/nature-fm1/>.

## 1. The first minute

1. Switch the FM-1 on. After the NATURE splash you are on **SHORE**: the sea at dusk, a person on the hill.
2. Press **G3**: a storm. Press **F#3** (a black key): thunder. Hold it and press harder: louder thunder.
3. Turn **KNOB 1** (MOOD) to the left: the storm calms down. To the right: it builds again.
4. Press **D4**: night, with crickets and the moon.
5. **Hold HOME** for a second: the key map on the screen. Any button or key closes it.

Nothing you do can break the sound: **HOME** brings the place back as it was.

## 2. The screen

- **Top**: the place's name. On the right: a moving dot while it plays (PAUSED when paused), TOUR when the tour is on,
  `Z15` and so on while the sleep timer counts down (minutes left).
- **Middle**: the scene. It is drawn from the sound itself: waves break when you hear them break, lightning comes with
  the thunder, sparks with the crackles, the bird on the pine sings when you hear it, the chimes flash when struck.
- **Messages**: every key and button says what it did in the middle of the scene for a moment (STORM, THUNDER,
  WIND OFF, SAVED...).
- **Bottom**: the five layers (WIND, SEA, RAIN, FIRE, BIRDS) with what they play right now and the key that switches
  them; the page's name and its four knobs with their values; REVERB (the SELECT knob) and TIME (the ALGORITHM knob).

## 3. The keys

### White keys: places

A place is a whole scene: which layers play, how loud, the mood, the time of day, the reverb. A new place fades in
within half a second (ENV > GLIDE sets how fast). The key of the place you are in is lit.

| Key | Place | | Key | Place |
| --- | --- | --- | --- | --- |
| F3 | SHORE: the sea at dusk, birds, a breeze | | E4 | DRIFT: everything, always changing |
| G3 | STORM: wind, waves and rain, thunder | | D5 | MEADOW: grass in the wind, many birds |
| A3 | FOREST: birds in the pines | | E5 | MONSOON: heavy rain on a stormy sea |
| B3 | CAMPFIRE: a fire at dusk | | F5 | WINTER: cold wind, a small fire |
| C4 | RAIN: steady rain | | G5 | DAWN: birdsong over a calm sea |
| D4 | NIGHT: the sea at night, crickets | | | |

### White keys: layers on / off

**F4** WIND · **G4** SEA · **A4** RAIN · **B4** FIRE · **C5** BIRDS. A layer's key is lit while it plays. A layer you
switch on that the place did not have comes in at a medium level; set it on EDIT's MIX page.

### Black keys: events

| Key | Event | | Key | Event |
| --- | --- | --- | --- | --- |
| F#3 | THUNDER | | G#4 | BIRD CHORUS |
| G#3 | BIG WAVE | | A#4 | CALM: the weather settles |
| A#3 | GUST | | C#5 | STORM: the weather turns |
| C#4 | DOWNPOUR | | D#5 | CRICKETS |
| D#4 | CRACKLE (the fire) | | F#5 | SHUFFLE: the scene picks a new mix |
| F#4 | BIRD | | | |

An event whose layer is off brings it in for a while, then it fades. **Key pressure**: hold an event key and press
harder for a stronger event (louder thunder, a bigger wave, a harder gust, heavier rain, more crackles, more birds). The
FM-1 reads pressure on F3 to C5, so it works on every event up to A#4. Sensitivity: GLO > SENS.

## 4. The knobs

- **KNOB 1 to 4**: the four of the page on the screen (below). On MAIN: **MOOD** (calm to storm; every layer answers it),
  **LIFE** (how much the scene changes by itself: layers come and go, the weather wanders), **TONE** (dark to bright),
  **VOL**.
- **SELECT**: REVERB. **ALGORITHM**: TIME (day, dusk, night). **PRESETS**: the places, one by one. **MASTER**: volume.

## 5. The buttons and their pages

A page button shows its page (its button lit, its name above the knobs); press it again for its second page or back to
MAIN. **HOME** goes back to MAIN from any page.

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- | --- |
| FX | FX 1: SPACE + ECHO | REVERB | ECHO | TIME (60 ms .. 2 s) | REPEAT |
| FX again | FX 2: COLOR | CHORUS | DRIVE | ROOM (size) | TYPE (room, spring, hall) |
| LFO | LFO: SLOW MOTION | TARGET | RATE (0.01 .. 3.7 Hz) | DEPTH | SHAPE (sine, triangle, square, random) |
| ENV | ENV: FADES + SPEED | FADE IN | FADE OUT | GLIDE (place to place) | EVENT (how long) |
| EDIT | MIX 1: LEVELS | WIND | SEA | RAIN | FIRE |
| EDIT again | MIX 2 | BIRDS | CRICKETS | THUNDER (how often) | CHIMES |
| SCL | LO-FI: TAPE + VINYL | CRUSH (bits) | RATE (sample rate) | WOBBLE (tape) | DUST (vinyl) |
| GLO | GLOBAL: MASTER | FILTER (left: low-pass, right: high-pass) | RESO | PAN | SENS (key pressure) |
| ARP | CHIMES: IN THE WIND | LEVEL | KEY | SCALE (major, minor, Japan, whole tone, Lydian) | DECAY |
| SEQ | TOUR: IT PLAYS ITSELF | TOUR (on / off) | EVERY (30 s .. 20 min) | ORDER (next, random) | AUTO EV (events by themselves) |

The LFO's TARGET can be MOOD, TONE, any layer, VOL, REVERB, ECHO, FILTER or CHIMES: try MOOD at a slow rate for
weather that comes and goes, or FILTER for a breathing sound. The knob it moves shows a `~`.

The other buttons:

- **PLAY**: pause / play, with a fade (ENV's FADE OUT / FADE IN). PLAY breathes while paused. Any key plays again.
- **HOME**: on a page, back to MAIN; on MAIN, the place as it was (every change to it undone; effects stay).
  **Held**: the key map (HELP).
- **OCT- / OCT+**: the previous / next place.
- **REC**: the sleep timer. Each press: 15, 30, 60, 90 minutes, off. REC is lit while it runs; when it runs out, the
  sound fades out (GOOD NIGHT).
- **SAVE**: keeps the sound as it is now (the place, the layers, every knob) for the next power-on. **Held 2 s**: back to
  the factory sound at power-on.

## 6. Ideas

- **Sleep**: NIGHT or RAIN, VOL low, REC to 30 or 60 minutes.
- **Focus**: CAMPFIRE or RAIN, LIFE around half, SEQ > TOUR on, EVERY 10 MIN.
- **An endless film**: DRIFT, LIFE high, SEQ > AUTO EV up: it tells its own story.
- **Music**: ARP > CHIMES in your key, the FM-1's USB audio into your DAW, black keys on the sequencer in time.
- **Old radio**: SCL > CRUSH and DUST up, GLO > FILTER to the left.

## 7. Install, update, go back

- Install from [The Firmware Hub](https://thefirmwarehub.com), or download `nature-X.Y.fwsc` from the
  [releases](https://github.com/Gorgonzola777/nature-fm1/releases) and install it with
  [M-Flasher](https://github.com/jvitkauskas/m-flasher) or `python3 tools/fm1_install.py nature-X.Y.fwsc`.
- Back to M-VAVE's firmware: [FM1 Workbench](https://open.m-vave.com/fm1-workbench) (V15).
- Older and newer FM-1s alike (both flash chips).
- **Update mode**: hold OCT- and OCT+. The screen counts down (UPDATE MODE IN 3, 2, 1); let go to cancel. After 5 s the
  FM-1 enters the chip's update mode: the screen goes dark and a computer sees it as `WL82 UBOOT` (do not format it).
  Any installer can write the FM-1 from there. Switch it off and on to leave that mode.

## 8. Credits

NATURE by Gorgonzola777, built on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton
Instruments): its hardware layer, sound core, effects, USB and update path. GPL-3.0. M-VAVE and FM-1 are trademarks of
their owners; NATURE is not affiliated with M-VAVE.
