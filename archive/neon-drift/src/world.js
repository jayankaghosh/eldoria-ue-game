import * as THREE from 'three';

// Shared palette (linear values come from THREE.Color conversion)
export const COLORS = {
  horizon: new THREE.Color('#ff3fa4'),
  skyTop: new THREE.Color('#05020f'),
  skyMid: new THREE.Color('#2a0b4f'),
  fog: new THREE.Color('#1a0733'),
  grid: new THREE.Color('#ff2fd1'),
  road: new THREE.Color('#2ff3ff'),
};

export const TRACK_HALF = 6.5;

const MOUNTAIN_LEN = 200;
const POST_SPACING = 12;
const POST_COUNT = 26;

export class World {
  constructor(scene) {
    this.scene = scene;
    this.offset = 0;
    this.time = 0;

    scene.background = COLORS.skyTop.clone();
    scene.fog = new THREE.Fog(COLORS.fog, 90, 330);

    this.buildSky();
    this.buildSun();
    this.buildFloor();
    this.buildMountains();
    this.buildPosts();
    this.buildStars();

    scene.add(new THREE.HemisphereLight(0xb18cff, 0x2a0b4f, 1.1));
    const key = new THREE.DirectionalLight(0xff9be8, 2.2);
    key.position.set(-4, 10, -12);
    scene.add(key);
    const rim = new THREE.DirectionalLight(0x2ff3ff, 1.6);
    rim.position.set(6, 4, 10);
    scene.add(rim);
  }

  buildSky() {
    const geo = new THREE.SphereGeometry(900, 32, 16);
    const mat = new THREE.ShaderMaterial({
      side: THREE.BackSide,
      depthWrite: false,
      fog: false,
      uniforms: {
        uTop: { value: COLORS.skyTop },
        uMid: { value: COLORS.skyMid },
        uHorizon: { value: COLORS.horizon },
      },
      vertexShader: /* glsl */ `
        varying vec3 vDir;
        void main() {
          vDir = normalize(position);
          gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }`,
      fragmentShader: /* glsl */ `
        uniform vec3 uTop; uniform vec3 uMid; uniform vec3 uHorizon;
        varying vec3 vDir;
        void main() {
          float h = vDir.y;
          vec3 col = mix(uMid, uTop, smoothstep(0.05, 0.55, h));
          col = mix(uHorizon * 0.55, col, smoothstep(-0.02, 0.16, h));
          // faint horizontal scan bands near the horizon
          col += uHorizon * 0.05 * smoothstep(0.2, 0.0, h) * (0.5 + 0.5 * sin(h * 400.0));
          gl_FragColor = vec4(col, 1.0);
        }`,
    });
    this.sky = new THREE.Mesh(geo, mat);
    this.sky.renderOrder = -10;
    this.scene.add(this.sky);
  }

