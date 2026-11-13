import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';
import { loadAssets } from './assets.js';
import { World } from './world.js';
import { Player } from './player.js';
import { Enemies } from './enemies.js';
import { Pickups } from './pickups.js';
import { Quest } from './quest.js';
import { FX } from './fx.js';
import { UI } from './ui.js';
import { Audio } from './audio.js';
import { PLACES, terrainHeight } from './layout.js';

class Events {
  constructor() {
    this.map = {};
  }
  on(name, fn) {
    (this.map[name] ||= []).push(fn);
  }
  emit(name, arg) {
    for (const fn of this.map[name] || []) fn(arg);
  }
}

// ---------- renderer ----------
const canvas = document.getElementById('game');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(devicePixelRatio, 1.75));
renderer.setSize(innerWidth, innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.0;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(55, innerWidth / innerHeight, 0.1, 1200);

const target = new THREE.WebGLRenderTarget(innerWidth, innerHeight, { type: THREE.HalfFloatType, samples: 4 });
const composer = new EffectComposer(renderer, target);
composer.addPass(new RenderPass(scene, camera));
const bloom = new UnrealBloomPass(new THREE.Vector2(innerWidth, innerHeight), 0.35, 0.5, 0.92);
composer.addPass(bloom);
composer.addPass(new OutputPass());

const ui = new UI();
const audio = new Audio();
const events = new Events();
const game = { state: 'loading', checkpoint: new THREE.Vector3() };
// exposed for play-testing in development builds only
if (import.meta.env.DEV) window.__game = game;

// ---------- boot ----------
ui.show('loading');
loadAssets((p) => ui.loading(p))
  .then(init)
  .catch((err) => {
    console.error(err);
    document.getElementById('load-text').textContent = 'The realm failed to load. Please refresh the page.';
  });

function init() {
  game.world = new World(scene, renderer);
  game.fx = new FX(scene);
  game.player = new Player(scene, game.world.physics, events);
  game.enemies = new Enemies(scene, game.world.physics, events, game.fx);
  game.pickups = new Pickups(scene, game.world.physics, events, game.fx);
  game.quest = new Quest(game);
  game.quest.populate();
  game.stats = { coins: 0, kills: 0, deaths: 0, time: 0 };
  buildBeacon();

  const spawn = new THREE.Vector3(PLACES.spawn.x, terrainHeight(PLACES.spawn.x, PLACES.spawn.z), PLACES.spawn.z);
  game.checkpoint.copy(spawn);
  game.player.reset(spawn, Math.PI);
  resize();

  // compile shaders up front so the first frames do not stutter
  renderer.compile(scene, camera);
  game.state = 'title';
  ui.show('title');
  refreshSound();
  requestAnimationFrame(frame);
}

// ---------- objective beacon ----------
let beacon;
function buildBeacon() {
  const geo = new THREE.CylinderGeometry(1.1, 1.6, 160, 24, 1, true);
  geo.translate(0, 80, 0);
  const mat = new THREE.ShaderMaterial({
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    side: THREE.DoubleSide,
    uniforms: { uTime: { value: 0 }, uAlpha: { value: 1 } },
    vertexShader: /* glsl */ `
      varying float vY; varying vec3 vN; varying vec3 vView;
      void main() {
        vY = position.y / 160.0;
        vN = normalize(normalMatrix * normal);
        vec4 mv = modelViewMatrix * vec4(position, 1.0);
        vView = normalize(-mv.xyz);
        gl_Position = projectionMatrix * mv;
      }`,
    fragmentShader: /* glsl */ `
      uniform float uTime; uniform float uAlpha;
      varying float vY; varying vec3 vN; varying vec3 vView;
      void main() {
        float edge = pow(1.0 - abs(dot(vN, vView)), 1.5);
        float fade = (1.0 - vY) * smoothstep(0.0, 0.02, vY);
        float pulse = 0.75 + 0.25 * sin(uTime * 2.0 - vY * 30.0);
        vec3 col = vec3(1.0, 0.78, 0.35) * (0.25 + edge) * fade * pulse;
        gl_FragColor = vec4(col * uAlpha, 1.0);
      }`,
  });
  beacon = new THREE.Mesh(geo, mat);
  beacon.frustumCulled = false;
  scene.add(beacon);
}

// ---------- camera rig ----------
const cam = { yaw: Math.PI * 0, pitch: 0.32, dist: 7.5, wantDist: 7.5, look: new THREE.Vector3(), shake: 0 };

function updateCamera(dt) {
  const p = game.player.pos;
  const want = new THREE.Vector3(p.x, p.y + 1.9, p.z);
  cam.look.lerp(want, Math.min(1, dt * 12));
  cam.dist += (cam.wantDist - cam.dist) * Math.min(1, dt * 8);
  const cp = Math.cos(cam.pitch);
  const pos = new THREE.Vector3(
    cam.look.x + Math.sin(cam.yaw) * cp * cam.dist,
    cam.look.y + Math.sin(cam.pitch) * cam.dist,
    cam.look.z + Math.cos(cam.yaw) * cp * cam.dist
  );
  // pull the camera in front of trees and walls between it and the hero
  const clear = game.world.physics.cameraClear(cam.look, pos);
  const full = cam.look.distanceTo(pos);
  if (clear < full) pos.lerpVectors(cam.look, pos, Math.max(1.2, clear) / full);
  cam.blocked = clear < full;
  const floor = terrainHeight(pos.x, pos.z) + 0.6;
  if (pos.y < floor) pos.y = floor;
  camera.position.copy(pos);
  if (cam.shake > 0) {
    const s = cam.shake * cam.shake * 0.35;
    camera.position.x += (Math.random() - 0.5) * s;
    camera.position.y += (Math.random() - 0.5) * s;
    cam.shake = Math.max(0, cam.shake - dt * 3);
  }
  camera.lookAt(cam.look);
}

function titleCamera(t) {
  const c = new THREE.Vector3(PLACES.spawn.x, terrainHeight(0, PLACES.spawn.z) + 1.6, PLACES.spawn.z);
  const a = t * 0.05 + 2.6;
  camera.position.set(c.x + Math.sin(a) * 11, c.y + 2.2 + Math.sin(t * 0.13) * 0.6, c.z + Math.cos(a) * 11);
  camera.lookAt(c.x, c.y + 1.2, c.z - 6);
}

// ---------- input ----------
const keys = new Set();
const pressed = new Set();
const mouse = { left: false, right: false, clicked: false, dragging: false };
let lockFailed = false;

addEventListener('keydown', (e) => {
  if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab'].includes(e.code)) e.preventDefault();
  audio.init();
  if (!keys.has(e.code)) pressed.add(e.code);
  keys.add(e.code);
  if (e.code === 'Escape' || e.code === 'KeyP') {
    if (game.state === 'playing') pause();
    else if (game.state === 'paused' && e.code === 'KeyP') resume();
  }
});
addEventListener('keyup', (e) => keys.delete(e.code));
addEventListener('blur', () => {
  keys.clear();
  mouse.left = mouse.right = false;
});

canvas.addEventListener('mousedown', (e) => {
  audio.init();
  if (game.state !== 'playing') return;
  if (!document.pointerLockElement && !lockFailed) {
    requestLock();
    return;
  }
  if (e.button === 0) {
    mouse.left = true;
    mouse.clicked = true;
  }
  if (e.button === 2) mouse.right = true;
  mouse.dragging = true;
});
addEventListener('mouseup', (e) => {
  if (e.button === 0) mouse.left = false;
  if (e.button === 2) mouse.right = false;
  mouse.dragging = false;
});
addEventListener('contextmenu', (e) => e.preventDefault());
addEventListener('mousemove', (e) => {
  if (game.state !== 'playing') return;
  if (document.pointerLockElement === canvas || (lockFailed && mouse.dragging)) {
    cam.yaw -= e.movementX * 0.0026;
    cam.pitch = THREE.MathUtils.clamp(cam.pitch + e.movementY * 0.0022, -0.35, 1.2);
  }
});
addEventListener('wheel', (e) => {
  if (game.state !== 'playing') return;
  cam.wantDist = THREE.MathUtils.clamp(cam.wantDist + Math.sign(e.deltaY) * 0.8, 3.5, 13);
}, { passive: true });

function requestLock() {
  try {
    const r = canvas.requestPointerLock();
    if (r && r.catch) r.catch(() => {
      lockFailed = true;
      ui.capture(false);
    });
  } catch {
    lockFailed = true;
    ui.capture(false);
  }
}
document.addEventListener('pointerlockchange', () => {
  const locked = document.pointerLockElement === canvas;
  ui.capture(!locked && game.state === 'playing' && !lockFailed);
  if (!locked && game.state === 'playing' && !lockFailed) pause();
});
document.addEventListener('pointerlockerror', () => {
  lockFailed = true;
  ui.capture(false);
});

function readInput() {
  const move = new THREE.Vector2(
    (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0),
    (keys.has('KeyW') ? 1 : 0) - (keys.has('KeyS') ? 1 : 0)
  );
  if (keys.has('ArrowLeft')) cam.yaw += 0.035;
  if (keys.has('ArrowRight')) cam.yaw -= 0.035;
  if (keys.has('ArrowUp')) move.y = 1;
  if (keys.has('ArrowDown')) move.y = -1;
  const input = {
    move,
    sprint: keys.has('ShiftLeft') || keys.has('ShiftRight'),
    jump: pressed.has('Space'),
    attack: mouse.clicked || pressed.has('KeyF') || pressed.has('KeyJ'),
    block: mouse.right || keys.has('KeyK'),
    dodge: pressed.has('KeyQ') || pressed.has('KeyC'),
    interact: pressed.has('KeyE'),
  };
  pressed.clear();
  mouse.clicked = false;
  return input;
}

// ---------- UI buttons ----------
const btn = (id, fn) =>
  document.getElementById(id).addEventListener('click', () => {
    audio.init();
    audio.click();
    fn();
  });
let controlsReturn = 'title';
btn('btn-start', () => ui.show('story'));
btn('btn-controls', () => {
  controlsReturn = 'title';
  ui.show('controls');
});
btn('btn-controls-back', () => ui.show(controlsReturn));
btn('btn-story', () => startPlaying(true));
btn('btn-resume', () => resume());
btn('btn-pause-controls', () => {
  controlsReturn = 'pause';
  ui.show('controls');
});
btn('btn-quit', () => location.reload());
btn('btn-respawn', () => respawn());
btn('btn-free', () => startPlaying(false));
btn('btn-again', () => location.reload());
for (const id of ['btn-sound', 'btn-pause-sound']) {
  btn(id, () => {
    audio.setEnabled(!audio.enabled);
    refreshSound();
  });
}
function refreshSound() {
  for (const id of ['btn-sound', 'btn-pause-sound']) document.getElementById(id).textContent = `Sound: ${audio.enabled ? 'On' : 'Off'}`;
}

// ---------- state changes ----------
let firstStart = true;
function startPlaying(fresh) {
  game.state = 'playing';
  ui.show(null);
  ui.setHud(true);
  if (fresh && firstStart) {
    firstStart = false;
    cam.yaw = 0;
    ui.toast('Your quest begins', 'Save the Kingdom');
    setTimeout(() => ui.hint('WASD to move · Mouse to look · Click to attack · Space to jump', 8), 1200);
  }
  if (!lockFailed) requestLock();
}

function pause() {
  if (game.state !== 'playing') return;
  game.state = 'paused';
  ui.capture(false);
  ui.show('pause');
  if (document.pointerLockElement) document.exitPointerLock();
}

function resume() {
  if (game.state !== 'paused') return;
  game.state = 'playing';
  ui.show(null);
  if (!lockFailed) requestLock();
}

function respawn() {
  game.player.reset(game.checkpoint.clone().setY(game.checkpoint.y + 0.2), cam.yaw + Math.PI);
  game.state = 'playing';
  ui.show(null);
  ui.setHud(true);
  game.fx.burst(game.player.pos.clone().setY(game.player.pos.y + 1), 'heal', 50);
  if (!lockFailed) requestLock();
}

game.questComplete = (title) => {
  ui.toast('Quest complete', title);
  audio.fanfare(false);
  setTimeout(() => {
    const next = game.quest.current();
    ui.hint(`New objective: ${next.title}`, 5);
  }, 3000);
};

game.openGate = () => {
  game.world.openGate();
  ui.toast('The Royal Key turns', 'The Gate Is Open');
  audio.rise();
  cam.shake = 0.8;
};

game.bossFight = (boss) => {
  boss.wake();
  game.enemies.alertGroup(boss);
  setTimeout(() => audio.roar(), 600);
  ui.toast('Mordrak awakens', 'The Bone King');
  cam.shake = 1;
};

game.victory = () => {
  game.world.liberate();
  audio.fanfare(true);
  ui.toast('Victory', 'The Kingdom Is Saved');
  setTimeout(() => {
    if (document.pointerLockElement) document.exitPointerLock();
    game.state = 'victory';
    const t = Math.floor(game.stats.time);
    document.getElementById('v-time').textContent = `${Math.floor(t / 60)}:${String(t % 60).padStart(2, '0')}`;
    document.getElementById('v-coins').textContent = game.stats.coins;
    document.getElementById('v-kills').textContent = game.stats.kills;
    document.getElementById('v-deaths').textContent = game.stats.deaths;
    ui.show('victory');
  }, 4200);
};

// ---------- gameplay events ----------
let hitstop = 0;
const up = (v, y) => v.clone().setY(v.y + y);

events.on('swing', (combo) => audio.swing(combo));
events.on('strike', ({ damage, combo }) => {
  const p = game.player;
  const hits = game.enemies.strike(p.pos, p.facing, 2.7, 1.15, damage);
  if (!hits.length) return;
  audio.hit(combo === 2);
  hitstop = combo === 2 ? 0.09 : 0.05;
  cam.shake = Math.max(cam.shake, combo === 2 ? 0.55 : 0.3);
  for (const e of hits) {
    const at = up(e.pos, 1.3 * e.scale);
    game.fx.burst(at, 'spark', 14);
    game.fx.burst(at, 'bone', 6);
    ui.floater(String(damage), at, camera, combo === 2 ? 'big' : '');
  }
});
events.on('jump', (double) => {
  audio.jump(double);
  game.fx.burst(game.player.pos, 'dust', double ? 6 : 10, { up: 0.3 });
  if (double) game.fx.ring(up(game.player.pos, 0.3), 'magic', 18, 3);
});
events.on('land', (v) => {
  audio.land(v);
  game.fx.burst(game.player.pos, 'dust', Math.min(20, v), { up: 0.2 });
});
events.on('step', (speed) => {
  audio.step(speed);
  if (speed > 7) game.fx.burst(game.player.pos, 'dust', 2, { up: 0.2, size: 0.7 });
});
events.on('dodge', () => {
  audio.dodge();
  game.fx.burst(game.player.pos, 'dust', 12, { up: 0.2 });
});
events.on('hurt', () => {
  audio.hurt();
  ui.hurt();
  cam.shake = Math.max(cam.shake, 0.5);
  game.fx.burst(up(game.player.pos, 1.3), 'spark', 10);
});
events.on('blocked', () => {
  audio.block();
  game.fx.burst(up(game.player.pos, 1.3).add(game.player.forward().multiplyScalar(0.6)), 'spark', 22, { speed: 1.4 });
});
events.on('died', () => {
  audio.death();
  game.stats.deaths++;
  setTimeout(() => {
    if (document.pointerLockElement) document.exitPointerLock();
    game.state = 'dead';
    ui.show('dead');
  }, 2600);
});
events.on('enemyDied', (e) => {
  audio.boneBreak();
  game.stats.kills++;
  game.fx.burst(up(e.pos, 1), 'bone', 30, { speed: 1.2 });
  game.fx.burst(e.pos, 'dust', 14, { up: 0.3 });
  setTimeout(() => game.pickups.coinBurst(e.pos, e.cfg.coins), 500);
  if (!e.cfg.boss && Math.random() < 0.22) setTimeout(() => game.pickups.add('potion', e.pos.x, e.pos.y + 1.2, e.pos.z, { vel: new THREE.Vector3(0, 5, 0) }), 700);
  if (e.cfg.boss) {
    cam.shake = 1.4;
    audio.roar();
    game.fx.burst(up(e.pos, 2), 'magic', 120, { speed: 2 });
  }
});
events.on('rise', (e) => {
  audio.rise();
  game.fx.burst(e.pos, 'dust', 16, { up: 0.4 });
});
events.on('cast', () => audio.cast());
events.on('bossSlam', (e) => {
  audio.slam();
  cam.shake = Math.max(cam.shake, 0.7);
  game.fx.ring(up(e.pos, 0.2), 'dust', 40, 9);
});
events.on('summon', (e) => {
  audio.cast();
  game.fx.ring(up(e.pos, 0.3), 'magic', 50, 8);
});
events.on('coin', () => {
  audio.coin();
  game.stats.coins++;
});
events.on('potion', () => {
  audio.potion();
  game.player.heal(40);
  ui.floater('+40', up(game.player.pos, 2.2), camera, 'heal');
});
events.on('key', () => {
  game.quest.hasKey = true;
  audio.fanfare(true);
  ui.toast('Treasure found', 'The Royal Key');
});
events.on('chestOpen', () => audio.chest());

// ---------- loop ----------
const clock = new THREE.Clock();
let musicCheck = 0;

function frame() {
  requestAnimationFrame(frame);
  const realDt = Math.min(clock.getDelta(), 1 / 20);
  const { world, player, enemies, pickups, quest, fx } = game;

  if (game.state === 'title') {
    titleCamera(world.time);
    world.update(realDt, player.pos, camera);
    player.anim.update(realDt);
    fx.update(realDt);
    fireFx(realDt);
    world.followShadow(player.pos);
    beacon.visible = false;
    composer.render();
    return;
  }

  let dt = realDt;
  if (hitstop > 0) {
    hitstop -= realDt;
    dt = realDt * 0.08;
  }
  const active = game.state === 'playing' || game.state === 'dead' || game.state === 'victory';
  if (game.state === 'paused' || game.state === 'story' || game.state === 'controls') dt = 0;

  if (active && dt > 0) {
    if (game.state === 'playing') game.stats.time += dt;
    const input = game.state === 'playing' ? readInput() : { move: new THREE.Vector2(), jump: false, attack: false, block: false, dodge: false, interact: false, sprint: false };

    // interact with chests
    const chest = player.alive ? pickups.nearestChest(player.pos) : null;
    ui.prompt(chest && game.state === 'playing' ? 'Open chest' : null);
    if (chest && input.interact) pickups.openChest(chest);

    player.update(dt, input, cam.yaw);
    enemies.update(dt, player);
    pickups.update(dt, player);
    quest.update();
    world.update(dt, player.pos, camera);
    fx.update(dt);
    fireFx(dt);

    // fell off the world (should not happen, but never leave the hero stranded)
    if (player.pos.y < terrainHeight(player.pos.x, player.pos.z) - 5) player.reset(game.checkpoint, player.facing);
  }

  updateCamera(realDt);
  world.followShadow(player.pos);

  // objective beacon
  const obj = quest.objective();
  if (obj) {
    beacon.visible = true;
    const by = quest.stage === 2 ? world.shrine.y : terrainHeight(obj.x, obj.z);
    beacon.position.set(obj.x, by, obj.z);
    const d = Math.hypot(player.pos.x - obj.x, player.pos.z - obj.z);
    beacon.material.uniforms.uAlpha.value = THREE.MathUtils.smoothstep(d, 8, 30);
    beacon.material.uniforms.uTime.value = world.time;
  } else beacon.visible = false;

  // HUD
  ui.health(player.hp, player.maxHp);
  ui.coins(game.stats.coins, pickups.totalCoins);
  const stage = quest.done ? null : quest.current();
  if (stage) ui.quest(stage.title, stage.desc, stage.progress());
  else ui.quest('Eldoria Is Free', 'The kingdom is yours to explore. Find every last coin.', '');
  ui.boss(quest.bossAwake && quest.boss.alive ? quest.boss : null);
  ui.update(realDt, camera, enemies, player);
  ui.minimap(player, cam.yaw, enemies, obj);

  musicCheck -= realDt;
  if (musicCheck <= 0) {
    musicCheck = 0.5;
    let level = 0;
    if (quest.bossAwake && quest.boss.alive) level = 2;
    else if (enemies.list.some((e) => e.alive && (e.state === 'chase' || e.state === 'attack') && e.pos.distanceTo(player.pos) < 28)) level = 1;
    audio.setIntensity(level);
  }

  composer.render();
}

let fireAcc = 0;
function fireFx(dt) {
  fireAcc += dt;
  if (fireAcc < 1 / 30) return;
  fireAcc = 0;
  for (const f of game.world.fires) {
    if (f.pos.distanceTo(camera.position) > 90) continue;
    game.fx.fire(f.pos, f.big);
    if (f.big) game.fx.fire(f.pos, true);
  }
}

// ---------- resize ----------
function resize() {
  const w = innerWidth, h = innerHeight;
  const pr = Math.min(devicePixelRatio, 1.75);
  renderer.setPixelRatio(pr);
  renderer.setSize(w, h);
  composer.setPixelRatio(pr);
  composer.setSize(w, h);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  game.fx?.setPixelScale(pr * (h / 900));
}
addEventListener('resize', resize);
