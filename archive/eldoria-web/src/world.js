import * as THREE from 'three';
import { assets, cloneModel, meshParts } from './assets.js';
import { Physics } from './physics.js';
import {
  WORLD_SIZE, WORLD_HALF, PLAY_RADIUS, WATER_LEVEL, PLACES,
  terrainHeight, roadFactor, pathDistance, fbm,
} from './layout.js';

const BUILDING_SCALE = 7;
const NATURE_SCALE = 7;

// deterministic random so the kingdom looks the same every visit
let seed = 1337;
const rand = () => {
  seed = (seed * 16807) % 2147483647;
  return (seed - 1) / 2147483646;
};
const range = (a, b) => a + rand() * (b - a);

export class World {
  constructor(scene, renderer) {
    this.scene = scene;
    this.renderer = renderer;
    this.physics = new Physics();
    this.time = 0;
    this.platforms = [];
    this.flags = [];
    this.fires = [];
    this.clouds = [];
    this.sizeCache = {};

    this.buildSky();
    this.buildLights();
    this.buildTerrain();
    this.buildWater();
    this.buildGrass();
    this.buildVillage();
    this.buildCastle();
    this.buildWestCamp();
    this.buildRuins();
    this.buildSkyPlatforms();
    this.buildNature();
    this.buildHorizon();
  }

  // ---------- sky & light ----------

  buildSky() {
    const hdr = assets.hdr;
    this.scene.background = hdr;
    this.scene.environment = hdr;
    this.scene.backgroundIntensity = 0.9;
    this.scene.environmentIntensity = 0.75;
    this.scene.backgroundRotation = new THREE.Euler(0, 1.2, 0);
    this.scene.environmentRotation = new THREE.Euler(0, 1.2, 0);
    this.scene.fog = new THREE.Fog(0xb9c8d8, 90, 420);
  }

  buildLights() {
    const sun = new THREE.DirectionalLight(0xfff1dc, 2.6);
    sun.castShadow = true;
    sun.shadow.mapSize.set(2048, 2048);
    const s = 48;
    Object.assign(sun.shadow.camera, { left: -s, right: s, top: s, bottom: -s, near: 1, far: 260 });
    sun.shadow.bias = -0.0004;
    sun.shadow.normalBias = 0.04;
    this.sunOffset = new THREE.Vector3(60, 110, 45);
    this.sun = sun;
    this.scene.add(sun, sun.target);
    this.scene.add(new THREE.HemisphereLight(0xcfe3ff, 0x4a5a2a, 0.55));
  }

  followShadow(target) {
    this.sun.position.copy(target).add(this.sunOffset);
    this.sun.target.position.copy(target);
  }

  // ---------- terrain ----------

  buildTerrain() {
    const seg = 300;
    const geo = new THREE.PlaneGeometry(WORLD_SIZE, WORLD_SIZE, seg, seg);
    geo.rotateX(-Math.PI / 2);
    const pos = geo.attributes.position;
    const colors = new Float32Array(pos.count * 3);
    const grassA = new THREE.Color('#5d8a32');
    const grassB = new THREE.Color('#86a63c');
    const grassC = new THREE.Color('#3f6b2a');
    const dirt = new THREE.Color('#9b7b52');
    const sand = new THREE.Color('#c7b48a');
    const rock = new THREE.Color('#77705f');
    const c = new THREE.Color();

    for (let i = 0; i < pos.count; i++) {
      const x = pos.getX(i), z = pos.getZ(i);
      const h = terrainHeight(x, z);
      pos.setY(i, h);
      const n = fbm(x * 0.04, z * 0.04);
      const n2 = fbm(x * 0.2 + 7, z * 0.2);
      c.copy(grassA).lerp(grassB, THREE.MathUtils.smoothstep(n, 0.35, 0.7)).lerp(grassC, THREE.MathUtils.smoothstep(n2, 0.55, 0.8) * 0.6);
      const slope = Math.abs(terrainHeight(x + 1, z) - h) + Math.abs(terrainHeight(x, z + 1) - h);
      c.lerp(rock, THREE.MathUtils.smoothstep(slope, 0.6, 1.4));
      c.lerp(sand, 1 - THREE.MathUtils.smoothstep(h, WATER_LEVEL + 0.2, WATER_LEVEL + 1.4));
      c.lerp(dirt, roadFactor(x, z) * (0.85 + n2 * 0.15));
      colors[i * 3] = c.r;
      colors[i * 3 + 1] = c.g;
      colors[i * 3 + 2] = c.b;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));
    geo.computeVertexNormals();

