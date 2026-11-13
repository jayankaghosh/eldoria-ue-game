import * as THREE from 'three';

// GPU point sprites with CPU-side simulation. Additive, so they glow under bloom.
export class Particles {
  constructor(scene, max = 3000) {
    this.max = max;
    this.cursor = 0;
    this.pos = new Float32Array(max * 3);
    this.col = new Float32Array(max * 3);
    this.size = new Float32Array(max);
    this.alpha = new Float32Array(max);
    this.vel = new Float32Array(max * 3);
    this.life = new Float32Array(max);
    this.maxLife = new Float32Array(max);
    this.drag = new Float32Array(max);
    this.gravity = new Float32Array(max);
    this.baseSize = new Float32Array(max);
    this.world = new Uint8Array(max); // 1 = scrolls with the world

    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('color', new THREE.BufferAttribute(this.col, 3).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('size', new THREE.BufferAttribute(this.size, 1).setUsage(THREE.DynamicDrawUsage));
    geo.setAttribute('alpha', new THREE.BufferAttribute(this.alpha, 1).setUsage(THREE.DynamicDrawUsage));
    this.geo = geo;

    const mat = new THREE.ShaderMaterial({
      transparent: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      uniforms: { uScale: { value: 1 } },
      vertexShader: /* glsl */ `
        attribute float size; attribute float alpha; attribute vec3 color;
        uniform float uScale;
        varying vec3 vColor; varying float vAlpha;
        void main() {
          vColor = color; vAlpha = alpha;
          vec4 mv = modelViewMatrix * vec4(position, 1.0);
          gl_PointSize = size * uScale * (300.0 / max(-mv.z, 0.1));
          gl_Position = projectionMatrix * mv;
        }`,
      fragmentShader: /* glsl */ `
        varying vec3 vColor; varying float vAlpha;
        void main() {
          float d = length(gl_PointCoord - 0.5);
          float a = smoothstep(0.5, 0.0, d);
          a *= a;
          if (vAlpha <= 0.001) discard;
          gl_FragColor = vec4(vColor * a * vAlpha, 1.0);
        }`,
    });
    this.material = mat;
    this.points = new THREE.Points(geo, mat);
    this.points.frustumCulled = false;
    scene.add(this.points);
  }

  setPixelScale(s) {
    this.material.uniforms.uScale.value = s;
  }

  spawn(x, y, z, vx, vy, vz, color, size, life, opts = {}) {
    const i = this.cursor;
    this.cursor = (this.cursor + 1) % this.max;
    const i3 = i * 3;
    this.pos[i3] = x; this.pos[i3 + 1] = y; this.pos[i3 + 2] = z;
    this.vel[i3] = vx; this.vel[i3 + 1] = vy; this.vel[i3 + 2] = vz;
    this.col[i3] = color.r; this.col[i3 + 1] = color.g; this.col[i3 + 2] = color.b;
    this.baseSize[i] = size;
    this.size[i] = size;
    this.life[i] = life;
    this.maxLife[i] = life;
    this.alpha[i] = 1;
    this.drag[i] = opts.drag ?? 0;
    this.gravity[i] = opts.gravity ?? 0;
    this.world[i] = opts.world ? 1 : 0;
  }

  burst(x, y, z, color, count, speed, size, life, opts = {}) {
    for (let k = 0; k < count; k++) {
      const u = Math.random() * 2 - 1;
      const th = Math.random() * Math.PI * 2;
      const s = Math.sqrt(1 - u * u);
      const m = speed * (0.35 + Math.random() * 0.65);
      this.spawn(
        x, y, z,
        s * Math.cos(th) * m, Math.abs(u) * m * (opts.up ?? 1), s * Math.sin(th) * m,
        color, size * (0.6 + Math.random() * 0.8), life * (0.6 + Math.random() * 0.6), opts
      );
    }
  }

  update(dt, worldSpeed) {
    for (let i = 0; i < this.max; i++) {
      if (this.life[i] <= 0) continue;
      this.life[i] -= dt;
      const i3 = i * 3;
      if (this.life[i] <= 0) {
        this.alpha[i] = 0;
        continue;
      }
      const dr = Math.max(0, 1 - this.drag[i] * dt);
      this.vel[i3] *= dr;
      this.vel[i3 + 1] = this.vel[i3 + 1] * dr - this.gravity[i] * dt;
      this.vel[i3 + 2] *= dr;
      this.pos[i3] += this.vel[i3] * dt;
      this.pos[i3 + 1] += this.vel[i3 + 1] * dt;
      this.pos[i3 + 2] += (this.vel[i3 + 2] + (this.world[i] ? worldSpeed : 0)) * dt;
      if (this.pos[i3 + 1] < 0.02) {
        this.pos[i3 + 1] = 0.02;
        this.vel[i3 + 1] *= -0.4;
      }
      const t = this.life[i] / this.maxLife[i];
      this.alpha[i] = Math.min(1, t * 1.6);
      this.size[i] = this.baseSize[i] * (0.4 + 0.6 * t);
    }
    const a = this.geo.attributes;
    a.position.needsUpdate = true;
    a.color.needsUpdate = true;
    a.size.needsUpdate = true;
    a.alpha.needsUpdate = true;
  }

  clear() {
    this.life.fill(0);
    this.alpha.fill(0);
  }
}

// Expanding ground ring used for pickups and explosions
export class Shockwaves {
  constructor(scene) {
    this.scene = scene;
    this.pool = [];
    this.geo = new THREE.RingGeometry(0.85, 1, 48);
    this.geo.rotateX(-Math.PI / 2);
  }

  spawn(x, y, z, color, maxScale = 8, life = 0.6) {
    let w = this.pool.find((p) => !p.active);
    if (!w) {
      const mesh = new THREE.Mesh(
        this.geo,
        new THREE.MeshBasicMaterial({ transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide })
      );
      w = { mesh, active: false };
      this.scene.add(mesh);
      this.pool.push(w);
    }
    w.active = true;
    w.t = 0;
    w.life = life;
    w.maxScale = maxScale;
    w.mesh.visible = true;
    w.mesh.position.set(x, y, z);
    w.mesh.material.color.copy(color);
  }

  update(dt, worldSpeed) {
    for (const w of this.pool) {
      if (!w.active) continue;
      w.t += dt;
      const k = w.t / w.life;
      if (k >= 1) {
        w.active = false;
        w.mesh.visible = false;
        continue;
      }
      const s = 0.5 + (1 - Math.pow(1 - k, 3)) * w.maxScale;
      w.mesh.scale.setScalar(s);
      w.mesh.material.opacity = 1 - k;
      w.mesh.position.z += worldSpeed * dt;
    }
  }

  clear() {
    for (const w of this.pool) {
      w.active = false;
      w.mesh.visible = false;
    }
  }
}