  buildSun() {
    const mat = new THREE.ShaderMaterial({
      transparent: true,
      depthWrite: false,
      fog: false,
      uniforms: { uTime: { value: 0 } },
      vertexShader: /* glsl */ `
        varying vec2 vUv;
        void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
      fragmentShader: /* glsl */ `
        uniform float uTime;
        varying vec2 vUv;
        void main() {
          vec2 p = vUv * 2.0 - 1.0;
          float r = length(p);
          float disc = smoothstep(0.62, 0.6, r);
          float glow = exp(-max(r - 0.55, 0.0) * 6.0) * 0.28;
          // classic stripe cut-outs in the lower half, scrolling slowly down
          float y = (p.y + 0.6) / 1.2;
          float bands = step(0.0, -p.y + 0.08);
          float stripe = fract((p.y + uTime * 0.04) * 9.0);
          float thick = mix(0.05, 0.45, clamp(-p.y / 0.6, 0.0, 1.0));
          float cut = bands * step(stripe, thick);
          vec3 top = vec3(1.25, 0.85, 0.12);
          vec3 bottom = vec3(1.2, 0.06, 0.55);
          vec3 col = mix(bottom, top, smoothstep(0.0, 1.0, y));
          float a = disc * (1.0 - cut);
          vec3 outCol = col * a + vec3(0.9, 0.12, 0.55) * glow * (1.0 - a);
          gl_FragColor = vec4(outCol, max(a, glow));
        }`,
    });
    this.sun = new THREE.Mesh(new THREE.PlaneGeometry(320, 320), mat);
    this.sun.position.set(0, 88, -720);
    this.sun.renderOrder = -9;
    this.scene.add(this.sun);
  }

  buildFloor() {
    const geo = new THREE.PlaneGeometry(700, 900, 1, 1);
    geo.rotateX(-Math.PI / 2);
    const mat = new THREE.ShaderMaterial({
      fog: false,
      uniforms: {
        uOffset: { value: 0 },
        uGrid: { value: COLORS.grid },
        uRoad: { value: COLORS.road },
        uFog: { value: COLORS.fog },
        uHorizon: { value: COLORS.horizon },
        uHalf: { value: TRACK_HALF },
        uBoost: { value: 0 },
      },
      vertexShader: /* glsl */ `
        varying vec3 vWorld;
        void main() {
          vec4 w = modelMatrix * vec4(position, 1.0);
          vWorld = w.xyz;
          gl_Position = projectionMatrix * viewMatrix * w;
        }`,
      fragmentShader: /* glsl */ `
        uniform float uOffset; uniform vec3 uGrid; uniform vec3 uRoad; uniform vec3 uFog; uniform vec3 uHorizon;
        uniform float uHalf; uniform float uBoost;
        varying vec3 vWorld;

        float gridLine(vec2 p, float cell, float width) {
          vec2 q = p / cell;
          vec2 d = abs(fract(q - 0.5) - 0.5) * cell;
          vec2 fw = fwidth(p) * 1.2;
          vec2 l = 1.0 - smoothstep(vec2(width), vec2(width) + fw, d);
          return max(l.x, l.y);
        }

        void main() {
          vec2 p = vec2(vWorld.x, vWorld.z - uOffset);
          float ax = abs(vWorld.x);
          float dist = length(vWorld - cameraPosition);

          vec3 col = vec3(0.012, 0.004, 0.03);

          // outer neon grid
          float g = gridLine(p, 5.0, 0.05);
          float glow = 0.0;
          {
            vec2 q = p / 5.0;
            vec2 d = abs(fract(q - 0.5) - 0.5) * 5.0;
            glow = exp(-min(d.x, d.y) * 3.0);
          }
          float outside = smoothstep(uHalf + 0.6, uHalf + 1.2, ax);
          float near = 1.0 - smoothstep(20.0, 240.0, dist);
          col += uGrid * (g * 1.8 + glow * 0.18) * outside * (0.35 + near * 1.1);

          // road surface
          float road = 1.0 - outside;
          col = mix(col, vec3(0.02, 0.01, 0.05), road * 0.85);
          float edge = 1.0 - smoothstep(0.08, 0.08 + fwidth(ax) * 1.5, abs(ax - uHalf));
          float edgeGlow = exp(-abs(ax - uHalf) * 2.5);
          col += uRoad * (edge * 2.6 + edgeGlow * 0.35);
          // lane dashes
          float dash = step(fract(p.y / 7.0), 0.45);
          float lane = 1.0 - smoothstep(0.05, 0.05 + fwidth(ax) * 1.5, abs(ax - 2.0));
          col += uRoad * lane * dash * 0.9 * road;
          // speed streaks on the road when boosting
          float streak = step(0.985, fract(sin(floor(vWorld.x * 3.0) * 91.7) * 4375.5)) * step(fract(p.y / 23.0 + vWorld.x), 0.3);
          col += vec3(1.5, 1.2, 0.4) * streak * uBoost * road;

          // fade into horizon haze
          float fog = smoothstep(90.0, 420.0, dist);
          vec3 haze = mix(uFog, uHorizon * 0.5, smoothstep(250.0, 450.0, dist));
          col = mix(col, haze, fog);
          gl_FragColor = vec4(col, 1.0);
        }`,
    });
    this.floor = new THREE.Mesh(geo, mat);
    this.floor.position.z = -350;
    this.scene.add(this.floor);
  }

  buildMountains() {
    // Heights are periodic along z so chunks tile seamlessly
    const makeChunk = (side) => {
      const width = 150;
      const geo = new THREE.PlaneGeometry(width, MOUNTAIN_LEN, 30, 40);
      geo.rotateX(-Math.PI / 2);
      const pos = geo.attributes.position;
      const TAU = Math.PI * 2;
      const seed = side > 0 ? 1.7 : 4.3;
      for (let i = 0; i < pos.count; i++) {
        const x = pos.getX(i);
        const z = pos.getZ(i);
        const u = (x + width / 2) / width; // 0 near road .. 1 far
        const fromRoad = side > 0 ? u : 1 - u;
        const zz = (z / MOUNTAIN_LEN) * TAU;
        let h =
          Math.sin(zz * 2 + seed) * 0.5 +
          Math.sin(zz * 5 + seed * 2.1 + x * 0.05) * 0.3 +
          Math.sin(zz * 11 + x * 0.21) * 0.15 +
          Math.abs(Math.sin(zz * 3 + x * 0.09 + seed)) * 0.6;
        h = Math.max(0, h + 0.4);
        const ramp = Math.pow(Math.min(1, Math.max(0, fromRoad - 0.04) * 1.6), 1.4);
        pos.setY(i, h * ramp * 26 + ramp * 4);
      }
      geo.computeVertexNormals();
      const group = new THREE.Group();
      const fill = new THREE.Mesh(
        geo,
        new THREE.MeshBasicMaterial({ color: 0x0b0320, polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 })
      );
      const wire = new THREE.LineSegments(
        new THREE.WireframeGeometry(geo),
        new THREE.LineBasicMaterial({ color: new THREE.Color('#9a3bff').multiplyScalar(1.3), transparent: true, opacity: 0.75 })
      );
      group.add(fill, wire);
      group.position.x = side * (TRACK_HALF + 12 + width / 2);
      return group;
    };

    this.mountains = [];
    for (const side of [-1, 1]) {
      for (let i = 0; i < 3; i++) {
        const chunk = makeChunk(side);
        chunk.userData.baseZ = -i * MOUNTAIN_LEN + MOUNTAIN_LEN / 2;
        this.scene.add(chunk);
        this.mountains.push(chunk);
      }
    }
  }

  buildPosts() {
    const postGeo = new THREE.BoxGeometry(0.18, 2.6, 0.18);
    postGeo.translate(0, 1.3, 0);
    const capGeo = new THREE.BoxGeometry(0.9, 0.12, 0.12);
    capGeo.translate(0, 2.6, 0);
    const postMat = new THREE.MeshStandardMaterial({ color: 0x1a0d33, metalness: 0.6, roughness: 0.4 });
    const capMat = new THREE.MeshBasicMaterial({ color: new THREE.Color('#2ff3ff').multiplyScalar(3) });
    const count = POST_COUNT * 2;
    this.posts = new THREE.InstancedMesh(postGeo, postMat, count);
    this.caps = new THREE.InstancedMesh(capGeo, capMat, count);
    this.posts.frustumCulled = false;
    this.caps.frustumCulled = false;
    this.scene.add(this.posts, this.caps);
    this._dummy = new THREE.Object3D();
    this.updatePosts();
  }

  updatePosts() {
    const span = POST_SPACING * POST_COUNT;
    const d = this._dummy;
    let k = 0;
    for (const side of [-1, 1]) {
      for (let i = 0; i < POST_COUNT; i++) {
        let z = ((i * POST_SPACING + this.offset) % span) - span + 20;
        d.position.set(side * (TRACK_HALF + 1.4), 0, z);
        d.rotation.set(0, 0, 0);
        d.updateMatrix();
        this.posts.setMatrixAt(k, d.matrix);
        this.caps.setMatrixAt(k, d.matrix);
        k++;
      }
    }
    this.posts.instanceMatrix.needsUpdate = true;
    this.caps.instanceMatrix.needsUpdate = true;
  }

  buildStars() {
    const n = 1400;
    const pos = new Float32Array(n * 3);
    const col = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) {
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.acos(Math.random() * 0.92 + 0.06);
      const r = 800;
      pos[i * 3] = Math.sin(phi) * Math.cos(theta) * r;
      pos[i * 3 + 1] = Math.cos(phi) * r;
      pos[i * 3 + 2] = Math.sin(phi) * Math.sin(theta) * r;
      const b = 0.6 + Math.random() * 1.6;
      const tint = Math.random();
      col[i * 3] = b * (tint > 0.7 ? 1 : 0.8);
      col[i * 3 + 1] = b * 0.8;
      col[i * 3 + 2] = b;
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    const mat = new THREE.PointsMaterial({ size: 1.6, sizeAttenuation: false, vertexColors: true, fog: false, transparent: true, depthWrite: false });
    this.stars = new THREE.Points(geo, mat);
    this.stars.renderOrder = -8;
    this.scene.add(this.stars);
  }

  update(dt, speed, boost) {
    this.time += dt;
    this.offset += speed * dt;
    this.floor.material.uniforms.uOffset.value = this.offset % 10000;
    this.floor.material.uniforms.uBoost.value = boost;
    this.sun.material.uniforms.uTime.value = this.time;

    const total = MOUNTAIN_LEN * 3;
    for (const m of this.mountains) {
      let z = m.userData.baseZ + (this.offset % total);
      if (z > MOUNTAIN_LEN * 1.5) z -= total;
      m.position.z = z;
    }
    this.updatePosts();
    this.stars.rotation.y += dt * 0.004;
  }

  // keeps sky elements centred on the camera so they read as infinitely far
  follow(camera) {
    this.sky.position.copy(camera.position);
    this.stars.position.copy(camera.position);
    this.sun.position.x = camera.position.x * 0.9;
  }
}
