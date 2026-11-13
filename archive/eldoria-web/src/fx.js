import * as THREE from 'three';

class Pool {
  constructor(scene, max, blending) {
    this.max = max;
    this.cursor = 0;
    this.pos = new Float32Array(max * 3);
    this.col = new Float32Array(max * 4);
    this.size = new Float32Array(max);
    this.vel = new Float32Array(max * 3);
    this.life = new Float32Array(max);
    this.maxLife = new Float32Array(max);
    this.grav = new Float32Array(max);
    this.drag = new Float32Array(max);
    this.base = new Float32Array(max);
    this.alpha0 = new Float32Array(max);
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('color', new THREE.BufferAttribute(this.col, 4).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('size', new THREE.BufferAttribute(this.size, 1).setUsage(THREE.DynamicDrawUsage));
    this.geo = geo;
    this.material = new THREE.ShaderMaterial({
      transparent: true,
      depthWrite: false,
      blending,
      uniforms: { uScale: { value: 1 } },
      vertexShader: /* glsl */ `
        attribute float size; attribute vec4 color;
        uniform float uScale;
        varying vec4 vColor;
        void main() {
          vColor = color;
          vec4 mv = modelViewMatrix * vec4(position, 1.0);
          gl_PointSize = size * uScale * (300.0 / max(-mv.z, 0.1));
          gl_Position = projectionMatrix * mv;
        }`,
      fragmentShader: /* glsl */ `
        varying vec4 vColor;
        void main() {
          float d = length(gl_PointCoord - 0.5);
          float a = smoothstep(0.5, 0.1, d);
          if (vColor.a * a < 0.003) discard;
          gl_FragColor = vec4(vColor.rgb, vColor.a * a);
        }`,
    });
    const pts = new THREE.Points(geo, this.material);
    pts.frustumCulled = false;
    scene.add(pts);
  }

  spawn(p, v, color, alpha, size, life, grav, drag) {
    const i = this.cursor;
    this.cursor = (i + 1) % this.max;
    this.pos.set([p.x, p.y, p.z], i * 3);
    this.vel.set([v.x, v.y, v.z], i * 3);
    this.col.set([color.r, color.g, color.b, alpha], i * 4);
    this.alpha0[i] = alpha;
    this.base[i] = size;
    this.size[i] = size;
    this.life[i] = this.maxLife[i] = life;
    this.grav[i] = grav;
    this.drag[i] = drag;
  }

  update(dt) {
    for (let i = 0; i < this.max; i++) {
      if (this.life[i] <= 0) continue;
      this.life[i] -= dt;
      if (this.life[i] <= 0) {
        this.col[i * 4 + 3] = 0;
        continue;
      }
      const k = Math.max(0, 1 - this.drag[i] * dt);
      const i3 = i * 3;
      this.vel[i3] *= k;
      this.vel[i3 + 1] = this.vel[i3 + 1] * k - this.grav[i] * dt;
      this.vel[i3 + 2] *= k;
      this.pos[i3] += this.vel[i3] * dt;
      this.pos[i3 + 1] += this.vel[i3 + 1] * dt;
      this.pos[i3 + 2] += this.vel[i3 + 2] * dt;
      const t = this.life[i] / this.maxLife[i];
      this.col[i * 4 + 3] = this.alpha0[i] * Math.min(1, t * 2);
      this.size[i] = this.base[i] * (0.5 + 0.5 * t);
    }
    this.geo.attributes.position.needsUpdate = true;
    this.geo.attributes.color.needsUpdate = true;
    this.geo.attributes.size.needsUpdate = true;
  }
}

const PRESETS = {
  spark: { color: new THREE.Color(2.2, 1.8, 1.0), speed: 7, size: 0.35, life: 0.35, grav: 10, drag: 3, glow: true },
  magic: { color: new THREE.Color(1.6, 0.6, 2.4), speed: 5, size: 0.5, life: 0.6, grav: -1, drag: 2.5, glow: true },
  coin: { color: new THREE.Color(2.4, 1.8, 0.4), speed: 3.5, size: 0.35, life: 0.6, grav: -2, drag: 3, glow: true },
  heal: { color: new THREE.Color(0.6, 2.2, 0.8), speed: 2.5, size: 0.4, life: 0.9, grav: -3, drag: 2, glow: true },
  bone: { color: new THREE.Color(0.92, 0.9, 0.82), speed: 5, size: 0.3, life: 0.9, grav: 14, drag: 1.5, glow: false, alpha: 1 },
  dust: { color: new THREE.Color(0.72, 0.64, 0.5), speed: 2.2, size: 0.9, life: 0.7, grav: -0.6, drag: 3, glow: false, alpha: 0.35 },
  smoke: { color: new THREE.Color(0.35, 0.32, 0.3), speed: 1.2, size: 2.2, life: 2.4, grav: -1.2, drag: 0.6, glow: false, alpha: 0.18 },
  fire: { color: new THREE.Color(2.4, 1.0, 0.25), speed: 1, size: 0.55, life: 0.7, grav: -3.2, drag: 1, glow: true },
};

const v = new THREE.Vector3();

export class FX {
  constructor(scene) {
    this.glow = new Pool(scene, 4000, THREE.AdditiveBlending);
    this.soft = new Pool(scene, 2000, THREE.NormalBlending);
  }

