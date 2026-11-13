import * as THREE from 'three';
import { WORLD_SIZE, WORLD_HALF, WATER_LEVEL, PLACES, terrainHeight, roadFactor } from './layout.js';

const $ = (id) => document.getElementById(id);
const proj = new THREE.Vector3();

export class UI {
  constructor() {
    this.el = {
      hud: $('hud'),
      hpFill: $('hp-fill'),
      hpLag: $('hp-lag'),
      hpText: $('hp-text'),
      coins: $('coin-count'),
      coinTotal: $('coin-total'),
      questTitle: $('quest-title'),
      questDesc: $('quest-desc'),
      questProgress: $('quest-progress'),
      prompt: $('prompt'),
      promptText: $('prompt-text'),
      hint: $('hint'),
      boss: $('boss'),
      bossName: $('boss-name'),
      bossFill: $('boss-fill'),
      bossLag: $('boss-lag'),
      toast: $('toast'),
      toastKicker: $('toast-kicker'),
      toastTitle: $('toast-title'),
      capture: $('capture'),
      bars: $('bars'),
      floaters: $('floaters'),
      vignette: $('hurt-vignette'),
      map: $('minimap'),
    };
    this.screens = ['loading', 'title', 'story', 'controls', 'pause', 'dead', 'victory'].reduce((o, k) => ((o[k] = $(`screen-${k}`)), o), {});
    this.mapCtx = this.el.map.getContext('2d');
    this.barPool = [];
    this.last = {};
    this.hintTimer = 0;
    this.hurtLevel = 0;
    this.buildMapImage();
  }

  show(name) {
    for (const [k, el] of Object.entries(this.screens)) el.classList.toggle('hidden', k !== name);
    const btn = name && this.screens[name]?.querySelector('.btn-gold');
    if (btn) btn.focus({ preventScroll: true });
  }

  setHud(on) {
    this.el.hud.classList.toggle('hidden', !on);
    this.el.bars.classList.toggle('hidden', !on);
  }

  loading(p) {
    $('load-fill').style.width = `${Math.round(p * 100)}%`;
  }

  // ---------- HUD values ----------

  health(hp, max) {
    const pct = `${(hp / max) * 100}%`;
    if (pct !== this.last.hp) {
      this.last.hp = pct;
      this.el.hpFill.style.width = pct;
      this.el.hpLag.style.width = pct;
      this.el.hpText.textContent = `${Math.ceil(hp)} / ${max}`;
    }
  }

  coins(n, total) {
    if (n !== this.last.coins) {
      this.last.coins = n;
      this.el.coins.textContent = n;
      const box = this.el.coins.parentElement;
      box.classList.remove('bump');
      void box.offsetWidth;
      box.classList.add('bump');
    }
    if (total !== this.last.total) {
      this.last.total = total;
      this.el.coinTotal.textContent = `/ ${total}`;
    }
  }

  quest(title, desc, progress) {
    if (title !== this.last.qt) {
      this.last.qt = title;
      this.el.questTitle.textContent = title;
      const panel = this.el.questTitle.parentElement;
      panel.classList.remove('flash');
      void panel.offsetWidth;
      panel.classList.add('flash');
    }
    if (desc !== this.last.qd) this.el.questDesc.textContent = this.last.qd = desc;
    if (progress !== this.last.qp) this.el.questProgress.textContent = this.last.qp = progress;
  }

  prompt(text) {
    this.el.prompt.classList.toggle('hidden', !text);
    if (text && text !== this.last.prompt) this.el.promptText.textContent = this.last.prompt = text;
  }

  hint(text, seconds = 6) {
    this.el.hint.textContent = text;
    this.el.hint.classList.add('show');
    this.hintTimer = seconds;
  }

  toast(kicker, title) {
    this.el.toastKicker.textContent = kicker;
    this.el.toastTitle.textContent = title;
    this.el.toast.classList.remove('show');
    void this.el.toast.offsetWidth;
    this.el.toast.classList.add('show');
  }

  boss(enemy) {
    this.el.boss.classList.toggle('hidden', !enemy);
    if (!enemy) return;
    this.el.bossName.textContent = enemy.cfg.name;
    const pct = `${(enemy.hp / enemy.maxHp) * 100}%`;
    if (pct !== this.last.boss) {
      this.last.boss = pct;
      this.el.bossFill.style.width = pct;
      this.el.bossLag.style.width = pct;
    }
  }

