#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Gorgonzola777
"""The emulator's screens (web/emu/nature_emu.mjs: raw RGB565, the panel's byte order) as PNGs, x2, and one sheet.
  tools/nature_shots.py DIR"""
import pathlib
import sys

from PIL import Image

d = pathlib.Path(sys.argv[1])
shots = []
for f in sorted(d.glob("*.rgb565")):
    raw = f.read_bytes()
    im = Image.new("RGB", (240, 240))
    px = im.load()
    for i in range(240 * 240):
        c = raw[2 * i] << 8 | raw[2 * i + 1]          # (byte-swapped: the panel's order)
        px[i % 240, i // 240] = (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
    im = im.resize((480, 480), Image.NEAREST)
    im.save(f.with_suffix(".png"))
    shots.append(im)
if shots:
    cols = 4
    sheet = Image.new("RGB", (cols * 480, ((len(shots) + cols - 1) // cols) * 480))
    for i, im in enumerate(shots):
        sheet.paste(im, ((i % cols) * 480, (i // cols) * 480))
    sheet.save(d / "sheet.png")
print(f"{len(shots)} screens -> {d}")
