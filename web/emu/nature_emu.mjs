// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Gorgonzola777
// NATURE in the browser build (build/emu/felucca.wasm), in Node: it boots into NATURE, plays, answers the keys,
// knobs and buttons; the screen at each step saved as raw RGB565 (the panel's byte order) for tools/nature_shots.py;
// then the cost: 10 s of NATURE with everything on, against real time on this machine (a relative figure: the FM-1
// is far slower than this; compare runs, not machines).
//   node web/emu/nature_emu.mjs build/emu/felucca.wasm OUTDIR
import fs from "fs";
const [wasmPath = "build/emu/felucca.wasm", out = "build/emu-shots"] = process.argv.slice(2);
fs.mkdirSync(out, { recursive: true });
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
const KEY = { F3: 0, "F#3": 1, G3: 2, "G#3": 3, A3: 4, "A#3": 5, B3: 6, C4: 7, "C#4": 8, D4: 9, "D#4": 10, E4: 11, F4: 12,
              "F#4": 13, G4: 14, "G#4": 15, A4: 16, "A#4": 17, B4: 18, C5: 19, "C#5": 20, D5: 21, "D#5": 22, E5: 23, F5: 24,
              "F#5": 25, G5: 26 };
let fails = 0, n = 0;
const check = (what, ok) => { console.log(`nature: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize();
ex.web_nor_erase();
ex.web_boot();
let peak = 0;
const render = (ms) => {
  const total = Math.round(ms * 44.1);
  for (let k = 0; k < total; k += 128) {
    const m = Math.min(128, total - k);
    ex.web_render(m);
    const l = new Float32Array(mem.buffer, ex.web_out_l(), m);
    for (let i = 0; i < m; i++) peak = Math.max(peak, Math.abs(l[i]));
  }
};
const shot = (name) => {
  const fb = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  fs.writeFileSync(`${out}/${String(++n).padStart(2, "0")}_${name}.rgb565`, Buffer.from(fb.buffer));
  return new Set(fb).size;
};
const tap = (b, hold = 60) => { ex.web_buttons(1 << b); render(hold); ex.web_buttons(0); render(80); };
const key = (k, hold = 80) => { ex.web_keys(1 << KEY[k]); render(hold); ex.web_keys(0); render(80); };
const turn = (role, d) => { for (let i = 0; i < Math.abs(d); i++) { ex.web_enc(role, Math.sign(d)); render(16); } };

render(700);
check("the splash: NATURE and a scene", shot("splash") > 30);
render(1600);
check("boots into NATURE: a scene of many colours", shot("boot") > 40);
peak = 0;
render(3000);
check(`SHORE sounds (peak ${peak.toFixed(3)})`, peak > 0.01);
shot("shore");
check("the place's key lit (F3)", ((ex.web_lit_keys() >> KEY.F3) & 1) === 1);
key("D4");
render(1500);
shot("night");
check("D4: NIGHT, its key lit", ((ex.web_lit_keys() >> KEY.D4) & 1) === 1);
key("B3");
render(1200);
shot("campfire");
key("F#3");
render(300);
shot("thunder");
key("G3");
render(1500);
shot("storm");
tap(B.FX);
turn(EN.K2, 25);
shot("fx_echo");
tap(B.SCL);
turn(EN.K4, 30);
turn(EN.K3, 20);
render(600);
shot("lofi");
tap(B.ARP);
turn(EN.K1, 45);
render(1500);
shot("chimes");
tap(B.GLO);
turn(EN.K1, -25);
render(600);
shot("filter");
tap(B.SEQ);
turn(EN.K1, 1);
shot("tour");
tap(B.HOME, 1100);
shot("help");
tap(B.HOME);
tap(B.HOME);
render(400);
shot("main");
tap(B.PLAY);
render(4000);
peak = 0;
render(500);
check(`PLAY: a pause fades out (peak ${peak.toFixed(4)})`, peak < 0.002);
shot("paused");
tap(B.PLAY);
render(2500);
peak = 0;
render(500);
check(`PLAY again: it sounds (peak ${peak.toFixed(3)})`, peak > 0.005);
// the cost: DRIFT with every layer, chimes, LO-FI, the echo and the reverb on; 10 s against real time
key("E4");
tap(B.EDIT); turn(EN.K1, 60); turn(EN.K2, 60); turn(EN.K3, 60); turn(EN.K4, 60);
tap(B.EDIT); turn(EN.K1, 60); turn(EN.K2, 60); turn(EN.K3, 60); turn(EN.K4, 60);
tap(B.HOME);
const t0 = process.hrtime.bigint();
render(10000);
const s = Number(process.hrtime.bigint() - t0) / 1e9;
console.log(`nature: cost: 10 s of everything on in ${s.toFixed(2)} s (${(s * 10).toFixed(1)} % of real time here)`);
shot("everything");
console.log(fails ? `nature: ${fails} FAILED` : "nature: all ok");
process.exit(fails ? 1 : 0);
