import * as THREE from 'three';
import { TRACK_HALF } from './world.js';

const HOVER = 0.55;
const JUMP_V = 11.5;
const GRAVITY = 32;
const MAX_LAT = 15;
export const PLAYER_LIMIT = TRACK_HALF - 1.1;

const ENGINE_COLOR = new THREE.Color('#ff4fd8').multiplyScalar(2.2);
const ENGINE_CORE = new THREE.Color('#ffffff').multiplyScalar(1.4);
const BOOST_COLOR = new THREE.Color('#ffe14d').multiplyScalar(2.4);

export class Player {
  constructor(scene) {
    this.group = new THREE.Group();
    this.ship = new THREE.Group();
    this.group.add(this.ship);
    scene.add(this.group);

    this.x = 0;
    this.y = 0; // height above hover line
    this.vx = 0;
    this.vy = 0;
    this.targetX = null;
    this.grounded = true;
    this.time = 0;
    this.halfW = 0.95;
    this.halfD = 1.3;

    this.buildShip();
    this.buildShield();
  }

  buildShip() {
    // Top-down arrow silhouette, extruded into a slim hull
    const s = new THREE.Shape();
    s.moveTo(0, -2.0);
    s.lineTo(0.42, -0.9);
    s.lineTo(1.25, 0.55);
    s.lineTo(1.3, 1.05);
    s.lineTo(0.55, 0.9);
    s.lineTo(0.35, 1.2);
    s.lineTo(-0.35, 1.2);
    s.lineTo(-0.55, 0.9);
    s.lineTo(-1.3, 1.05);
    s.lineTo(-1.25, 0.55);
    s.lineTo(-0.42, -0.9);
    s.closePath();
    const hullGeo = new THREE.ExtrudeGeometry(s, {
      depth: 0.32,
      bevelEnabled: true,
      bevelThickness: 0.08,
      bevelSize: 0.08,
      bevelSegments: 2,
    });
    hullGeo.rotateX(Math.PI / 2);
    hullGeo.translate(0, 0.2, 0);

    const hullMat = new THREE.MeshStandardMaterial({
      color: 0x241246,
      metalness: 0.75,
      roughness: 0.28,
      emissive: 0x1a0638,
      emissiveIntensity: 0.6,
    });
    const hull = new THREE.Mesh(hullGeo, hullMat);
    this.ship.add(hull);

    const edges = new THREE.LineSegments(
      new THREE.EdgesGeometry(hullGeo, 25),
      new THREE.LineBasicMaterial({ color: new THREE.Color('#2ff3ff').multiplyScalar(2.2) })
    );
    this.ship.add(edges);
    this.edges = edges;

    // Cockpit canopy
    const canopy = new THREE.Mesh(
      new THREE.SphereGeometry(0.42, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2),
      new THREE.MeshStandardMaterial({
        color: 0x5b2bff,
        emissive: 0x6a3bff,
        emissiveIntensity: 0.35,
        metalness: 0.2,
        roughness: 0.05,
        transparent: true,
        opacity: 0.9,
      })
    );
    canopy.scale.set(0.85, 0.7, 1.8);
    canopy.position.set(0, 0.32, -0.15);
    this.ship.add(canopy);

    // Fins
    const finGeo = new THREE.BoxGeometry(0.06, 0.45, 0.6);
    finGeo.translate(0, 0.22, 0);
    const finMat = new THREE.MeshStandardMaterial({ color: 0x2a1450, metalness: 0.7, roughness: 0.3 });
    for (const side of [-1, 1]) {
      const fin = new THREE.Mesh(finGeo, finMat);
      fin.position.set(side * 1.05, 0.25, 0.75);
      fin.rotation.z = side * -0.25;
      this.ship.add(fin);
      const tip = new THREE.Mesh(
        new THREE.BoxGeometry(0.08, 0.06, 0.62),
        new THREE.MeshBasicMaterial({ color: new THREE.Color('#ff2fd1').multiplyScalar(2.5) })
      );
      tip.position.set(side * 1.15, 0.7, 0.75);
      this.ship.add(tip);
    }

    // Engines
    this.engineMats = [];
    this.engines = [];
    const engGeo = new THREE.CylinderGeometry(0.2, 0.24, 0.5, 16);
    engGeo.rotateX(Math.PI / 2);
    const engMat = new THREE.MeshStandardMaterial({ color: 0x150a2a, metalness: 0.9, roughness: 0.25 });
    for (const side of [-1, 1]) {
      const e = new THREE.Mesh(engGeo, engMat);
      e.position.set(side * 0.38, 0.22, 1.3);
      this.ship.add(e);
      const glowMat = new THREE.MeshBasicMaterial({ color: ENGINE_COLOR.clone() });
      const glow = new THREE.Mesh(new THREE.CircleGeometry(0.17, 20), glowMat);
      glow.position.set(side * 0.38, 0.22, 1.56);
      this.ship.add(glow);
      this.engineMats.push(glowMat);
      this.engines.push(new THREE.Vector3(side * 0.38, 0.22, 1.6));
    }

    // Underglow on the road
    const c = document.createElement('canvas');
    c.width = c.height = 128;
    const g = c.getContext('2d');
    const grad = g.createRadialGradient(64, 64, 0, 64, 64, 64);
    grad.addColorStop(0, 'rgba(255,255,255,1)');
    grad.addColorStop(0.4, 'rgba(255,255,255,0.35)');
    grad.addColorStop(1, 'rgba(255,255,255,0)');
    g.fillStyle = grad;
    g.fillRect(0, 0, 128, 128);
    const tex = new THREE.CanvasTexture(c);
    this.glowMat = new THREE.MeshBasicMaterial({
      map: tex,
      color: new THREE.Color('#ff2fd1').multiplyScalar(0.9),
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
    });
    this.underglow = new THREE.Mesh(new THREE.PlaneGeometry(3.6, 5), this.glowMat);
    this.underglow.rotation.x = -Math.PI / 2;
    this.underglow.position.y = 0.03;
    this.group.add(this.underglow);
  }

