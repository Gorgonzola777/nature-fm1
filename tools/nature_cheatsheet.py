# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Gorgonzola777
# NATURE's one-page cheat sheet (A4 landscape PDF): python3 tools/nature_cheatsheet.py docs/NATURE-cheatsheet.pdf
import sys
from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, white, black
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
pdfmetrics.registerFont(TTFont("Sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("SansB", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("Mono", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"))
W, H = landscape(A4)
BG, INK, DIM, ACC, SUN = HexColor("#151213"), HexColor("#f4f1e8"), HexColor("#8a8494"), HexColor("#ff8f5a"), HexColor("#ffd86b")
PLACE, LAYER, EVENT = HexColor("#ffd86b"), HexColor("#7fd1c8"), HexColor("#ff8f5a")
c = canvas.Canvas(sys.argv[1], pagesize=(W, H))
c.setTitle("NATURE for the FM-1: cheat sheet"); c.setAuthor("Gorgonzola777")
c.setFillColor(BG); c.rect(0, 0, W, H, fill=1, stroke=0)
c.setFillColor(SUN); c.setFont("Mono", 30); c.drawString(36, H - 56, "NATURE")
c.setFillColor(INK); c.setFont("Sans", 11); c.drawString(170, H - 52, "generative nature soundscapes for the M-VAVE FM-1  ·  cheat sheet")
c.setFillColor(DIM); c.setFont("Sans", 8.5)
c.drawString(170, H - 66, "try it in a browser: gorgonzola777.github.io/nature-fm1   ·   manual: github.com/Gorgonzola777/nature-fm1/blob/main/docs/MANUAL.md")
# the keyboard
whites = [("F3", "SHORE", PLACE), ("G3", "STORM", PLACE), ("A3", "FOREST", PLACE), ("B3", "CAMPFIRE", PLACE),
          ("C4", "RAIN", PLACE), ("D4", "NIGHT", PLACE), ("E4", "DRIFT", PLACE), ("F4", "WIND", LAYER),
          ("G4", "SEA", LAYER), ("A4", "RAIN", LAYER), ("B4", "FIRE", LAYER), ("C5", "BIRDS", LAYER),
          ("D5", "MEADOW", PLACE), ("E5", "MONSOON", PLACE), ("F5", "WINTER", PLACE), ("G5", "DAWN", PLACE)]
blacks = {0: ("F#3", "THUNDER"), 1: ("G#3", "BIG WAVE"), 2: ("A#3", "GUST"), 4: ("C#4", "DOWNPOUR"), 5: ("D#4", "CRACKLE"),
          7: ("F#4", "BIRD"), 8: ("G#4", "CHORUS"), 9: ("A#4", "CALM"), 11: ("C#5", "STORM"), 12: ("D#5", "CRICKETS"),
          14: ("F#5", "SHUFFLE")}
x0, kw, ky, kh = 36, 48, H - 250, 150
for i, (n, label, col) in enumerate(whites):
    x = x0 + i * kw
    c.setFillColor(HexColor("#2a2530")); c.setStrokeColor(HexColor("#4a4350")); c.roundRect(x + 1, ky, kw - 2, kh, 4, fill=1, stroke=1)
    c.setFillColor(col); c.roundRect(x + 4, ky + 4, kw - 8, 4, 1, fill=1, stroke=0)
    c.setFillColor(DIM); c.setFont("Sans", 7); c.drawCentredString(x + kw / 2, ky + 30, n)
    c.setFillColor(col); c.setFont("SansB", 6.6 if len(label) > 6 else 7.4); c.drawCentredString(x + kw / 2, ky + 16, label)
for i, (n, label) in blacks.items():
    x = x0 + (i + 1) * kw - 15
    c.setFillColor(HexColor("#0b090a")); c.setStrokeColor(EVENT); c.roundRect(x, ky + 52, 30, kh - 52, 3, fill=1, stroke=1)
    c.saveState(); c.translate(x + 18, ky + kh - 8); c.rotate(-90)
    c.setFillColor(EVENT); c.setFont("SansB", 7.2); c.drawString(0, 0, label)
    c.restoreState()
    c.setFillColor(DIM); c.setFont("Sans", 6.5); c.drawCentredString(x + 15, ky + 58, n)
