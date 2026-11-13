import * as THREE from 'three';
import { TRACK_HALF } from './world.js';

export const LANES = [-4, 0, 4];
const SPAWN_Z = -240;
const DESPAWN_Z = 14;

const C = {
  barrier: new THREE.Color('#ff3b5c'),
  low: new THREE.Color('#ff8a1f'),
  shard: new THREE.Color('#2ff3ff'),
  shield: new THREE.Color('#6cff9a'),
  boost: new THREE.Color('#ffe14d'),
};

function glowing(color, k) {
  return color.clone().multiplyScalar(k);
}

// Builds and pools every kind of track object.
class Factory {
  constructor() {
    const bodyMat = new THREE.MeshStandardMaterial({ color: 0x1c0820, metalness: 0.6, roughness: 0.35, emissive: 0x300010, emissiveIntensity: 0.5 });
    const lowBody = new THREE.MeshStandardMaterial({ color: 0x24100a, metalness: 0.6, roughness: 0.35, emissive: 0x3a1500, emissiveIntensity: 0.5 });

    this.barrierGeo = new THREE.BoxGeometry(2.8, 3.2, 0.7);
    this.barrierGeo.translate(0, 1.6, 0);
    this.barrierEdges = new THREE.EdgesGeometry(this.barrierGeo);
    this.barrierEdgeMat = new THREE.LineBasicMaterial({ color: glowing(C.barrier, 2.6) });
    this.barrierBody = bodyMat;
    this.stripeGeo = new THREE.BoxGeometry(2.6, 0.16, 0.74);
    this.stripeMat = new THREE.MeshBasicMaterial({ color: glowing(C.barrier, 3) });

    this.lowGeo = new THREE.BoxGeometry(1, 0.85, 0.6);
    this.lowGeo.translate(0, 0.425, 0);
    this.lowEdges = new THREE.EdgesGeometry(this.lowGeo);
    this.lowEdgeMat = new THREE.LineBasicMaterial({ color: glowing(C.low, 2.6) });
    this.lowBody = lowBody;
    this.lowStripeMat = new THREE.MeshBasicMaterial({ color: glowing(C.low, 3) });

    this.shardGeo = new THREE.OctahedronGeometry(0.42, 0);
    this.shardMat = new THREE.MeshStandardMaterial({ color: 0x0a3a40, emissive: glowing(C.shard, 1.0), emissiveIntensity: 0.9, metalness: 0.3, roughness: 0.2 });
    this.shardEdges = new THREE.EdgesGeometry(this.shardGeo);
    this.shardEdgeMat = new THREE.LineBasicMaterial({ color: glowing(new THREE.Color('#9ffbff'), 1.4) });

    this.shieldGeo = new THREE.IcosahedronGeometry(0.6, 1);
    this.shieldMat = new THREE.MeshStandardMaterial({ color: 0x0a3a1a, emissive: glowing(C.shield, 1), emissiveIntensity: 1.0, wireframe: true });
    this.shieldCore = new THREE.MeshBasicMaterial({ color: glowing(C.shield, 2) });

    this.ringGeo = new THREE.TorusGeometry(1.55, 0.12, 12, 48);
    this.ringMat = new THREE.MeshBasicMaterial({ color: glowing(C.boost, 2.8) });
    this.ringInnerMat = new THREE.MeshBasicMaterial({ color: glowing(C.boost, 0.6), transparent: true, opacity: 0.25, side: THREE.DoubleSide, depthWrite: false, blending: THREE.AdditiveBlending });
  }

  make(type) {
    const g = new THREE.Group();
    if (type === 'barrier') {
      g.add(new THREE.Mesh(this.barrierGeo, this.barrierBody));
      g.add(new THREE.LineSegments(this.barrierEdges, this.barrierEdgeMat));
      for (const y of [0.9, 2.3]) {
        const s = new THREE.Mesh(this.stripeGeo, this.stripeMat);
        s.position.y = y;
        g.add(s);
      }
    } else if (type === 'low') {
      g.add(new THREE.Mesh(this.lowGeo, this.lowBody));
      g.add(new THREE.LineSegments(this.lowEdges, this.lowEdgeMat));
      const s = new THREE.Mesh(this.lowGeo, this.lowStripeMat);
      s.scale.set(1.01, 0.12, 1.05);
      s.position.y = 0.6;
      g.add(s);
    } else if (type === 'shard') {
      const m = new THREE.Mesh(this.shardGeo, this.shardMat);
      m.scale.y = 1.4;
      m.add(new THREE.LineSegments(this.shardEdges, this.shardEdgeMat));
      g.add(m);
      g.userData.spin = m;
    } else if (type === 'shield') {
      const m = new THREE.Mesh(this.shieldGeo, this.shieldMat);
      const core = new THREE.Mesh(new THREE.SphereGeometry(0.28, 16, 8), this.shieldCore);
      m.add(core);
      g.add(m);
      g.userData.spin = m;
    } else if (type === 'boost') {
      const ring = new THREE.Mesh(this.ringGeo, this.ringMat);
      const inner = new THREE.Mesh(new THREE.CircleGeometry(1.45, 40), this.ringInnerMat);
      ring.add(inner);
      g.add(ring);
      g.userData.spin = ring;
    }
    return g;
  }
}

