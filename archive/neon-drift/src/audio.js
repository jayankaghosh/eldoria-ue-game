// Procedural synthwave soundtrack and sound effects, all generated with Web Audio.

const BPM = 112;
const STEP = 60 / BPM / 4; // sixteenth note

// A minor progression: Am - F - C - G
const CHORDS = [
  { root: 45, notes: [57, 60, 64, 69] },
  { root: 41, notes: [53, 57, 60, 65] },
  { root: 48, notes: [55, 60, 64, 67] },
  { root: 43, notes: [55, 59, 62, 67] },
];
const ARP = [0, 1, 2, 3, 2, 1, 2, 3, 0, 2, 1, 3, 2, 3, 1, 2];

const mtof = (m) => 440 * Math.pow(2, (m - 69) / 12);

export class Audio {
  constructor() {
    this.ctx = null;
    this.enabled = localStorage.getItem('nd-sound') !== 'off';
    this.playingMusic = false;
    this.step = 0;
    this.nextTime = 0;
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
    this.master.gain.value = this.enabled ? 0.8 : 0;
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -14;
    comp.ratio.value = 4;
    this.master.connect(comp).connect(ctx.destination);

    this.musicFilter = ctx.createBiquadFilter();
    this.musicFilter.type = 'lowpass';
    this.musicFilter.frequency.value = 900;
    this.music = ctx.createGain();
    this.music.gain.value = 0.55;
    this.music.connect(this.musicFilter).connect(this.master);

    this.sfx = ctx.createGain();
    this.sfx.gain.value = 0.7;
    this.sfx.connect(this.master);

    // echo send for the arpeggio
    this.delay = ctx.createDelay(1);
    this.delay.delayTime.value = STEP * 3;
    const fb = ctx.createGain();
    fb.gain.value = 0.35;
    const wet = ctx.createGain();
    wet.gain.value = 0.4;
    this.delay.connect(fb).connect(this.delay);
    this.delay.connect(wet).connect(this.music);

    const len = ctx.sampleRate * 1;
    this.noise = ctx.createBuffer(1, len, ctx.sampleRate);
    const d = this.noise.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;

    this.nextTime = ctx.currentTime + 0.1;
    this.timer = setInterval(() => this.schedule(), 25);
  }

  setEnabled(on) {
    this.enabled = on;
    localStorage.setItem('nd-sound', on ? 'on' : 'off');
    if (this.master) this.master.gain.setTargetAtTime(on ? 0.8 : 0, this.ctx.currentTime, 0.05);
  }

  // intensity: 0 = muffled menu mix, 1 = full in-game mix
  setIntensity(k) {
    if (!this.ctx) return;
    const f = 600 + Math.pow(k, 2) * 17000;
    this.musicFilter.frequency.setTargetAtTime(f, this.ctx.currentTime, 0.25);
  }

  startMusic() {
    this.playingMusic = true;
  }

  schedule() {
    if (!this.ctx || !this.playingMusic) {
      if (this.ctx) this.nextTime = Math.max(this.nextTime, this.ctx.currentTime + 0.05);
      return;
    }
    while (this.nextTime < this.ctx.currentTime + 0.12) {
      this.playStep(this.step, this.nextTime);
      this.step = (this.step + 1) % 64;
      this.nextTime += STEP;
    }
  }

  playStep(step, t) {
    const bar = Math.floor(step / 16);
    const s = step % 16;
    const chord = CHORDS[bar];

    if (s % 4 === 0) this.kick(t);
    if (s === 4 || s === 12) this.snare(t);
    if (s % 2 === 1) this.hat(t, s % 4 === 3 ? 0.07 : 0.04);

    // bass on 8ths with an octave bounce
    if (s % 2 === 0) {
      const note = chord.root - 12 + (s % 8 === 6 ? 12 : 0);
      this.bass(t, mtof(note), STEP * 1.8);
    }
    // arpeggio
    const n = chord.notes[ARP[s]] + (s >= 8 ? 12 : 0);
    this.lead(t, mtof(n), STEP * 0.9);
    // pad at the top of each bar
    if (s === 0) this.pad(t, chord.notes, STEP * 16);
  }

  env(g, t, a, peak, dur) {
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(peak, t + a);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  }

  kick(t) {
    const ctx = this.ctx;
    const o = ctx.createOscillator();
    const g = ctx.createGain();
    o.frequency.setValueAtTime(140, t);
    o.frequency.exponentialRampToValueAtTime(42, t + 0.12);
    this.env(g, t, 0.003, 0.9, 0.32);
    o.connect(g).connect(this.music);
    o.start(t);
    o.stop(t + 0.35);
  }