# legend
ly = ky - 18
for col, t in [(PLACE, "places: the whole scene glides to it (the place's key is lit)"),
               (LAYER, "layers on / off (lit while on)"),
               (EVENT, "events: hold and press harder for more (pressure: F#3..A#4)")]:
    c.setFillColor(col); c.rect(x0, ly, 9, 9, fill=1, stroke=0)
    c.setFillColor(INK); c.setFont("Sans", 8.5); c.drawString(x0 + 14, ly + 1, t)
    x0 += 14 + c.stringWidth(t, "Sans", 8.5) + 22
x0 = 36
# buttons and pages
ty = ly - 30
c.setFillColor(SUN); c.setFont("SansB", 11); c.drawString(36, ty, "BUTTONS AND PAGES"); c.drawString(470, ty, "KNOBS")
rows = [("FX", "REVERB · ECHO · TIME · REPEAT", "again: CHORUS · DRIVE · ROOM · TYPE"),
        ("LFO", "TARGET · RATE · DEPTH · SHAPE", "slow motion on mood, tone, a layer, filter..."),
        ("ENV", "FADE IN · FADE OUT · GLIDE · EVENT", "fades, how fast places change, events' length"),
        ("EDIT", "WIND · SEA · RAIN · FIRE", "again: BIRDS · CRICKETS · THUNDER · CHIMES"),
        ("SCL", "LO-FI: CRUSH · RATE · WOBBLE · DUST", "bits, sample rate, tape, vinyl"),
        ("GLO", "FILTER · RESO · PAN · SENS", "DJ filter: left low-pass, right high-pass"),
        ("ARP", "CHIMES: LEVEL · KEY · SCALE · DECAY", "wind chimes rung by the gusts"),
        ("SEQ", "TOUR · EVERY · ORDER · AUTO EV", "it moves from place to place by itself"),
        ("PLAY", "pause / play", "with a fade"),
        ("HOME", "back to MAIN · on MAIN: reset the place", "HOLD: the key map on the screen"),
        ("OCT- +", "previous / next place", "both held 5 s: update mode (let go: cancel)"),
        ("REC", "sleep timer: 15 · 30 · 60 · 90 min · off", "then it fades out"),
        ("SAVE", "keep this sound for power-on", "held 2 s: the factory sound")]
y = ty - 18
for b, a, n in rows:
    c.setFillColor(ACC); c.setFont("Mono", 9); c.drawString(36, y, b)
    c.setFillColor(INK); c.setFont("Sans", 8.6); c.drawString(84, y, a)
    c.setFillColor(DIM); c.setFont("Sans", 7.8); c.drawString(270, y, n)
    y -= 13.2
# knobs
ky2 = ty - 18
for k, a, n in [("KNOB 1", "MOOD", "calm .. storm: everything answers it"), ("KNOB 2", "LIFE", "how much it changes by itself"),
                ("KNOB 3", "TONE", "dark .. bright"), ("KNOB 4", "VOL", ""), ("", "", "(on MAIN; other pages: as named on screen)"),
                ("SELECT", "REVERB", ""), ("ALGORITHM", "TIME", "day · dusk · night (moon, crickets)"),
                ("PRESETS", "PLACES", "one by one"), ("MASTER", "VOLUME", "")]:
    c.setFillColor(ACC); c.setFont("Mono", 9); c.drawString(470, ky2, k)
    c.setFillColor(INK); c.setFont("SansB", 8.6); c.drawString(545, ky2, a)
    c.setFillColor(DIM); c.setFont("Sans", 7.8); c.drawString(600, ky2, n)
    ky2 -= 13.2
c.setFillColor(SUN); c.setFont("SansB", 11); c.drawString(470, ky2 - 8, "IDEAS")
for i, t in enumerate(["sleep: NIGHT, VOL low, REC 30 min", "focus: CAMPFIRE or RAIN, SEQ > TOUR on",
                       "a film: DRIFT, LIFE high, SEQ > AUTO EV", "old radio: SCL > CRUSH + DUST, GLO > FILTER left"]):
    c.setFillColor(INK); c.setFont("Sans", 8.4); c.drawString(470, ky2 - 24 - i * 12.5, "·  " + t)
c.setFillColor(DIM); c.setFont("Sans", 7)
c.drawString(36, 26, "NATURE by Gorgonzola777, built on Felucca by Leo Kuroshita (Hügelton Instruments). GPL-3.0. M-VAVE and FM-1 are trademarks of their owners; NATURE is not affiliated with M-VAVE.")
c.showPage(); c.save()
print("ok")