export class Track {
  constructor(scene) {
    this.scene = scene;
    this.factory = new Factory();
    this.pools = {};
    this.active = [];
    this.reset();
  }

  reset() {
    for (const e of this.active) this.release(e);
    this.active.length = 0;
    this.untilNext = 15;
    this.rowsSincePower = 0;
    this.lastPattern = '';
  }

  acquire(type) {
    const pool = (this.pools[type] ||= []);
    let e = pool.pop();
    if (!e) {
      e = { type, mesh: this.factory.make(type) };
      this.scene.add(e.mesh);
    }
    e.mesh.visible = true;
    e.mesh.scale.set(1, 1, 1);
    e.mesh.rotation.set(0, 0, 0);
    e.done = false;
    e.optional = false;
    e.moveAmp = 0;
    e.movePhase = 0;
    e.time = 0;
    this.active.push(e);
    return e;
  }

  release(e) {
    e.mesh.visible = false;
    this.pools[e.type].push(e);
  }

  add(type, x, z, opts = {}) {
    const e = this.acquire(type);
    e.x = x;
    e.baseX = x;
    e.z = z;
    e.y = opts.y ?? 0;
    switch (type) {
      case 'barrier':
        e.halfW = 1.4; e.halfD = 0.35; e.height = 3.2;
        break;
      case 'low': {
        const w = opts.width ?? TRACK_HALF * 2;
        e.mesh.scale.x = w;
        e.halfW = w / 2; e.halfD = 0.3; e.height = 0.85;
        break;
      }
      case 'shard':
        e.halfW = 0.9; e.halfD = 0.9; e.y = opts.y ?? 1.0;
        break;
      case 'shield':
        e.halfW = 1.1; e.halfD = 1.1; e.y = 1.2;
        break;
      case 'boost':
        e.halfW = 1.5; e.halfD = 0.8; e.y = 1.7;
        break;
    }
    if (opts.optional) e.optional = true;
    if (opts.moveAmp) {
      e.moveAmp = opts.moveAmp;
      e.movePhase = opts.movePhase ?? 0;
      e.moveSpeed = opts.moveSpeed ?? 2;
    }
    e.mesh.position.set(e.x, e.y, e.z);
    return e;
  }

  // ---------- Patterns ----------
  // Each returns the length of track it occupies.

  pick(arr) {
    return arr[Math.floor(Math.random() * arr.length)];
  }

  shardLine(x, z, n, spacing = 4, y) {
    for (let i = 0; i < n; i++) this.add('shard', x, z - i * spacing, { y });
  }

  patBarriers(z, diff) {
    const lanes = [0, 1, 2].sort(() => Math.random() - 0.5);
    const blocked = Math.random() < 0.35 + diff * 0.5 ? 2 : 1;
    for (let i = 0; i < blocked; i++) this.add('barrier', LANES[lanes[i]], z);
    const free = LANES[lanes[blocked]];
    this.shardLine(free, z + 8, 4);
    return 14;
  }

  patJumpWall(z) {
    this.add('low', 0, z);
    const x = this.pick(LANES);
    this.shardLine(x, z + 10, 2);
    this.add('shard', x, z, { y: 2.6, optional: true });
    this.add('shard', x, z - 4, { y: 2.2, optional: true });
    return 16;
  }

  patSlalom(z, diff) {
    const rows = 3 + Math.floor(diff * 2);
    let side = Math.random() < 0.5 ? 0 : 2;
    const gap = 18 - diff * 3;
    for (let i = 0; i < rows; i++) {
      const zz = z - i * gap;
      this.add('barrier', LANES[side], zz);
      this.add('barrier', LANES[1], zz);
      const open = LANES[2 - side];
      this.add('shard', open, zz + 3);
      side = 2 - side;
    }
    return rows * gap;
  }

  patWeave(z) {
    const n = 12;
    const phase = Math.random() * Math.PI * 2;
    for (let i = 0; i < n; i++) {
      const x = Math.sin(phase + i * 0.5) * 4;
      this.add('shard', x, z - i * 3.5);
    }
    return n * 3.5 + 6;
  }

  patMover(z, diff) {
    this.add('barrier', 0, z, { moveAmp: 4, movePhase: Math.random() * Math.PI * 2, moveSpeed: 1.6 + diff * 1.2 });
    if (diff > 0.55) {
      this.add('barrier', 0, z - 16, { moveAmp: 4, movePhase: Math.random() * Math.PI * 2, moveSpeed: 1.6 + diff * 1.2 });
      return 30;
    }
    return 16;
  }