  capture(on) {
    this.el.capture.classList.toggle('hidden', !on);
  }

  hurt() {
    this.hurtLevel = 1;
  }

  floater(text, pos, camera, cls = '') {
    proj.copy(pos).project(camera);
    if (proj.z > 1) return;
    const d = document.createElement('div');
    d.className = `floater ${cls}`;
    d.textContent = text;
    d.style.left = `${((proj.x + 1) / 2) * innerWidth + (Math.random() - 0.5) * 30}px`;
    d.style.top = `${((1 - proj.y) / 2) * innerHeight}px`;
    this.el.floaters.appendChild(d);
    setTimeout(() => d.remove(), 900);
  }

  // ---------- per frame ----------

  update(dt, camera, enemies, player) {
    if (this.hintTimer > 0) {
      this.hintTimer -= dt;
      if (this.hintTimer <= 0) this.el.hint.classList.remove('show');
    }
    this.hurtLevel = Math.max(0, this.hurtLevel - dt * 1.5);
    const low = player.hp / player.maxHp < 0.3 ? 0.35 + Math.sin(performance.now() * 0.006) * 0.15 : 0;
    this.el.vignette.style.opacity = Math.max(this.hurtLevel, low).toFixed(2);

    // enemy health bars
    let used = 0;
    for (const e of enemies.list) {
      if (!e.alive || !e.awake || e.cfg.boss) continue;
      if (e.hp >= e.maxHp && e.state !== 'chase' && e.state !== 'attack') continue;
      if (e.pos.distanceTo(player.pos) > 32) continue;
      proj.copy(e.pos).setY(e.pos.y + 2.6 * e.scale).project(camera);
      if (proj.z > 1 || Math.abs(proj.x) > 1.1 || Math.abs(proj.y) > 1.1) continue;
      let bar = this.barPool[used];
      if (!bar) {
        bar = document.createElement('div');
        bar.className = 'ebar';
        bar.appendChild(document.createElement('i'));
        this.el.bars.appendChild(bar);
        this.barPool.push(bar);
      }
      bar.style.display = 'block';
      bar.style.left = `${((proj.x + 1) / 2) * innerWidth}px`;
      bar.style.top = `${((1 - proj.y) / 2) * innerHeight}px`;
      bar.firstChild.style.width = `${(e.hp / e.maxHp) * 100}%`;
      used++;
    }
    for (let i = used; i < this.barPool.length; i++) this.barPool[i].style.display = 'none';
  }

  // ---------- minimap ----------

  buildMapImage() {
    const n = 360;
    const cv = document.createElement('canvas');
    cv.width = cv.height = n;
    const g = cv.getContext('2d');
    const img = g.createImageData(n, n);
    for (let j = 0; j < n; j++) {
      for (let i = 0; i < n; i++) {
        const x = (i / n) * WORLD_SIZE - WORLD_HALF;
        const z = (j / n) * WORLD_SIZE - WORLD_HALF;
        const h = terrainHeight(x, z);
        let r = 86, gg = 128, b = 58;
        const shade = Math.max(0, Math.min(1, (h + 4) / 20));
        r += shade * 40; gg += shade * 30; b += shade * 10;
        const road = roadFactor(x, z);
        r = r + (176 - r) * road; gg = gg + (146 - gg) * road; b = b + (100 - b) * road;
        if (h < WATER_LEVEL) { r = 52; gg = 110; b = 140; }
        if (Math.abs(x) < 42 && z < -66 && z > -130) { r = 150; gg = 140; b = 120; }
        if (h > 22) { r = 120; gg = 112; b = 96; }
        const k = (j * n + i) * 4;
        img.data[k] = r; img.data[k + 1] = gg; img.data[k + 2] = b; img.data[k + 3] = 255;
      }
    }
    g.putImageData(img, 0, 0);
    // castle and village
    const toPx = (v) => ((v + WORLD_HALF) / WORLD_SIZE) * n;
    g.fillStyle = '#5a5248';
    g.fillRect(toPx(-9), toPx(-114), 18 * n / WORLD_SIZE, 20 * n / WORLD_SIZE);
    g.strokeStyle = '#3d362e';
    g.lineWidth = 3;
    g.strokeRect(toPx(-42), toPx(-129), 84 * n / WORLD_SIZE, 61 * n / WORLD_SIZE);
    g.fillStyle = 'rgba(70, 90, 140, 0.55)';
    g.beginPath();
    g.arc(toPx(PLACES.village.x), toPx(PLACES.village.z), 26 * n / WORLD_SIZE, 0, Math.PI * 2);
    g.fill();
    this.mapImage = cv;
  }