  buildShield() {
    this.shieldMat = new THREE.ShaderMaterial({
      transparent: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      uniforms: { uTime: { value: 0 }, uAlpha: { value: 0 } },
      vertexShader: /* glsl */ `
        varying vec3 vN; varying vec3 vView; varying vec3 vPos;
        void main() {
          vN = normalize(normalMatrix * normal);
          vec4 mv = modelViewMatrix * vec4(position, 1.0);
          vView = normalize(-mv.xyz);
          vPos = position;
          gl_Position = projectionMatrix * mv;
        }`,
      fragmentShader: /* glsl */ `
        uniform float uTime; uniform float uAlpha;
        varying vec3 vN; varying vec3 vView; varying vec3 vPos;
        void main() {
          float fres = pow(1.0 - abs(dot(vN, vView)), 2.5);
          float hex = 0.5 + 0.5 * sin(vPos.y * 14.0 - uTime * 6.0);
          vec3 col = vec3(0.4, 2.0, 0.9) * (fres * 1.4 + hex * 0.12);
          gl_FragColor = vec4(col * uAlpha, 1.0);
        }`,
    });
    this.shield = new THREE.Mesh(new THREE.SphereGeometry(1.9, 32, 16), this.shieldMat);
    this.shield.scale.set(1, 0.6, 1.25);
    this.shield.position.y = 0.3;
    this.ship.add(this.shield);
    this.shieldLevel = 0;
  }

  reset() {
    this.x = 0;
    this.y = 0;
    this.vx = 0;
    this.vy = 0;
    this.targetX = null;
    this.grounded = true;
    this.group.visible = true;
    this.group.position.set(0, 0, 0);
    this.ship.rotation.set(0, 0, 0);
  }

  jump() {
    if (!this.grounded) return false;
    this.vy = JUMP_V;
    this.grounded = false;
    return true;
  }