  patGate(z) {
    const wallLane = Math.floor(Math.random() * 3);
    for (let i = 0; i < 3; i++) {
      if (i === wallLane) this.add('barrier', LANES[i], z);
      else this.add('low', LANES[i], z, { width: 4 });
    }
    const x = LANES[wallLane === 0 ? 2 : 0];
    this.add('shard', x, z, { y: 2.6, optional: true });
    return 14;
  }

  patTunnel(z) {
    // low walls on both sides, a lane of shards through the middle under pressure
    const x = this.pick(LANES);
    for (let i = 0; i < 3; i++) {
      for (const l of LANES) if (l !== x) this.add('barrier', l, z - i * 10);
      this.add('shard', x, z - i * 10 + 5);
    }
    return 30;
  }

  spawnRow(z, diff, player) {
    const options = [
      ['barriers', 3],
      ['jump', 2],
      ['weave', diff < 0.3 ? 2 : 1],
      ['slalom', diff > 0.12 ? 2 : 0],
      ['mover', diff > 0.22 ? 2 : 0],
      ['gate', diff > 0.35 ? 2 : 0],
      ['tunnel', diff > 0.5 ? 1.5 : 0],
    ].filter(([name, w]) => w > 0 && name !== this.lastPattern);
    let total = options.reduce((s, o) => s + o[1], 0);
    let r = Math.random() * total;
    let choice = options[0][0];
    for (const [name, w] of options) {
      if ((r -= w) <= 0) {
        choice = name;
        break;
      }
    }
    this.lastPattern = choice;

    let len;
    switch (choice) {
      case 'barriers': len = this.patBarriers(z, diff); break;
      case 'jump': len = this.patJumpWall(z); break;
      case 'weave': len = this.patWeave(z); break;
      case 'slalom': len = this.patSlalom(z, diff); break;
      case 'mover': len = this.patMover(z, diff); break;
      case 'gate': len = this.patGate(z); break;
      case 'tunnel': len = this.patTunnel(z); break;
    }

    // occasional power-ups in the breathing room after a pattern
    this.rowsSincePower++;
    if (this.rowsSincePower > 5 && Math.random() < 0.18) {
      this.rowsSincePower = 0;
      const type = !player.hasShield && Math.random() < 0.55 ? 'shield' : 'boost';
      this.add(type, this.pick(LANES), z - len - 6);
      len += 12;
    }
    return len;
  }

  // ---------- Simulation ----------

  update(dt, speed, diff, player, handlers) {
    // spawning
    this.untilNext -= speed * dt;
    if (this.untilNext <= 0 && handlers.spawning) {
      const len = this.spawnRow(SPAWN_Z, diff, player);
      const breath = speed * (0.75 - diff * 0.3) + 8;
      this.untilNext += len + breath;
    }

    const px = player.x;
    const py = player.y;
    for (let i = this.active.length - 1; i >= 0; i--) {
      const e = this.active[i];
      e.time += dt;
      e.z += speed * dt;
      if (e.moveAmp) {
        e.x = e.baseX + Math.sin(e.time * e.moveSpeed + e.movePhase) * e.moveAmp;
      }
      e.mesh.position.set(e.x, e.y, e.z);
      const spin = e.mesh.userData.spin;
      if (spin) {
        if (e.type === 'boost') {
          spin.rotation.z += dt * 1.5;
          e.mesh.scale.setScalar(1 + Math.sin(e.time * 6) * 0.05);
        } else {
          spin.rotation.y += dt * 2.6;
          e.mesh.position.y = e.y + Math.sin(e.time * 3 + e.z * 0.1) * 0.15;
        }
      }

      if (!e.done) {
        const dz = Math.abs(e.z);
        const dx = Math.abs(e.x - px);
        if (dz < e.halfD + player.halfD && dx < e.halfW + player.halfW * 0.85) {
          if (e.type === 'barrier') {
            e.done = true;
            handlers.hit(e);
          } else if (e.type === 'low') {
            if (py < e.height + 0.05) {
              e.done = true;
              handlers.hit(e);
            }
          } else {
            const shipY = 0.55 + py + 0.3;
            if (Math.abs(shipY - e.y) < 1.6) {
              e.done = true;
              handlers.collect(e);
              this.release(e);
              this.active.splice(i, 1);
              continue;
            }
          }
        } else if (e.z > player.halfD + 1.2) {
          e.done = true;
          if (e.type === 'shard' && !e.optional) handlers.miss(e);
          if ((e.type === 'barrier' || e.type === 'low') && dx < e.halfW + 2.2) handlers.nearMiss(e);
        }
      }

      if (e.z > DESPAWN_Z) {
        this.release(e);
        this.active.splice(i, 1);
      }
    }
  }

  // knocks an obstacle away after a shielded hit
  shatter(e) {
    e.mesh.visible = false;
  }
}