  minimap(player, camYaw, enemies, objective) {
    const ctx = this.mapCtx;
    const W = this.el.map.width;
    const c = W / 2;
    const s = 1.7; // pixels per metre
    const cos = Math.cos(camYaw), sin = Math.sin(camYaw);
    const px = player.pos.x, pz = player.pos.z;
    const toScreen = (x, z) => {
      const dx = x - px, dz = z - pz;
      return [c + s * (dx * cos - dz * sin), c + s * (dx * sin + dz * cos)];
    };

    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#2c3a1e';
    ctx.fillRect(0, 0, W, W);
    ctx.setTransform(s * cos, s * sin, -s * sin, s * cos, c - s * (px * cos - pz * sin), c - s * (px * sin + pz * cos));
    ctx.drawImage(this.mapImage, -WORLD_HALF, -WORLD_HALF, WORLD_SIZE, WORLD_SIZE);
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    // enemies
    for (const e of enemies.list) {
      if (!e.alive) continue;
      const [x, y] = toScreen(e.pos.x, e.pos.z);
      if (Math.hypot(x - c, y - c) > c - 6) continue;
      ctx.fillStyle = e.cfg.boss ? '#ff3b2f' : e.awake ? '#e8463c' : 'rgba(230, 230, 220, 0.75)';
      ctx.beginPath();
      ctx.arc(x, y, e.cfg.boss ? 6 : 3.2, 0, Math.PI * 2);
      ctx.fill();
    }

    // objective marker, pinned to the rim when far away
    if (objective) {
      let [x, y] = toScreen(objective.x, objective.z);
      const dx = x - c, dy = y - c;
      const d = Math.hypot(dx, dy);
      const max = c - 14;
      if (d > max) {
        x = c + (dx / d) * max;
        y = c + (dy / d) * max;
      }
      this.star(ctx, x, y, 9, '#ffd36a');
    }

    // compass north
    const [nx, ny] = toScreen(px, pz - 1000);
    const nd = Math.hypot(nx - c, ny - c);
    const nX = c + ((nx - c) / nd) * (c - 12), nY = c + ((ny - c) / nd) * (c - 12);
    ctx.font = '900 13px Cinzel, serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = '#000';
    ctx.fillText('N', nX + 1, nY + 1);
    ctx.fillStyle = '#e8c27a';
    ctx.fillText('N', nX, nY);

    // hero arrow: facing relative to the camera
    const rel = player.facing - (camYaw + Math.PI);
    ctx.save();
    ctx.translate(c, c);
    ctx.rotate(-rel);
    ctx.fillStyle = '#fff6dc';
    ctx.strokeStyle = '#2b1a07';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(0, -9);
    ctx.lineTo(6.5, 7);
    ctx.lineTo(0, 3.5);
    ctx.lineTo(-6.5, 7);
    ctx.closePath();
    ctx.stroke();
    ctx.fill();
    ctx.restore();

    // inner shading ring
    const grad = ctx.createRadialGradient(c, c, c * 0.7, c, c, c);
    grad.addColorStop(0, 'rgba(0,0,0,0)');
    grad.addColorStop(1, 'rgba(0,0,0,0.45)');
    ctx.fillStyle = grad;
    ctx.fillRect(0, 0, W, W);
  }

  star(ctx, x, y, r, color) {
    ctx.save();
    ctx.translate(x, y);
    ctx.beginPath();
    for (let i = 0; i < 10; i++) {
      const a = (i / 10) * Math.PI * 2 - Math.PI / 2;
      const rr = i % 2 ? r * 0.45 : r;
      ctx.lineTo(Math.cos(a) * rr, Math.sin(a) * rr);
    }
    ctx.closePath();
    ctx.fillStyle = color;
    ctx.strokeStyle = '#3b2306';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.fill();
    ctx.restore();
  }
}