    const mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.95, metalness: 0, map: this.detailTexture() });
    this.terrain = new THREE.Mesh(geo, mat);
    this.terrain.receiveShadow = true;
    this.scene.add(this.terrain);

    this.buildHeightTexture();
  }

  // fine speckle texture multiplied over vertex colours so the ground does not look flat
  detailTexture() {
    const size = 256;
    const cv = document.createElement('canvas');
    cv.width = cv.height = size;
    const g = cv.getContext('2d');
    const img = g.createImageData(size, size);
    for (let i = 0; i < size * size; i++) {
      const x = i % size, y = Math.floor(i / size);
      const v = 205 + fbm(x * 0.09, y * 0.09, 3) * 40 + (Math.random() - 0.5) * 30;
      img.data[i * 4] = img.data[i * 4 + 1] = img.data[i * 4 + 2] = Math.min(255, v);
      img.data[i * 4 + 3] = 255;
    }
    g.putImageData(img, 0, 0);
    const tex = new THREE.CanvasTexture(cv);
    tex.wrapS = tex.wrapT = THREE.RepeatWrapping;
    tex.repeat.set(90, 90);
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.anisotropy = 8;
    return tex;
  }

  // height + grass density sampled by the grass and water shaders
  buildHeightTexture() {
    const n = 512;
    const data = new Float32Array(n * n * 4);
    for (let j = 0; j < n; j++) {
      for (let i = 0; i < n; i++) {
        const x = (i / (n - 1)) * WORLD_SIZE - WORLD_HALF;
        const z = (j / (n - 1)) * WORLD_SIZE - WORLD_HALF;
        const h = terrainHeight(x, z);
        const k = (j * n + i) * 4;
        let grass = 1 - roadFactor(x, z) * 1.2;
        grass *= THREE.MathUtils.smoothstep(h, WATER_LEVEL + 0.3, WATER_LEVEL + 1.2);
        const dc = Math.hypot(x - PLACES.castle.x, z - PLACES.castle.z);
        if (dc < 42 && Math.abs(x) < 42 && z < -66) grass *= 0.15;
        data[k] = h;
        data[k + 1] = Math.max(0, grass);
        data[k + 3] = 1;
      }
    }
    const tex = new THREE.DataTexture(data, n, n, THREE.RGBAFormat, THREE.FloatType);
    tex.needsUpdate = true;
    this.heightTex = tex;
    this.heightTexSize = n;
  }

  // ---------- water ----------

  buildWater() {
    const geo = new THREE.CircleGeometry(PLACES.lake.r + 14, 64);
    geo.rotateX(-Math.PI / 2);
    this.waterMat = new THREE.ShaderMaterial({
      transparent: true,
      fog: true,
      uniforms: THREE.UniformsUtils.merge([
        THREE.UniformsLib.fog,
        {
          uTime: { value: 0 },
          uHeight: { value: this.heightTex },
          uSize: { value: WORLD_SIZE },
          uSun: { value: this.sunOffset.clone().normalize() },
        },
      ]),
      vertexShader: /* glsl */ `
        #include <fog_pars_vertex>
        varying vec3 vWorld;
        void main() {
          vec4 w = modelMatrix * vec4(position, 1.0);
          vWorld = w.xyz;
          vec4 mvPosition = viewMatrix * w;
          gl_Position = projectionMatrix * mvPosition;
          #include <fog_vertex>
        }`,
      fragmentShader: /* glsl */ `
        #include <fog_pars_fragment>
        uniform float uTime; uniform sampler2D uHeight; uniform float uSize; uniform vec3 uSun;
        varying vec3 vWorld;
        void main() {
          vec2 uv = vWorld.xz / uSize + 0.5;
          float ground = texture2D(uHeight, uv).r;
          float depth = clamp((vWorld.y - ground) / 3.5, 0.0, 1.0);
          float t = uTime;
          vec3 n = normalize(vec3(
            sin(vWorld.x * 0.9 + t * 1.3) * 0.06 + sin(vWorld.z * 1.7 - t * 1.1) * 0.04,
            1.0,
            cos(vWorld.z * 0.8 + t * 1.2) * 0.06 + cos(vWorld.x * 1.5 + t * 0.9) * 0.04));
          vec3 view = normalize(cameraPosition - vWorld);
          float fres = pow(1.0 - max(dot(n, view), 0.0), 3.0);
          vec3 shallow = vec3(0.24, 0.55, 0.52);
          vec3 deep = vec3(0.04, 0.17, 0.27);
          vec3 sky = vec3(0.62, 0.74, 0.86);
          vec3 col = mix(shallow, deep, depth);
          col = mix(col, sky, fres * 0.7);
          float spec = pow(max(dot(reflect(-uSun, n), view), 0.0), 120.0);
          col += vec3(1.0, 0.95, 0.85) * spec * 1.5;
          float foam = smoothstep(0.12, 0.0, depth) * (0.6 + 0.4 * sin(t * 2.0 + vWorld.x * 2.0));
          col = mix(col, vec3(0.9), foam * 0.5);
          gl_FragColor = vec4(col, mix(0.55, 0.9, depth) + foam * 0.2);
          #include <tonemapping_fragment>
          #include <colorspace_fragment>
          #include <fog_fragment>
        }`,
    });
    const water = new THREE.Mesh(geo, this.waterMat);
    water.position.set(PLACES.lake.x, WATER_LEVEL, PLACES.lake.z);
    this.scene.add(water);

    for (let i = 0; i < 14; i++) {
      const a = rand() * Math.PI * 2;
      const r = range(PLACES.lake.r - 6, PLACES.lake.r + 2);
      const x = PLACES.lake.x + Math.cos(a) * r, z = PLACES.lake.z + Math.sin(a) * r;
      this.prop(rand() < 0.5 ? 'waterlily_A' : 'waterplant_A', x, z, { scale: NATURE_SCALE * 0.8, y: WATER_LEVEL - 0.05, collide: false, rot: rand() * 6 });
    }
  }

  // ---------- grass ----------
  // A fixed pool of blades that wraps around the camera, reading height from a texture.

  buildGrass() {
    const count = 90000;
    const R = 38;
    const blade = new THREE.BufferGeometry();
    const w = 0.07;
    blade.setAttribute('position', new THREE.Float32BufferAttribute([
      -w, 0, 0, w, 0, 0, -w * 0.7, 0.45, 0, w * 0.7, 0.45, 0, 0, 1, 0,
    ], 3));
    blade.setIndex([0, 1, 2, 2, 1, 3, 2, 3, 4]);
    const geo = new THREE.InstancedBufferGeometry();
    geo.index = blade.index;
    geo.attributes.position = blade.attributes.position;
    const offsets = new Float32Array(count * 4);
    for (let i = 0; i < count; i++) {
      offsets[i * 4] = (Math.random() * 2 - 1) * R;
      offsets[i * 4 + 1] = (Math.random() * 2 - 1) * R;
      offsets[i * 4 + 2] = Math.random();
      offsets[i * 4 + 3] = Math.random() * Math.PI * 2;
    }
    geo.setAttribute('aOffset', new THREE.InstancedBufferAttribute(offsets, 4));
    geo.instanceCount = count;

    this.grassMat = new THREE.ShaderMaterial({
      side: THREE.DoubleSide,
      fog: true,
      uniforms: THREE.UniformsUtils.merge([
        THREE.UniformsLib.fog,
        {
          uTime: { value: 0 },
          uCenter: { value: new THREE.Vector2() },
          uHeight: { value: this.heightTex },
          uRes: { value: this.heightTexSize },
          uSize: { value: WORLD_SIZE },
          uR: { value: R },
          uPlayer: { value: new THREE.Vector3(0, -100, 0) },
        },
      ]),
      vertexShader: /* glsl */ `
        #include <fog_pars_vertex>
        attribute vec4 aOffset;
        uniform float uTime; uniform vec2 uCenter; uniform sampler2D uHeight; uniform float uRes; uniform float uSize; uniform float uR;
        uniform vec3 uPlayer;
        varying float vY; varying float vShade;

        vec2 sampleMap(vec2 p) {
          vec2 uv = (p / uSize + 0.5) * (uRes - 1.0);
          vec2 f = fract(uv); ivec2 i = ivec2(floor(uv));
          vec2 a = texelFetch(uHeight, i, 0).rg, b = texelFetch(uHeight, i + ivec2(1, 0), 0).rg;
          vec2 c = texelFetch(uHeight, i + ivec2(0, 1), 0).rg, d = texelFetch(uHeight, i + ivec2(1, 1), 0).rg;
          return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
        }

        void main() {
          vec2 rel = mod(aOffset.xy - uCenter + uR, 2.0 * uR) - uR;
          vec2 p = uCenter + rel;
          vec2 m = sampleMap(p);
          float density = m.y;
          float fade = smoothstep(uR, uR * 0.6, length(rel));
          float keep = step(aOffset.z, density);
          float h = (0.28 + aOffset.z * 0.42) * fade * keep;

          float ang = aOffset.w;
          vec3 local = position;
          local.y *= h;
          float t = position.y;
          // wind
          float wind = sin(uTime * 1.7 + p.x * 0.25 + p.y * 0.18) * 0.5 + sin(uTime * 3.1 + p.x * 0.9) * 0.15;
          vec2 bend = vec2(wind * 0.35, wind * 0.15) * t * t;
          // part grass around the hero
          vec2 away = p - uPlayer.xz;
          float pd = length(away);
          bend += normalize(away + 1e-4) * smoothstep(1.4, 0.0, pd) * 0.6 * t * step(abs(uPlayer.y - m.x), 1.5);

          vec3 wp = vec3(p.x, m.x, p.y);
          wp.x += local.x * cos(ang) + bend.x;
          wp.z += local.x * sin(ang) + bend.y;
          wp.y += local.y - dot(bend, bend) * 0.3;
          vY = t;
          vShade = 0.75 + aOffset.z * 0.4;
          vec4 mvPosition = viewMatrix * vec4(wp, 1.0);
          gl_Position = projectionMatrix * mvPosition;
          #include <fog_vertex>
        }`,
      fragmentShader: /* glsl */ `
        #include <fog_pars_fragment>
        varying float vY; varying float vShade;
        void main() {
          vec3 base = vec3(0.05, 0.12, 0.02);
          vec3 tip = vec3(0.24, 0.38, 0.08);
          vec3 col = mix(base, tip, vY) * vShade;
          gl_FragColor = vec4(col, 1.0);
          #include <tonemapping_fragment>
          #include <colorspace_fragment>
          #include <fog_fragment>
        }`,
    });
    this.grass = new THREE.Mesh(geo, this.grassMat);
    this.grass.frustumCulled = false;
    this.scene.add(this.grass);
  }

  // ---------- placement helpers ----------

  modelSize(key) {
    if (!this.sizeCache[key]) {
      const box = new THREE.Box3().setFromObject(assets.models[key].scene);
      this.sizeCache[key] = box;
    }
    return this.sizeCache[key];
  }

  // Places a static model on the ground and gives it a box collider matching its footprint.
  prop(key, x, z, opts = {}) {
    const s = opts.scale ?? BUILDING_SCALE;
    const rot = opts.rot ?? 0;
    const y = opts.y ?? terrainHeight(x, z) + (opts.lift ?? 0);
    const obj = cloneModel(key);
    obj.scale.setScalar(s);
    obj.rotation.y = rot;
    obj.position.set(x, y, z);
    this.scene.add(obj);
    if (opts.collide !== false) {
      const b = this.modelSize(key);
      const shrink = opts.shrink ?? 0.85;
      const hw = ((b.max.x - b.min.x) / 2) * s * shrink;
      const hd = ((b.max.z - b.min.z) / 2) * s * shrink;
      const cx = ((b.max.x + b.min.x) / 2) * s, cz = ((b.max.z + b.min.z) / 2) * s;
      const wx = x + cx * Math.cos(rot) + cz * Math.sin(rot);
      const wz = z - cx * Math.sin(rot) + cz * Math.cos(rot);
      const top = y + (b.max.y - b.min.y) * s;
      if (opts.circle) this.physics.addCircle(wx, wz, Math.min(hw, hd), top);
      else obj.userData.collider = this.physics.addBox(wx, wz, hw, hd, -rot, y - 2, top, !!opts.walkable);
    }
    return obj;
  }

  // Draws many copies of a static model with InstancedMesh (one draw call per part).
  instanced(key, transforms) {
    if (!transforms.length) return;
    const parts = meshParts(key);
    const m = new THREE.Matrix4();
    const q = new THREE.Quaternion();
    for (const part of parts) {
      const inst = new THREE.InstancedMesh(part.geometry, part.material, transforms.length);
      inst.castShadow = true;
      inst.receiveShadow = true;
      transforms.forEach((t, i) => {
        q.setFromAxisAngle(THREE.Object3D.DEFAULT_UP, t.rot);
        m.compose(new THREE.Vector3(t.x, t.y, t.z), q, new THREE.Vector3(t.s, t.s * (t.sy ?? 1), t.s));
        inst.setMatrixAt(i, m.multiply(part.matrix));
      });
      inst.computeBoundingSphere();
      this.scene.add(inst);
    }
  }

  torch(x, z, y, light = true) {
    const t = cloneModel('torch_lit');
    t.position.set(x, y ?? terrainHeight(x, z), z);
    t.scale.setScalar(1.6);
    this.scene.add(t);
    const fire = { pos: new THREE.Vector3(x, t.position.y + 1.25, z), light: null };
    if (light) {
      fire.light = new THREE.PointLight(0xff9a3c, 14, 14, 1.8);
      fire.light.position.copy(fire.pos);
      this.scene.add(fire.light);
    }
    this.fires.push(fire);
    this.physics.addCircle(x, z, 0.35, t.position.y + 2);
  }

  // ---------- areas ----------

  buildVillage() {
    const V = PLACES.village;
    this.prop('building_well_blue', V.x + 5, V.z, { scale: 5, rot: 0 });
    const houses = [
      ['building_home_A_blue', -15, 44, 0.6],
      ['building_home_B_blue', -18, 60, 1.4],
      ['building_tavern_blue', -13, 78, 2.4],
      ['building_home_A_blue', 15, 44, -0.5],
      ['building_blacksmith_blue', 19, 52, -1.4],
      ['building_market_blue', 13, 72, 3.6],
      ['building_church_blue', -6, 100, Math.PI],
      ['building_home_B_blue', 12, 92, 3.0],
      ['building_home_A_blue', -26, 86, 2.2],
      ['building_windmill_blue', -36, 68, 1.0],
      ['building_tower_B_blue', 30, 82, -2.4],
      ['building_barracks_blue', -30, 34, 0.4],
    ];
    for (const [k, x, z, r] of houses) this.prop(k, x, z, { rot: r });

    const props = [
      ['barrel', -9, 46], ['barrel', -9.5, 47.5], ['crate_A_big', 9, 48], ['sack', 10, 49.5], ['wheelbarrow', -8, 66],
      ['crate_B_small', 8, 62], ['resource_lumber', -22, 72], ['weaponrack', -24, 38], ['target', -34, 44], ['target', -36, 40],
    ];
    for (const [k, x, z] of props) this.prop(k, x, z, { scale: 6, rot: rand() * 6, shrink: 0.6 });

    for (const [x, z] of [[-4, 50], [4, 66], [-4, 66], [4, 36], [-4, 36]]) this.torch(x, z, undefined, false);
    // fences around the fields east of the village
    for (let i = 0; i < 5; i++) this.prop('fence_wood_straight', 24 + i * 5.6, 34, { scale: 4, rot: 0, shrink: 0.9 });
    this.banner('banner_patternA_blue', -5, 27, 0);
    this.banner('banner_patternA_blue', 5, 27, 0);
  }

  banner(key, x, z, rot) {
    const b = cloneModel(key);
    b.position.set(x, terrainHeight(x, z), z);
    b.rotation.y = rot;
    b.scale.setScalar(1.1);
    this.scene.add(b);
  }

  buildCastle() {
    const plateau = 7;
    const S = BUILDING_SCALE;
    this.castleObj = this.prop('building_castle_blue', PLACES.castle.x, PLACES.castle.z, { scale: 9, y: plateau, rot: Math.PI, shrink: 0.8 });

    const wallZ = PLACES.gate.z;
    // front wall with the gate
    for (const x of [-28, -14, 14, 28]) this.prop('wall_straight', x, wallZ, { y: plateau, scale: S, shrink: 1 });
    this.prop('wall_straight_gate', 0, wallZ, { y: plateau, scale: S, collide: false });
    this.physics.addBox(-5.3, wallZ, 1.7, 2.8, 0, plateau - 1, plateau + 9);
    this.physics.addBox(5.3, wallZ, 1.7, 2.8, 0, plateau - 1, plateau + 9);
    // side and back walls
    for (const z of [-80.5, -94.5, -108.5, -122.5]) {
      this.prop('wall_straight', -42, z, { y: plateau, rot: Math.PI / 2, scale: S, shrink: 1 });
      this.prop('wall_straight', 42, z, { y: plateau, rot: Math.PI / 2, scale: S, shrink: 1 });
    }
    for (const x of [-28, -14, 0, 14, 28]) this.prop('wall_straight', x, -129, { y: plateau, scale: S, shrink: 1 });

    this.enemyTowers = [];
    for (const [x, z] of [[-42, wallZ], [42, wallZ], [-42, -129], [42, -129]]) {
      this.enemyTowers.push(this.prop('building_tower_A_red', x, z, { y: plateau, scale: S, shrink: 0.75 }));
    }
    for (const x of [-12, 12]) this.flags.push(this.prop('flag_red', x, wallZ + 6, { y: terrainHeight(x, wallZ + 6), scale: 6, collide: false }));
    this.prop('building_tower_catapult_red', -26, -95, { y: plateau, scale: S, rot: 0.4 });
    this.prop('building_barracks_red', 27, -97, { y: plateau, scale: S, rot: -0.4 });

    // portcullis that blocks the gate until the Royal Key is found
    const bars = new THREE.Group();
    const iron = new THREE.MeshStandardMaterial({ color: 0x2b2b30, metalness: 0.8, roughness: 0.45 });
    for (let i = -4; i <= 4; i++) {
      const bar = new THREE.Mesh(new THREE.BoxGeometry(0.16, 6.2, 0.16), iron);
      bar.position.set(i * 0.5, 3.1, 0);
      bar.castShadow = true;
      bars.add(bar);
    }
    for (let j = 0; j < 5; j++) {
      const bar = new THREE.Mesh(new THREE.BoxGeometry(4.6, 0.14, 0.14), iron);
      bar.position.set(0, 0.6 + j * 1.3, 0);
      bars.add(bar);
    }
    bars.position.set(0, plateau, wallZ);
    this.scene.add(bars);
    this.portcullis = bars;
    this.gateCollider = this.physics.addBox(0, wallZ, 2.6, 1.2, 0, plateau - 1, plateau + 9);
    this.gateOpen = 0;
    this.gateOpening = false;

    this.torch(-4.5, wallZ + 4, plateau);
    this.torch(4.5, wallZ + 4, plateau);
    this.torch(-8, -96, plateau);
    this.torch(8, -96, plateau);
  }

  openGate() {
    this.gateOpening = true;
    this.gateCollider.enabled = false;
  }

  liberate() {
    for (const f of this.flags) {
      const blue = this.prop('flag_blue', f.position.x, f.position.z, { y: f.position.y, scale: 6, collide: false });
      blue.rotation.copy(f.rotation);
      f.visible = false;
    }
  }

  buildWestCamp() {
    const C = PLACES.westCamp;
    for (let i = 0; i < 5; i++) {
      const a = (i / 5) * Math.PI * 2 + 0.3;
      this.prop('tent', C.x + Math.cos(a) * 11, C.z + Math.sin(a) * 11, { scale: 7, rot: -a + Math.PI / 2, shrink: 0.7 });
    }
    this.prop('resource_lumber', C.x, C.z, { scale: 3, collide: false });
    this.fires.push({ pos: new THREE.Vector3(C.x, terrainHeight(C.x, C.z) + 0.4, C.z), light: new THREE.PointLight(0xff8a2a, 30, 22, 1.6), big: true });
    const f = this.fires[this.fires.length - 1];
    f.light.position.copy(f.pos).add(new THREE.Vector3(0, 1, 0));
    this.scene.add(f.light);
    this.physics.addCircle(C.x, C.z, 1.2, terrainHeight(C.x, C.z) + 1);
    for (const [dx, dz, k] of [[6, -3, 'crate_A_big'], [-5, 5, 'barrel'], [-6, -6, 'weaponrack'], [4, 7, 'sack']]) {
      this.prop(k, C.x + dx, C.z + dz, { scale: 6, rot: rand() * 6, shrink: 0.6 });
    }
    this.prop('flag_red', C.x + 13, C.z + 6, { scale: 6, collide: false });
    this.prop('flag_red', C.x - 12, C.z - 7, { scale: 6, collide: false });
  }

  buildRuins() {
    const R = PLACES.eastRuins;
    const y = terrainHeight(R.x, R.z);
    this.prop('building_destroyed', R.x + 4, R.z + 6, { scale: 9, y, rot: 0.6 });
    for (let i = 0; i < 7; i++) {
      const a = (i / 7) * Math.PI * 2;
      const px = R.x + Math.cos(a) * 12, pz = R.z + Math.sin(a) * 12;
      const p = this.prop('pillar', px, pz, { scale: 1.1, rot: rand(), circle: true });
      if (i % 3 === 1) {
        p.scale.y = 0.45;
        p.rotation.z = 0.15;
      }
    }
    this.prop('stairs', R.x - 6, R.z - 4, { scale: 1, rot: 0.8, collide: false });
    this.prop('flag_red', R.x - 3, R.z + 10, { scale: 6, collide: false });
    this.torch(R.x - 9, R.z + 3);
    this.torch(R.x + 8, R.z - 7);
  }

  // A spiral of floating stone platforms climbing to the shrine of the Royal Key.
  buildSkyPlatforms() {
    const center = new THREE.Vector2(72, -46);
    const toRuins = new THREE.Vector2(PLACES.eastRuins.x, PLACES.eastRuins.z).sub(center).normalize();
    let a = Math.atan2(toRuins.y, toRuins.x);
    const radius = 8.6;
    let y = terrainHeight(PLACES.eastRuins.x, PLACES.eastRuins.z) + 1.7;
    const steps = 10;
    for (let i = 0; i < steps; i++) {
      const x = center.x + Math.cos(a) * radius;
      const z = center.y + Math.sin(a) * radius;
      const moving = i === 4 || i === 7;
      this.platform(x, y, z, 2.4, moving ? { axis: new THREE.Vector3(-Math.sin(a), 0, Math.cos(a)), amp: 2.2, speed: 0.9 + i * 0.05 } : null);
      a -= 0.84;
      y += i < 2 ? 1.8 : 2.05;
    }
    // shrine platform
    const sx = center.x + Math.cos(a) * (radius + 6.5);
    const sz = center.y + Math.sin(a) * (radius + 6.5);
    const sy = y - 0.6;
    for (const [dx, dz] of [[-2, -2], [2, -2], [-2, 2], [2, 2]]) this.platform(sx + dx * 1.0, sy, sz + dz * 1.0, 0, null, true);
    this.physics.addBox(sx, sz, 4.2, 4.2, 0, sy - 1.2, sy, true);
    this.shrine = new THREE.Vector3(sx, sy, sz);
    for (const [dx, dz] of [[-3.4, -3.4], [3.4, -3.4], [-3.4, 3.4], [3.4, 3.4]]) {
      const p = this.prop('pillar', sx + dx, sz + dz, { scale: 0.5, y: sy, circle: true });
      p.scale.y = 0.6;
    }
    this.torch(sx - 3.4, sz, sy);
    this.torch(sx + 3.4, sz, sy);
  }

  platform(x, y, z, half, motion, decorOnly = false) {
    const g = new THREE.Group();
    const tile = cloneModel('floor_tile_large');
    tile.scale.set(1.2, 1, 1.2);
    tile.position.y = -0.05;
    g.add(tile);
    const rockKeys = ['rock_single_C', 'rock_single_D', 'rock_single_E'];
    const rock = cloneModel(rockKeys[Math.floor(rand() * 3)]);
    rock.scale.set(16, 22, 16);
    rock.rotation.set(Math.PI, rand() * 6, 0);
    rock.position.y = -0.15;
    g.add(rock);
    g.position.set(x, y, z);
    this.scene.add(g);
    if (decorOnly) return;
    const box = this.physics.addBox(x, z, half, half, 0, y - 1.2, y, true);
    this.platforms.push({ group: g, box, base: new THREE.Vector3(x, y, z), motion, phase: rand() * 6 });
  }

  buildNature() {
    const singles = { tree_single_A: [], tree_single_B: [] };
    const clusters = { trees_A_medium: [], trees_B_medium: [], trees_A_large: [] };
    const rocks = { rock_single_A: [], rock_single_B: [], rock_single_C: [], rock_single_D: [], rock_single_E: [] };

    const blocked = (x, z, pad) => {
      if (Math.hypot(x, z) > PLAY_RADIUS + 25) return true;
      if (pathDistance(x, z) < 6 + pad) return true;
      const near = (p, r) => Math.hypot(x - p.x, z - p.z) < r + pad;
      if (near(PLACES.village, 40) || near(PLACES.westCamp, 20) || near(PLACES.eastRuins, 22) || near(PLACES.lake, PLACES.lake.r + 2)) return true;
      if (near({ x: 72, z: -46 }, 26)) return true;
      if (Math.abs(x) < 52 && z < -50 && z > -140) return true;
      if (terrainHeight(x, z) < WATER_LEVEL + 0.5) return true;
      return false;
    };

    let tries = 0;
    let placed = 0;
    while (placed < 420 && tries++ < 8000) {
      const x = range(-WORLD_HALF + 15, WORLD_HALF - 15);
      const z = range(-WORLD_HALF + 15, WORLD_HALF - 15);
      // forests are denser to the west and north
      const forest = fbm(x * 0.02 + 40, z * 0.02) + (x < -30 ? 0.25 : 0) + (z < -20 ? 0.1 : 0);
      if (rand() > forest * 1.1) continue;
      if (blocked(x, z, 2)) continue;
      const y = terrainHeight(x, z);
      const rot = rand() * Math.PI * 2;
      if (rand() < 0.75) {
        const s = range(6, 9.5);
        (rand() < 0.5 ? singles.tree_single_A : singles.tree_single_B).push({ x, y: y - 0.2, z, s, rot });
        if (Math.hypot(x, z) < PLAY_RADIUS + 2) this.physics.addCircle(x, z, 0.55, y + s * 1.1, s * 0.3);
      } else {
        const keys = Object.keys(clusters);
        clusters[keys[Math.floor(rand() * keys.length)]].push({ x, y: y - 0.3, z, s: range(5, 7), rot });
        if (Math.hypot(x, z) < PLAY_RADIUS + 6) this.physics.addCircle(x, z, 3.6, y + 8, 6);
      }
      placed++;
    }

    for (let i = 0; i < 160; i++) {
      const x = range(-WORLD_HALF + 20, WORLD_HALF - 20);
      const z = range(-WORLD_HALF + 20, WORLD_HALF - 20);
      if (blocked(x, z, -3)) continue;
      const keys = Object.keys(rocks);
      const s = range(5, 13);
      rocks[keys[Math.floor(rand() * keys.length)]].push({ x, y: terrainHeight(x, z) - 0.1, z, s, rot: rand() * 6 });
      if (s > 9) this.physics.addCircle(x, z, s * 0.1, terrainHeight(x, z) + 0.6);
    }

    for (const [k, list] of Object.entries({ ...singles, ...clusters, ...rocks })) this.instanced(k, list);
  }

  buildHorizon() {
    const ring = { mountain_A_grass_trees: [], mountain_B_grass_trees: [], mountain_C_grass: [], hill_single_A: [], hill_single_B: [] };
    const keys = Object.keys(ring);
    for (let i = 0; i < 44; i++) {
      const a = (i / 44) * Math.PI * 2 + rand() * 0.1;
      const r = range(178, 205);
      const x = Math.cos(a) * r, z = Math.sin(a) * r;
      const k = keys[i % 3];
      ring[k].push({ x, y: terrainHeight(x, z) - 8, z, s: range(22, 34), sy: range(0.9, 1.4), rot: rand() * 6 });
    }
    for (let i = 0; i < 30; i++) {
      const a = rand() * Math.PI * 2;
      const r = range(150, 172);
      const x = Math.cos(a) * r, z = Math.sin(a) * r;
      ring[rand() < 0.5 ? 'hill_single_A' : 'hill_single_B'].push({ x, y: terrainHeight(x, z) - 1, z, s: range(10, 18), rot: rand() * 6 });
    }
    for (const [k, list] of Object.entries(ring)) this.instanced(k, list);

    for (let i = 0; i < 16; i++) {
      const c = cloneModel(rand() < 0.6 ? 'cloud_big' : 'cloud_small');
      c.traverse((o) => {
        if (o.isMesh) {
          o.castShadow = false;
          o.receiveShadow = false;
          o.material = o.material.clone();
          o.material.transparent = true;
          o.material.opacity = 0.92;
          o.material.emissive = new THREE.Color(0xffffff);
          o.material.emissiveIntensity = 0.35;
        }
      });
      c.scale.setScalar(range(9, 16));
      c.position.set(range(-200, 200), range(70, 110), range(-200, 200));
      c.rotation.y = rand() * 6;
      this.scene.add(c);
      this.clouds.push({ obj: c, speed: range(0.8, 2) });
    }
  }

  // ---------- per-frame ----------

  update(dt, playerPos, camera) {
    this.time += dt;
    this.grassMat.uniforms.uTime.value = this.time;
    this.grassMat.uniforms.uCenter.value.set(camera.position.x, camera.position.z);
    this.grassMat.uniforms.uPlayer.value.copy(playerPos);
    this.waterMat.uniforms.uTime.value = this.time;

    for (const p of this.platforms) {
      if (!p.motion) continue;
      const off = Math.sin(this.time * p.motion.speed + p.phase) * p.motion.amp;
      const x = p.base.x + p.motion.axis.x * off;
      const z = p.base.z + p.motion.axis.z * off;
      p.group.position.set(x, p.base.y, z);
      this.physics.moveBox(p.box, x, p.base.y, z);
    }

    for (const c of this.clouds) {
      c.obj.position.x += c.speed * dt;
      if (c.obj.position.x > 230) c.obj.position.x = -230;
    }
    for (const f of this.fires) {
      if (f.light) f.light.intensity = (f.big ? 30 : 14) * (0.85 + Math.sin(this.time * 17 + f.pos.x) * 0.08 + Math.random() * 0.12);
    }

    if (this.gateOpening && this.gateOpen < 1) {
      this.gateOpen = Math.min(1, this.gateOpen + dt * 0.35);
      this.portcullis.position.y = 7 + this.gateOpen * 6;
    }
  }
}
