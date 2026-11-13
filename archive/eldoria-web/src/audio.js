// All sound is synthesised with Web Audio: effects plus an adaptive medieval soundtrack.

const mtof = (m) => 440 * Math.pow(2, (m - 69) / 12);

// D dorian progression: Dm - C - Bb - C
const CHORDS = [
  [50, 57, 62, 65, 69],
  [48, 55, 60, 64, 67],
  [46, 53, 58, 62, 65],
  [48, 55, 60, 64, 67],
];
const MELODY = [74, 72, 69, 72, 74, 77, 76, 74, 72, 69, 67, 69, 72, 74, 72, 69];

export class Audio {
  constructor() {
    this.ctx = null;
    this.enabled = localStorage.getItem('eld-sound') !== 'off';
    this.intensity = 0; // 0 explore, 1 combat, 2 boss
    this.step = 0;
  }

  init() {
    if (this.ctx) {
      if (this.ctx.state === 'suspended') this.ctx.resume();
      return;
    }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    const ctx = (this.ctx = new AC());
    this.master = ctx.createGain();
    this.master.gain.value = this.enabled ? 0.85 : 0;
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -16;
    comp.ratio.value = 3.5;
    this.master.connect(comp).connect(ctx.destination);

    this.music = ctx.createGain();
    this.music.gain.value = 0.32;
    this.music.connect(this.master);
    this.sfx = ctx.createGain();
    this.sfx.gain.value = 0.75;
    this.sfx.connect(this.master);

    // hall reverb for the music
    const conv = ctx.createConvolver();
    const len = ctx.sampleRate * 2.4;
    const ir = ctx.createBuffer(2, len, ctx.sampleRate);
    for (let ch = 0; ch < 2; ch++) {
      const d = ir.getChannelData(ch);
      for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 2.6);
    }
    conv.buffer = ir;
    this.reverb = ctx.createGain();
    this.reverb.gain.value = 0.45;
    this.reverb.connect(conv).connect(this.music);

    const nlen = ctx.sampleRate;
    this.noise = ctx.createBuffer(1, nlen, ctx.sampleRate);
    const nd = this.noise.getChannelData(0);
    for (let i = 0; i < nlen; i++) nd[i] = Math.random() * 2 - 1;

    // soft wind bed
    const wind = ctx.createBufferSource();
    wind.buffer = this.noise;
    wind.loop = true;
    const wf = ctx.createBiquadFilter();
    wf.type = 'bandpass';
    wf.frequency.value = 500;
    wf.Q.value = 0.5;
    const lfo = ctx.createOscillator();
    lfo.frequency.value = 0.08;
    const lfoGain = ctx.createGain();
    lfoGain.gain.value = 250;
    lfo.connect(lfoGain).connect(wf.frequency);
    const wg = ctx.createGain();
    wg.gain.value = 0.05;
    wind.connect(wf).connect(wg).connect(this.master);
    wind.start();
    lfo.start();

