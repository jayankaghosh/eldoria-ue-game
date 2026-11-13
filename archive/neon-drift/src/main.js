import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/examples/jsm/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';
import { World } from './world.js';
import { Player } from './player.js';
import { Track } from './track.js';
import { Particles, Shockwaves } from './fx.js';
import { Audio } from './audio.js';

// ---------- Renderer & post ----------
const canvas = document.getElementById('game');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.1, 2000);

const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
const bloom = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 0.75, 0.45, 0.32);
composer.addPass(bloom);

const lensPass = new ShaderPass({
  uniforms: {
    tDiffuse: { value: null },
    uAberration: { value: 0.0015 },
    uVignette: { value: 1.0 },
    uTime: { value: 0 },
  },
  vertexShader: /* glsl */ `
    varying vec2 vUv;
    void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
  fragmentShader: /* glsl */ `
    uniform sampler2D tDiffuse; uniform float uAberration; uniform float uVignette; uniform float uTime;
    varying vec2 vUv;
    void main() {
      vec2 c = vUv - 0.5;
      float d = dot(c, c);
      vec2 off = c * uAberration * (1.0 + d * 6.0);
      vec3 col;
      col.r = texture2D(tDiffuse, vUv + off).r;
      col.g = texture2D(tDiffuse, vUv).g;
      col.b = texture2D(tDiffuse, vUv - off).b;
      col *= 1.0 - d * 1.1 * uVignette;
      // subtle scanlines and grain
      col *= 0.97 + 0.03 * sin(vUv.y * 900.0);
      col += (fract(sin(dot(vUv * (uTime + 1.0), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * 0.025;
      gl_FragColor = vec4(col, 1.0);
    }`,
});
composer.addPass(lensPass);
composer.addPass(new OutputPass());

// ---------- Game objects ----------
const world = new World(scene);
const player = new Player(scene);
const track = new Track(scene);
const particles = new Particles(scene, 3500);
const waves = new Shockwaves(scene);
const audio = new Audio();

// ---------- UI ----------
const $ = (id) => document.getElementById(id);
const ui = {
  hud: $('hud'),
  score: $('hud-score'),
  mult: $('hud-mult'),
  speed: $('hud-speed'),
  dist: $('hud-dist'),
  shield: $('hud-shield'),
  boost: $('hud-boost'),
  combo: $('combo-pop'),
  countdown: $('countdown'),
  flash: $('flash'),
  screens: {
    title: $('screen-title'),
    how: $('screen-how'),
    pause: $('screen-pause'),
    over: $('screen-over'),
  },
};

function showScreen(name) {
  for (const [k, el] of Object.entries(ui.screens)) el.classList.toggle('hidden', k !== name);
  const focusBtn = name && ui.screens[name]?.querySelector('.btn-primary');
  if (focusBtn) focusBtn.focus({ preventScroll: true });
}

function flash(kind) {
  ui.flash.className = 'flash';
  void ui.flash.offsetWidth;
  ui.flash.className = `flash ${kind}`;
}

function popText(text) {
  ui.combo.textContent = text;
  ui.combo.classList.remove('show');
  void ui.combo.offsetWidth;
  ui.combo.classList.add('show');
}

let best = Number(localStorage.getItem('nd-best') || 0);
$('title-best').textContent = best.toLocaleString();

const soundBtn = $('btn-sound');
const refreshSoundLabel = () => (soundBtn.textContent = `SOUND: ${audio.enabled ? 'ON' : 'OFF'}`);
refreshSoundLabel();

const press = (id, fn) =>
  $(id).addEventListener('click', (e) => {
    e.stopPropagation();
    audio.init();
    audio.click();
    fn();
  });

press('btn-play', () => startCountdown());
press('btn-how', () => showScreen('how'));
press('btn-how-back', () => showScreen('title'));
press('btn-sound', () => {
  audio.setEnabled(!audio.enabled);
  refreshSoundLabel();
});
press('btn-resume', () => resume());
press('btn-quit', () => toTitle());
press('btn-retry', () => startCountdown());
press('btn-menu', () => toTitle());
$('btn-pause').addEventListener('click', (e) => {
  e.stopPropagation();
  pause();
});

// ---------- State ----------
const START_SPEED = 42;
const MAX_SPEED = 102;
const BOOST_TIME = 3.5;

let state = 'title';
const run = {};
function resetRun() {
  Object.assign(run, {
    score: 0,
    dist: 0,
    combo: 0,
    bestMult: 1,
    mult: 1,
    hasShield: false,
    boost: 0,
    invuln: 0,
    speed: 30,
    time: 0,
  });
}
resetRun();

let countdownT = 0;
let countdownN = 0;
let dyingT = 0;
let timeScale = 1;
let shake = 0;
let fovKick = 0;

function toTitle() {
  state = 'title';
  track.reset();
  player.reset();
  particles.clear();
  waves.clear();
  resetRun();
  ui.hud.classList.add('hidden');
  ui.countdown.classList.add('hidden');
  $('title-best').textContent = best.toLocaleString();
  showScreen('title');
  audio.setIntensity(0);
}

function startCountdown() {
  audio.init();
  audio.startMusic();
  audio.setIntensity(0.6);
  track.reset();
  player.reset();
  particles.clear();
  waves.clear();
  resetRun();
  showScreen(null);
  ui.hud.classList.remove('hidden');
  ui.shield.classList.add('hidden');
  ui.boost.classList.add('hidden');
  state = 'countdown';
  countdownT = 0;
  countdownN = 3;
  tickCountdown();
}

function tickCountdown() {
  ui.countdown.classList.remove('hidden', 'tick');
  void ui.countdown.offsetWidth;
  ui.countdown.textContent = countdownN > 0 ? countdownN : 'GO!';
  ui.countdown.classList.add('tick');
  audio.beep(countdownN === 0);
}

function pause() {
  if (state !== 'playing' && state !== 'countdown') return;
  run.pausedFrom = state;
  state = 'paused';
  audio.setIntensity(0.1);
  showScreen('pause');
}

function resume() {
  if (state !== 'paused') return;
  state = run.pausedFrom;
  audio.setIntensity(1);
  showScreen(null);
}

function gameOver() {
  state = 'over';
  ui.hud.classList.add('hidden');
  const score = Math.floor(run.score);
  const isBest = score > best;
  if (isBest) {
    best = score;
    localStorage.setItem('nd-best', String(best));
  }
  $('over-score').textContent = score.toLocaleString();
  $('over-dist').textContent = `${Math.floor(run.dist).toLocaleString()} M`;
  $('over-combo').textContent = `x${run.bestMult}`;
  $('over-best').textContent = best.toLocaleString();
  $('new-best').classList.toggle('hidden', !isBest);
  showScreen('over');
  audio.setIntensity(0.15);
}

// ---------- Gameplay events ----------
const tmpV = new THREE.Vector3();
const COL = {
  shard: new THREE.Color('#2ff3ff').multiplyScalar(2),
  white: new THREE.Color('#ffffff').multiplyScalar(2),
  red: new THREE.Color('#ff3b5c').multiplyScalar(2.5),
  orange: new THREE.Color('#ff8a1f').multiplyScalar(2.5),
  pink: new THREE.Color('#ff2fd1').multiplyScalar(2.2),
  green: new THREE.Color('#6cff9a').multiplyScalar(2),
  yellow: new THREE.Color('#ffe14d').multiplyScalar(2.2),
};

function addScore(base) {
  run.score += base * run.mult * (run.boost > 0 ? 2 : 1);
}

const handlers = {
  spawning: true,
  collect(e) {
    const y = e.mesh.position.y;
    if (e.type === 'shard') {
      run.combo++;
      const newMult = Math.min(8, 1 + Math.floor(run.combo / 6));
      if (newMult > run.mult) {
        run.mult = newMult;
        run.bestMult = Math.max(run.bestMult, newMult);
        popText(`x${newMult} MULTIPLIER`);
        ui.mult.classList.add('bump');
        setTimeout(() => ui.mult.classList.remove('bump'), 160);
      }
      addScore(25);
      audio.pickup(run.combo);
      particles.burst(e.x, y, e.z, COL.shard, 18, 7, 0.5, 0.45, { world: true, drag: 3 });
      particles.burst(e.x, y, e.z, COL.white, 6, 4, 0.35, 0.3, { world: true, drag: 3 });
    } else if (e.type === 'shield') {
      run.hasShield = true;
      ui.shield.classList.remove('hidden');
      audio.power();
      popText('SHIELD UP');
      particles.burst(e.x, y, e.z, COL.green, 40, 10, 0.6, 0.6, { world: true, drag: 2 });
      waves.spawn(player.x, 0.1, 0, COL.green, 6, 0.5);
      addScore(100);
    } else if (e.type === 'boost') {
      run.boost = BOOST_TIME;
      ui.boost.classList.remove('hidden');
      audio.boost();
      popText('BOOST!');
      flash('good');
      fovKick = 1;
      particles.burst(e.x, y, e.z, COL.yellow, 50, 12, 0.6, 0.6, { world: true, drag: 2 });
      waves.spawn(player.x, 0.1, 0, COL.yellow, 9, 0.6);
      addScore(150);
    }
  },
  miss() {
    if (run.combo >= 6) popText('COMBO LOST');
    run.combo = 0;
    run.mult = 1;
  },
  nearMiss(e) {
    if (state !== 'playing') return;
    if (e.type === 'low') {
      addScore(30);
    } else if (Math.abs(e.x - player.x) < e.halfW + 1.5) {
      addScore(60);
      popText('CLOSE CALL +' + 60 * run.mult);
      audio.whoosh();
    }
  },
  hit(e) {
    if (state !== 'playing') return;
    const col = e.type === 'low' ? COL.orange : COL.red;
    if (run.boost > 0 || run.invuln > 0) {
      smash(e, col);
      addScore(80);
      return;
    }
    if (run.hasShield) {
      run.hasShield = false;
      run.invuln = 1.3;
      ui.shield.classList.add('hidden');
      smash(e, col);
      audio.shieldBreak();
      flash('hit');
      shake = 0.6;
      popText('SHIELD DOWN');
      return;
    }
    crash(col);
  },
};

function smash(e, col) {
  e.mesh.visible = false;
  particles.burst(e.x, 1.2, e.z, col, 60, 14, 0.7, 0.8, { world: true, drag: 1.5, gravity: 12 });
  waves.spawn(e.x, 0.1, e.z, col, 7, 0.5);
  shake = Math.max(shake, 0.4);
}

function crash(col) {
  state = 'dying';
  dyingT = 0;
  timeScale = 0.25;
  shake = 1.4;
  audio.crash();
  flash('hit');
  player.worldPos(tmpV);
  player.group.visible = false;
  particles.burst(tmpV.x, tmpV.y, tmpV.z, COL.pink, 140, 22, 0.9, 1.4, { drag: 1.2, gravity: 14 });
  particles.burst(tmpV.x, tmpV.y, tmpV.z, col, 100, 16, 0.8, 1.2, { drag: 1.2, gravity: 14 });
  particles.burst(tmpV.x, tmpV.y, tmpV.z, COL.white, 50, 10, 0.6, 0.6, { drag: 2 });
  waves.spawn(tmpV.x, 0.1, tmpV.z, COL.pink, 14, 0.9);
  waves.spawn(tmpV.x, 0.1, tmpV.z, COL.white, 8, 0.6);
  audio.setIntensity(0.2);
}

// ---------- Input ----------
const keys = new Set();
window.addEventListener('keydown', (e) => {
  const k = e.code;
  if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'Space'].includes(k)) e.preventDefault();
  if (keys.has(k)) return;
  keys.add(k);
  audio.init();
  if (k === 'Escape' || k === 'KeyP') {
    if (state === 'paused') resume();
    else pause();
    return;
  }
  if (state === 'playing' && (k === 'Space' || k === 'KeyW' || k === 'ArrowUp')) doJump();
  if (k === 'Enter' && document.activeElement?.tagName !== 'BUTTON') {
    if (state === 'title') startCountdown();
    else if (state === 'over') startCountdown();
  }
});
window.addEventListener('keyup', (e) => keys.delete(e.code));
window.addEventListener('blur', () => {
  keys.clear();
  pause();
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden) pause();
});

function doJump() {
  if (player.jump()) {
    audio.jump();
    particles.burst(player.x, 0.3, 0.5, COL.pink, 16, 5, 0.5, 0.4, { world: true, drag: 3, up: 0.3 });
  }
}

// touch: drag to steer, tap to jump
let touch = null;
canvas.addEventListener('pointerdown', (e) => {
  audio.init();
  if (state !== 'playing' && state !== 'countdown') return;
  touch = { id: e.pointerId, x0: e.clientX, px0: player.x, t0: performance.now(), moved: 0 };
  canvas.setPointerCapture(e.pointerId);
});
canvas.addEventListener('pointermove', (e) => {
  if (!touch || e.pointerId !== touch.id) return;
  const dx = e.clientX - touch.x0;
  touch.moved = Math.max(touch.moved, Math.abs(dx));
  const sens = 14 / Math.min(window.innerWidth, 900);
  player.targetX = touch.px0 + dx * sens;
});
const endTouch = (e) => {
  if (!touch || e.pointerId !== touch.id) return;
  if (touch.moved < 12 && performance.now() - touch.t0 < 260 && state === 'playing') doJump();
  touch = null;
  player.targetX = null;
};
canvas.addEventListener('pointerup', endTouch);
canvas.addEventListener('pointercancel', endTouch);

// ---------- Camera ----------
const camPos = new THREE.Vector3(-4.5, 1.8, 6.5);
const camLook = new THREE.Vector3(1, 2.4, -20);
const wantPos = new THREE.Vector3();
const wantLook = new THREE.Vector3();

function updateCamera(dt, realDt) {
  if (state === 'title') {
    const t = world.time * 0.18;
    wantPos.set(-4.2 + Math.sin(t) * 1.6, 1.5 + Math.sin(t * 0.7) * 0.4, 6.2 + Math.cos(t) * 0.8);
    wantLook.set(1.2, 2.6, -20);
  } else {
    wantPos.set(player.x * 0.55, 3.1 + player.y * 0.45, 7.4);
    wantLook.set(player.x * 0.7, 1.2 + player.y * 0.35, -14);
  }
  const k = state === 'countdown' ? 3 : 7;
  camPos.lerp(wantPos, Math.min(1, k * realDt));
  camLook.lerp(wantLook, Math.min(1, k * realDt));
  camera.position.copy(camPos);
  if (shake > 0) {
    const s = shake * shake * 0.5;
    camera.position.x += (Math.random() - 0.5) * s;
    camera.position.y += (Math.random() - 0.5) * s;
    shake = Math.max(0, shake - realDt * 2.2);
  }
  camera.lookAt(camLook);

  const speedK = THREE.MathUtils.clamp((run.speed - START_SPEED) / (MAX_SPEED - START_SPEED), 0, 1);
  fovKick = Math.max(0, fovKick - realDt * 1.5);
  const targetFov = 68 + speedK * 10 + (run.boost > 0 ? 8 : 0) + fovKick * 6;
  camera.fov += (targetFov - camera.fov) * Math.min(1, 4 * realDt);
  camera.updateProjectionMatrix();
  world.follow(camera);
}

// ---------- HUD ----------
let lastHud = {};
function updateHud() {
  const score = Math.floor(run.score).toLocaleString();
  const speed = Math.round(run.speed * 3.6).toString();
  const dist = Math.floor(run.dist).toLocaleString();
  const mult = `x${run.mult}`;
  if (score !== lastHud.score) ui.score.textContent = lastHud.score = score;
  if (speed !== lastHud.speed) ui.speed.textContent = lastHud.speed = speed;
  if (dist !== lastHud.dist) ui.dist.textContent = lastHud.dist = dist;
  if (mult !== lastHud.mult) ui.mult.textContent = lastHud.mult = mult;
}

// ---------- Main loop ----------
const clock = new THREE.Clock();
let trailAcc = 0;

function frame() {
  const realDt = Math.min(clock.getDelta(), 1 / 20);
  timeScale += (1 - timeScale) * Math.min(1, realDt * (state === 'dying' ? 0.6 : 3));
  const dt = state === 'paused' ? 0 : realDt * timeScale;

  let steer = 0;
  if (keys.has('ArrowLeft') || keys.has('KeyA')) steer -= 1;
  if (keys.has('ArrowRight') || keys.has('KeyD')) steer += 1;

  switch (state) {
    case 'title': {
      run.speed = 30;
      // gentle autopilot sway so the menu scene feels alive
      const sway = Math.sin(world.time * 0.6) * 2.2;
      player.targetX = sway;
      player.update(dt, 0, { boost: 0 });
      world.update(dt, run.speed, 0);
      break;
    }
    case 'countdown': {
      countdownT += dt;
      if (countdownT >= 0.75) {
        countdownT -= 0.75;
        countdownN--;
        if (countdownN < 0) {
          ui.countdown.classList.add('hidden');
          state = 'playing';
          audio.setIntensity(1);
          run.speed = START_SPEED;
        } else {
          tickCountdown();
        }
      }
      run.speed += (START_SPEED - run.speed) * Math.min(1, dt * 1.5);
      player.update(dt, steer, { boost: 0 });
      world.update(dt, run.speed, 0);
      break;
    }
    case 'playing': {
      run.time += dt;
      run.dist += run.speed * dt;
      const diff = Math.min(1, run.dist / 11000);
      const base = START_SPEED + (MAX_SPEED - START_SPEED) * (1 - Math.pow(1 - diff, 1.6));
      const target = base * (run.boost > 0 ? 1.35 : 1);
      run.speed += (target - run.speed) * Math.min(1, dt * 2);
      if (run.boost > 0) {
        run.boost -= dt;
        if (run.boost <= 0) ui.boost.classList.add('hidden');
      }
      run.invuln = Math.max(0, run.invuln - dt);
      addScore(run.speed * dt * 0.1);

      player.update(dt, steer, {
        boost: run.boost > 0 ? 1 : 0,
        shield: run.hasShield || run.boost > 0,
        invulnBlink: run.invuln > 0,
      });
      if (player.landed) {
        player.landed = false;
        audio.land();
        particles.burst(player.x, 0.2, 0.3, COL.pink, 12, 4, 0.4, 0.35, { world: true, drag: 3, up: 0.2 });
      }
      player.hasShield = run.hasShield;
      world.update(dt, run.speed, run.boost > 0 ? 1 : 0);
      track.update(dt, run.speed, diff, player, handlers);
      updateHud();
      break;
    }
    case 'dying': {
      dyingT += realDt;
      run.speed *= Math.max(0, 1 - dt * 1.5);
      world.update(dt, run.speed, 0);
      track.update(dt, run.speed, 0, player, { ...handlers, spawning: false, hit() {}, collect() {}, miss() {}, nearMiss() {} });
      if (dyingT > 1.6) gameOver();
      break;
    }
    case 'over': {
      run.speed += (14 - run.speed) * Math.min(1, dt);
      world.update(dt, run.speed, 0);
      track.update(dt, run.speed, 0, player, { ...handlers, spawning: false, hit() {}, collect() {}, miss() {}, nearMiss() {} });
      break;
    }
  }

  // engine trail
  if (player.group.visible && dt > 0) {
    trailAcc += dt;
    while (trailAcc > 1 / 60) {
      trailAcc -= 1 / 60;
      player.emitTrail(particles, run.speed, run.boost > 0 ? 1 : 0);
    }
  }
  // speed dust while boosting
  if (state === 'playing' && run.boost > 0) {
    for (let i = 0; i < 3; i++) {
      particles.spawn(
        (Math.random() - 0.5) * 30, Math.random() * 8, -60 - Math.random() * 40,
        0, 0, run.speed * 0.8, COL.yellow, 0.35, 1.2, { world: true }
      );
    }
  }

  particles.update(dt, state === 'dying' || state === 'over' ? run.speed : run.speed);
  waves.update(dt, run.speed);
  updateCamera(dt, realDt);

  const ab = 0.0015 + (run.boost > 0 ? 0.004 : 0) + shake * 0.006;
  lensPass.uniforms.uAberration.value += (ab - lensPass.uniforms.uAberration.value) * Math.min(1, realDt * 6);
  lensPass.uniforms.uTime.value = world.time % 100;

  composer.render();
  requestAnimationFrame(frame);
}

// ---------- Resize ----------
function resize() {
  const w = window.innerWidth;
  const h = window.innerHeight;
  const pr = Math.min(window.devicePixelRatio, 2);
  renderer.setPixelRatio(pr);
  renderer.setSize(w, h);
  composer.setPixelRatio(pr);
  composer.setSize(w, h);
  bloom.resolution.set(w, h);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  particles.setPixelScale(pr * (h / 900));
}
window.addEventListener('resize', resize);
resize();

showScreen('title');
requestAnimationFrame(frame);