  setPixelScale(s) {
    this.glow.material.uniforms.uScale.value = s;
    this.soft.material.uniforms.uScale.value = s;
  }

  burst(p, kind, n, opts = {}) {
    const pr = PRESETS[kind];
    const pool = pr.glow ? this.glow : this.soft;
    for (let i = 0; i < n; i++) {
      v.set(Math.random() * 2 - 1, Math.random() * (opts.up ?? 1), Math.random() * 2 - 1).normalize().multiplyScalar(pr.speed * (0.3 + Math.random() * 0.8) * (opts.speed ?? 1));
      pool.spawn(p, v, pr.color, pr.alpha ?? 1, pr.size * (0.6 + Math.random() * 0.8) * (opts.size ?? 1), pr.life * (0.6 + Math.random() * 0.7), pr.grav, pr.drag);
    }
  }

  ring(p, kind, n, radius) {
    const pr = PRESETS[kind];
    const pool = pr.glow ? this.glow : this.soft;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2;
      v.set(Math.cos(a) * radius, 0.4, Math.sin(a) * radius);
      pool.spawn(p, v, pr.color, pr.alpha ?? 1, pr.size * 1.2, pr.life, 0, 1.5);
    }
  }

  trail(p) {
    v.set((Math.random() - 0.5) * 0.4, (Math.random() - 0.5) * 0.4, (Math.random() - 0.5) * 0.4);
    this.glow.spawn(p, v, PRESETS.magic.color, 1, 0.45, 0.35, 0, 1);
  }

  fire(p, big) {
    const pr = PRESETS.fire;
    v.set((Math.random() - 0.5) * 0.5, 0.5 + Math.random(), (Math.random() - 0.5) * 0.5);
    const q = new THREE.Vector3(p.x + (Math.random() - 0.5) * (big ? 1.2 : 0.2), p.y, p.z + (Math.random() - 0.5) * (big ? 1.2 : 0.2));
    this.glow.spawn(q, v, pr.color, 1, pr.size * (big ? 2 : 0.8), pr.life * (big ? 1.3 : 0.6), pr.grav, pr.drag);
    if (big && Math.random() < 0.15) this.soft.spawn(q.setY(q.y + 1.5), v.set(0, 1, 0), PRESETS.smoke.color, 0.18, 2.2, 2.4, -0.6, 0.4);
  }

  update(dt) {
    this.glow.update(dt);
    this.soft.update(dt);
  }
}