    this.nextTime = ctx.currentTime + 0.2;
    setInterval(() => this.schedule(), 40);
  }

  setEnabled(on) {
    this.enabled = on;
    localStorage.setItem('eld-sound', on ? 'on' : 'off');
    if (this.master) this.master.gain.setTargetAtTime(on ? 0.85 : 0, this.ctx.currentTime, 0.05);
  }

  setIntensity(k) {
    this.intensity = k;
  }

  // ---------- music ----------

  schedule() {
    if (!this.ctx) return;
    const bpm = this.intensity >= 2 ? 132 : this.intensity >= 1 ? 112 : 84;
    const stepDur = 60 / bpm / 2; // eighth notes
    while (this.nextTime < this.ctx.currentTime + 0.2) {
      this.playStep(this.step, this.nextTime, stepDur);
      this.step = (this.step + 1) % 32;
      this.nextTime += stepDur;
    }
  }

  playStep(step, t, dur) {
    const chord = CHORDS[Math.floor(step / 8)];
    const s = step % 8;
    if (s === 0) this.pad(t, chord, dur * 8);
    // plucked arpeggio
    const arp = [1, 2, 3, 4, 3, 2, 3, 4];
    this.pluck(t, mtof(chord[arp[s]] + 12), 0.07, dur * 3);
    if (s === 0 || s === 4) this.pluck(t, mtof(chord[0] - 12), 0.12, dur * 4, 'triangle');
    if (this.intensity === 0 && s % 2 === 0 && step % 16 < 12 && Math.random() < 0.6) {
      this.flute(t, mtof(MELODY[(step / 2) % 16]), dur * 1.8);
    }
    if (this.intensity >= 1) {
      if (s === 0 || s === 3 || s === 6 || (this.intensity >= 2 && s === 4)) this.drum(t, 70, 0.5);
      if (s === 2 || s === 6) this.snare(t, 0.18);
      if (this.intensity >= 2) this.hat(t, s % 2 ? 0.04 : 0.07);
      if (this.intensity >= 2 && s % 2 === 0) this.pluck(t, mtof(chord[0] - 24), 0.2, dur * 1.5, 'sawtooth', 600);
    }
  }

  env(g, t, a, peak, dur) {
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(peak, t + a);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  }

  pluck(t, f, peak, dur, type = 'triangle', cutoff = 3200) {
    const o = this.ctx.createOscillator();
    o.type = type;
    o.frequency.value = f;
    const flt = this.ctx.createBiquadFilter();
    flt.type = 'lowpass';
    flt.frequency.setValueAtTime(cutoff, t);
    flt.frequency.exponentialRampToValueAtTime(400, t + dur);
    const g = this.ctx.createGain();
    this.env(g, t, 0.004, peak, dur);
    o.connect(flt).connect(g);
    g.connect(this.music);
    g.connect(this.reverb);
    o.start(t);
    o.stop(t + dur + 0.05);
  }

  flute(t, f, dur) {
    const o = this.ctx.createOscillator();
    o.type = 'sine';
    o.frequency.value = f;
    const vib = this.ctx.createOscillator();
    vib.frequency.value = 5;
    const vg = this.ctx.createGain();
    vg.gain.value = 4;
    vib.connect(vg).connect(o.frequency);
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(0.05, t + 0.08);
    g.gain.linearRampToValueAtTime(0.0001, t + dur);
    o.connect(g);
    g.connect(this.music);
    g.connect(this.reverb);
    o.start(t);
    vib.start(t);
    o.stop(t + dur + 0.05);
    vib.stop(t + dur + 0.05);
  }

  pad(t, notes, dur) {
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(0.03, t + 0.8);
    g.gain.linearRampToValueAtTime(0.0001, t + dur);
    const flt = this.ctx.createBiquadFilter();
    flt.type = 'lowpass';
    flt.frequency.value = 900;
    g.connect(flt);
    flt.connect(this.music);
    flt.connect(this.reverb);
    for (const n of notes.slice(0, 4)) {
      for (const det of [-7, 7]) {
        const o = this.ctx.createOscillator();
        o.type = 'sawtooth';
        o.frequency.value = mtof(n);
        o.detune.value = det;
        o.connect(g);
        o.start(t);
        o.stop(t + dur + 0.1);
      }
    }
  }

  drum(t, f, peak) {
    const o = this.ctx.createOscillator();
    o.frequency.setValueAtTime(f * 2, t);
    o.frequency.exponentialRampToValueAtTime(f * 0.6, t + 0.2);
    const g = this.ctx.createGain();
    this.env(g, t, 0.003, peak, 0.4);
    o.connect(g).connect(this.music);
    o.start(t);
    o.stop(t + 0.45);
  }

  noiseHit(t, type, freq, q, peak, dur, dest) {
    const src = this.ctx.createBufferSource();
    src.buffer = this.noise;
    const f = this.ctx.createBiquadFilter();
    f.type = type;
    f.frequency.value = freq;
    f.Q.value = q;
    const g = this.ctx.createGain();
    this.env(g, t, 0.003, peak, dur);
    src.connect(f).connect(g).connect(dest ?? this.sfx);
    src.start(t, Math.random() * 0.5);
    src.stop(t + dur + 0.05);
    return f;
  }

  snare(t, peak) {
    this.noiseHit(t, 'bandpass', 1500, 0.7, peak, 0.18, this.music);
  }

  hat(t, peak) {
    this.noiseHit(t, 'highpass', 7000, 0.6, peak, 0.05, this.music);
  }

  // ---------- effects ----------

  get now() {
    return this.ctx ? this.ctx.currentTime : 0;
  }

  tone(type, f0, f1, dur, peak, delay = 0) {
    if (!this.ctx) return;
    const t = this.now + delay;
    const o = this.ctx.createOscillator();
    o.type = type;
    o.frequency.setValueAtTime(f0, t);
    o.frequency.exponentialRampToValueAtTime(f1, t + dur);
    const g = this.ctx.createGain();
    this.env(g, t, 0.005, peak, dur);
    o.connect(g).connect(this.sfx);
    o.start(t);
    o.stop(t + dur + 0.05);
  }

  swing(combo) {
    if (!this.ctx) return;
    const t = this.now;
    const f = this.noiseHit(t, 'bandpass', 900, 1.4, 0.28, 0.22);
    f.frequency.setValueAtTime(600 + combo * 200, t);
    f.frequency.exponentialRampToValueAtTime(3200, t + 0.18);
  }

  hit(heavy) {
    if (!this.ctx) return;
    const t = this.now;
    this.noiseHit(t, 'lowpass', 900, 1, heavy ? 0.7 : 0.5, 0.18);
    this.tone('square', heavy ? 160 : 220, 60, 0.12, 0.25);
    this.noiseHit(t + 0.02, 'bandpass', 2500, 3, 0.25, 0.08);
  }

  boneBreak() {
    if (!this.ctx) return;
    const t = this.now;
    for (let i = 0; i < 5; i++) this.noiseHit(t + i * 0.05 + Math.random() * 0.03, 'bandpass', 1800 + Math.random() * 2000, 4, 0.3, 0.06);
    this.tone('sine', 120, 40, 0.5, 0.3);
  }

  block() {
    this.tone('square', 1250, 1150, 0.25, 0.12);
    this.tone('square', 1870, 1700, 0.2, 0.08);
    if (this.ctx) this.noiseHit(this.now, 'highpass', 4000, 1, 0.2, 0.1);
  }

  hurt() {
    this.tone('sawtooth', 180, 90, 0.25, 0.2);
    if (this.ctx) this.noiseHit(this.now, 'lowpass', 700, 1, 0.4, 0.15);
  }

  coin() {
    this.tone('sine', 1568, 1568, 0.25, 0.14);
    this.tone('sine', 2093, 2093, 0.35, 0.12, 0.06);
  }

  potion() {
    for (let i = 0; i < 6; i++) this.tone('sine', 400 + i * 120, 700 + i * 140, 0.08, 0.1, i * 0.05);
  }

  jump(double) {
    this.tone('sine', double ? 300 : 200, double ? 700 : 420, 0.15, 0.12);
    if (this.ctx) this.noiseHit(this.now, 'bandpass', 1800, 1, 0.08, 0.12);
  }

  land(v) {
    if (this.ctx) this.noiseHit(this.now, 'lowpass', 400, 1, Math.min(0.5, v * 0.03), 0.15);
  }

  step(speed) {
    if (this.ctx) this.noiseHit(this.now, 'lowpass', 500 + Math.random() * 300, 1, speed > 7 ? 0.16 : 0.1, 0.07);
  }

  dodge() {
    if (!this.ctx) return;
    const f = this.noiseHit(this.now, 'bandpass', 500, 1, 0.25, 0.3);
    f.frequency.exponentialRampToValueAtTime(2000, this.now + 0.25);
  }

  cast() {
    this.tone('sine', 300, 1200, 0.4, 0.12);
    this.tone('triangle', 450, 1800, 0.35, 0.06);
  }

  rise() {
    if (!this.ctx) return;
    this.noiseHit(this.now, 'lowpass', 300, 1, 0.35, 0.8);
    this.tone('sawtooth', 60, 90, 0.7, 0.1);
  }

  chest() {
    if (!this.ctx) return;
    this.tone('sawtooth', 120, 260, 0.4, 0.06);
    [784, 988, 1175, 1568].forEach((f, i) => this.tone('sine', f, f, 0.5, 0.12, 0.35 + i * 0.08));
  }

  fanfare(big) {
    const notes = big ? [62, 66, 69, 74, 69, 74, 78, 81] : [67, 71, 74, 79];
    notes.forEach((n, i) => {
      this.tone('square', mtof(n), mtof(n), big ? 0.45 : 0.3, 0.07, i * (big ? 0.16 : 0.12));
      this.tone('triangle', mtof(n), mtof(n), big ? 0.6 : 0.4, 0.12, i * (big ? 0.16 : 0.12));
    });
  }

  roar() {
    if (!this.ctx) return;
    const t = this.now;
    const o = this.ctx.createOscillator();
    o.type = 'sawtooth';
    o.frequency.setValueAtTime(70, t);
    o.frequency.linearRampToValueAtTime(110, t + 0.5);
    o.frequency.linearRampToValueAtTime(50, t + 1.6);
    const f = this.ctx.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.value = 500;
    const g = this.ctx.createGain();
    this.env(g, t, 0.15, 0.45, 1.7);
    o.connect(f).connect(g).connect(this.sfx);
    o.start(t);
    o.stop(t + 1.8);
    this.noiseHit(t, 'lowpass', 400, 1, 0.4, 1.5);
  }

  slam() {
    this.tone('sine', 90, 30, 0.6, 0.6);
    if (this.ctx) this.noiseHit(this.now, 'lowpass', 600, 1, 0.5, 0.5);
  }

  death() {
    this.tone('sawtooth', 220, 55, 1.2, 0.2);
    [62, 60, 57, 50].forEach((n, i) => this.tone('triangle', mtof(n), mtof(n), 0.6, 0.12, 0.3 + i * 0.3));
  }

  click() {
    this.tone('sine', 900, 700, 0.06, 0.1);
  }
}