  noiseHit(t, type, freq, q, peak, dur, dest = this.music) {
    const ctx = this.ctx;
    const src = ctx.createBufferSource();
    src.buffer = this.noise;
    const f = ctx.createBiquadFilter();
    f.type = type;
    f.frequency.value = freq;
    f.Q.value = q;
    const g = ctx.createGain();
    this.env(g, t, 0.002, peak, dur);
    src.connect(f).connect(g).connect(dest);
    src.start(t, Math.random() * 0.5);
    src.stop(t + dur + 0.02);
  }

  snare(t) {
    this.noiseHit(t, 'bandpass', 1800, 0.8, 0.5, 0.2);
    const o = this.ctx.createOscillator();
    const g = this.ctx.createGain();
    o.type = 'triangle';
    o.frequency.setValueAtTime(220, t);
    o.frequency.exponentialRampToValueAtTime(140, t + 0.08);
    this.env(g, t, 0.002, 0.25, 0.12);
    o.connect(g).connect(this.music);
    o.start(t);
    o.stop(t + 0.15);
  }

  hat(t, peak) {
    this.noiseHit(t, 'highpass', 8000, 0.5, peak, 0.05);
  }

  bass(t, freq, dur) {
    const ctx = this.ctx;
    const o = ctx.createOscillator();
    o.type = 'sawtooth';
    o.frequency.value = freq;
    const f = ctx.createBiquadFilter();
    f.type = 'lowpass';
    f.Q.value = 6;
    f.frequency.setValueAtTime(1400, t);
    f.frequency.exponentialRampToValueAtTime(180, t + dur);
    const g = ctx.createGain();
    this.env(g, t, 0.005, 0.32, dur);
    o.connect(f).connect(g).connect(this.music);
    o.start(t);
    o.stop(t + dur + 0.05);
  }

  lead(t, freq, dur) {
    const ctx = this.ctx;
    const o = ctx.createOscillator();
    o.type = 'square';
    o.frequency.value = freq;
    const f = ctx.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.value = 2600;
    const g = ctx.createGain();
    this.env(g, t, 0.004, 0.06, dur);
    o.connect(f).connect(g);
    g.connect(this.music);
    g.connect(this.delay);
    o.start(t);
    o.stop(t + dur + 0.05);
  }

  pad(t, notes, dur) {
    const ctx = this.ctx;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(0.05, t + 0.5);
    g.gain.linearRampToValueAtTime(0.0001, t + dur);
    const f = ctx.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.value = 1200;
    g.connect(f).connect(this.music);
    for (const n of notes) {
      for (const det of [-8, 8]) {
        const o = ctx.createOscillator();
        o.type = 'sawtooth';
        o.frequency.value = mtof(n);
        o.detune.value = det;
        o.connect(g);
        o.start(t);
        o.stop(t + dur + 0.05);
      }
    }
  }

  // ---------- SFX ----------

  tone(type, f0, f1, dur, peak = 0.3, when = 0) {
    if (!this.ctx) return;
    const t = this.ctx.currentTime + when;
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

  pickup(combo) {
    const base = 880 * Math.pow(2, Math.min(combo, 24) / 24);
    this.tone('sine', base, base * 1.5, 0.12, 0.25);
    this.tone('triangle', base * 2, base * 2.5, 0.08, 0.08);
  }

  jump() {
    this.tone('sine', 220, 520, 0.18, 0.2);
    if (this.ctx) this.noiseHit(this.ctx.currentTime, 'bandpass', 2500, 1, 0.08, 0.18, this.sfx);
  }

  land() {
    if (this.ctx) this.noiseHit(this.ctx.currentTime, 'lowpass', 500, 1, 0.15, 0.12, this.sfx);
  }

  whoosh() {
    if (this.ctx) this.noiseHit(this.ctx.currentTime, 'bandpass', 1200, 2, 0.12, 0.25, this.sfx);
  }

  power() {
    this.tone('square', 330, 990, 0.25, 0.12);
    this.tone('sine', 660, 1320, 0.3, 0.2, 0.08);
  }

  boost() {
    this.tone('sawtooth', 120, 900, 0.6, 0.18);
    if (this.ctx) this.noiseHit(this.ctx.currentTime, 'highpass', 3000, 0.7, 0.15, 0.6, this.sfx);
  }

  shieldBreak() {
    this.tone('square', 900, 120, 0.35, 0.18);
    if (this.ctx) this.noiseHit(this.ctx.currentTime, 'bandpass', 3000, 1, 0.3, 0.3, this.sfx);
  }

  crash() {
    if (!this.ctx) return;
    const t = this.ctx.currentTime;
    this.noiseHit(t, 'lowpass', 1600, 0.7, 0.9, 1.2, this.sfx);
    this.tone('sawtooth', 160, 30, 0.9, 0.5);
    this.tone('sine', 90, 25, 1.2, 0.7);
  }

  beep(high) {
    this.tone('square', high ? 1320 : 660, high ? 1320 : 660, high ? 0.4 : 0.15, 0.15);
  }

  click() {
    this.tone('sine', 1200, 900, 0.05, 0.12);
  }
}