  // steer: -1..1 from keyboard; targetX: from touch drag (overrides when set)
  update(dt, steer, opts = {}) {
    this.time += dt;
    let desiredVx;
    if (this.targetX !== null && steer === 0) {
      desiredVx = THREE.MathUtils.clamp((this.targetX - this.x) * 9, -MAX_LAT * 1.3, MAX_LAT * 1.3);
    } else {
      desiredVx = steer * MAX_LAT;
    }
    const accel = steer === 0 && this.targetX === null ? 10 : 14;
    this.vx += (desiredVx - this.vx) * Math.min(1, accel * dt);
    this.x += this.vx * dt;
    if (this.x > PLAYER_LIMIT) {
      this.x = PLAYER_LIMIT;
      this.vx = Math.min(this.vx, 0);
    } else if (this.x < -PLAYER_LIMIT) {
      this.x = -PLAYER_LIMIT;
      this.vx = Math.max(this.vx, 0);
    }

    if (!this.grounded) {
      this.vy -= GRAVITY * dt;
      this.y += this.vy * dt;
      if (this.y <= 0) {
        this.y = 0;
        this.vy = 0;
        this.grounded = true;
        this.landed = true;
      }
    }

    const bob = Math.sin(this.time * 3.2) * 0.06;
    this.group.position.x = this.x;
    this.ship.position.y = HOVER + this.y + bob;
    const bank = -this.vx * 0.045;
    this.ship.rotation.z += (bank - this.ship.rotation.z) * Math.min(1, 10 * dt);
    this.ship.rotation.y += (-this.vx * 0.012 - this.ship.rotation.y) * Math.min(1, 10 * dt);
    const pitch = this.grounded ? 0 : THREE.MathUtils.clamp(this.vy * 0.025, -0.2, 0.25);
    this.ship.rotation.x += (pitch - this.ship.rotation.x) * Math.min(1, 8 * dt);

    // underglow fades with height
    const h = this.y;
    this.underglow.scale.setScalar(1 + h * 0.25);
    this.glowMat.opacity = 0.9 / (1 + h * 0.8);

    // engines
    const boost = opts.boost ?? 0;
    const flicker = 0.85 + Math.random() * 0.3;
    for (const m of this.engineMats) {
      m.color.copy(ENGINE_COLOR).lerp(BOOST_COLOR, boost).multiplyScalar(flicker);
    }

    // shield visual
    const target = opts.shield ? 1 : 0;
    this.shieldLevel += (target - this.shieldLevel) * Math.min(1, 6 * dt);
    this.shieldMat.uniforms.uAlpha.value = this.shieldLevel * (opts.invulnBlink ? 0.4 + 0.6 * Math.abs(Math.sin(this.time * 25)) : 1);
    this.shieldMat.uniforms.uTime.value = this.time;
    this.shield.visible = this.shieldLevel > 0.01;

    // blink hull when temporarily invulnerable without shield
    this.ship.visible = !(opts.invulnBlink && !opts.shield && Math.sin(this.time * 40) > 0.3);
  }

  emitTrail(particles, speed, boost) {
    const col = boost > 0.5 ? BOOST_COLOR : ENGINE_COLOR;
    const tmp = new THREE.Vector3();
    for (const e of this.engines) {
      tmp.copy(e);
      this.ship.localToWorld(tmp);
      for (let k = 0; k < 3; k++) {
        particles.spawn(
          tmp.x + (Math.random() - 0.5) * 0.1,
          tmp.y + (Math.random() - 0.5) * 0.1,
          tmp.z + Math.random() * speed / 60,
          (Math.random() - 0.5) * 0.6,
          (Math.random() - 0.5) * 0.6,
          speed * 0.15 + 4,
          Math.random() < 0.3 ? ENGINE_CORE : col,
          0.55 + boost * 0.4,
          0.32 + boost * 0.15,
          { world: true, drag: 1.5 }
        );
      }
    }
  }

  worldPos(out) {
    return this.ship.getWorldPosition(out);
  }
}
