import * as THREE from 'three';
import { PLACES, PATHS, terrainHeight } from './layout.js';

// Populates the kingdom with enemies, coins and chests, and tracks the main quest line.
export class Quest {
  constructor(game) {
    this.game = game;
    this.stage = 0;
    this.hasKey = false;
    this.bossAwake = false;
    this.done = false;
  }

  get stages() {
    const g = this.game;
    return [
      {
        title: 'The Western Woods',
        desc: 'A skeleton warband is camped in the Western Woods. Destroy it.',
        target: () => PLACES.westCamp,
        progress: () => this.groupProgress('west'),
        complete: () => g.enemies.aliveIn('west') === 0,
        checkpoint: { x: PLACES.westCamp.x + 6, z: PLACES.westCamp.z + 12 },
      },
      {
        title: 'The Old Ruins',
        desc: 'Bone Knights guard the Old Ruins on the eastern hill. Cut them down.',
        target: () => PLACES.eastRuins,
        progress: () => this.groupProgress('ruins'),
        complete: () => g.enemies.aliveIn('ruins') === 0,
        checkpoint: { x: PLACES.eastRuins.x - 4, z: PLACES.eastRuins.z - 2 },
      },
      {
        title: 'The Sky Shrine',
        desc: 'Climb the floating stones above the ruins and claim the Royal Key from the shrine.',
        target: () => ({ x: g.world.shrine.x, z: g.world.shrine.z }),
        progress: () => (this.hasKey ? 'Royal Key claimed' : 'Use double jump to reach higher stones'),
        complete: () => this.hasKey,
        checkpoint: { x: 2, z: -40 },
      },
      {
        title: 'Storm the Castle',
        desc: 'Bring the Royal Key to the castle gate in the north.',
        target: () => PLACES.gate,
        progress: () => 'Approach the gate',
        complete: () => g.world.gateOpening,
        checkpoint: { x: 0, z: -60 },
      },
      {
        title: 'The Bone King',
        desc: 'Mordrak waits in the courtyard. End his reign.',
        target: () => PLACES.courtyard,
        progress: () => (this.boss && this.boss.alive ? `${Math.ceil((this.boss.hp / this.boss.maxHp) * 100)}% of his strength remains` : 'Enter the courtyard'),
        complete: () => this.boss && !this.boss.alive,
        checkpoint: { x: 0, z: -62 },
      },
    ];
  }

  groupProgress(group) {
    const total = this.game.enemies.totalIn(group);
    const left = this.game.enemies.aliveIn(group);
    return `Skeletons defeated: ${total - left} / ${total}`;
  }

  current() {
    return this.stages[this.stage];
  }

  populate() {
    const { enemies, pickups, world } = this.game;
    this.stage = 0;
    this.hasKey = false;
    this.bossAwake = false;
    this.done = false;

    // west warband: they lie among the tents and rise when you approach
    const W = PLACES.westCamp;
    const west = [['minion', -4, 3], ['minion', 4, -4], ['rogue', 2, 5], ['minion', -5, -4], ['warrior', 7, 2]];
    for (const [t, dx, dz] of west) enemies.spawn(t, W.x + dx, W.z + dz, { dormant: true, group: 'west' });

    // ruins garrison
    const R = PLACES.eastRuins;
    const ruins = [['warrior', -3, 4], ['warrior', 4, -3], ['minion', -6, -3], ['minion', 6, 5], ['mage', 0, -8], ['rogue', 1, 8]];
    for (const [t, dx, dz] of ruins) enemies.spawn(t, R.x + dx, R.z + dz, { dormant: true, group: 'ruins' });

    // wandering stragglers along the roads
    for (const [t, x, z] of [['minion', -40, 10], ['minion', 40, -20], ['rogue', 20, 0], ['minion', -20, -30], ['minion', 30, 30]]) {
      enemies.spawn(t, x, z, { group: 'roam' });
    }

    // the Bone King and his honour guard
    const C = PLACES.courtyard;
    this.boss = enemies.spawn('boss', C.x, C.z - 4, { dormant: true, group: 'castle', facing: 0 });
    enemies.spawn('warrior', C.x - 9, C.z + 2, { dormant: true, group: 'castle' });
    enemies.spawn('warrior', C.x + 9, C.z + 2, { dormant: true, group: 'castle' });

    // coins along every road
    for (const path of PATHS) {
      for (let i = 0; i < path.length - 1; i++) {
        const [ax, az] = path[i];
        const [bx, bz] = path[i + 1];
        const len = Math.hypot(bx - ax, bz - az);
        const n = Math.floor(len / 9);
        for (let k = 1; k < n; k++) {
          if (k % 4 === 0) continue;
          const t = k / n;
          const side = Math.sin(k * 1.7) * 1.5;
          pickups.coin(ax + (bx - ax) * t + side, az + (bz - az) * t);
        }
      }
    }
    // rings of coins around landmarks
    const ring = (cx, cz, r, n) => {
      for (let i = 0; i < n; i++) {
        const a = (i / n) * Math.PI * 2;
        pickups.coin(cx + Math.cos(a) * r, cz + Math.sin(a) * r);
      }
    };
    ring(PLACES.village.x + 5, PLACES.village.z, 5, 8);
    ring(PLACES.lake.x, PLACES.lake.z, PLACES.lake.r + 4, 14);
    ring(-60, 40, 4, 6);
    ring(60, 20, 4, 6);
    ring(-100, -60, 5, 7);
    ring(100, 40, 5, 7);
    // a coin above every floating stone
    for (const p of world.platforms) pickups.coin(p.base.x, p.base.z, p.base.y + 0.2);

    // chests
    pickups.chest(6, 30, Math.PI, { coins: 8 });
    pickups.chest(W.x - 2, W.z - 10, 0.3, { coins: 14, potion: true });
    pickups.chest(R.x - 4, R.z + 12, 2.6, { coins: 14, potion: true });
    pickups.chest(world.shrine.x, world.shrine.z, 0.5, { coins: 25, key: true }, { gold: true, y: world.shrine.y });
    pickups.chest(-118, 30, 1.2, { coins: 12 });
    pickups.chest(112, -70, -0.8, { coins: 12, potion: true });

    // potions
    for (const [x, z] of [[-30, 20], [40, -10], [PLACES.westCamp.x + 14, PLACES.westCamp.z], [PLACES.eastRuins.x - 14, PLACES.eastRuins.z + 6], [-6, -56], [6, -56]]) {
      pickups.potion(x, z);
    }
  }

  update() {
    const g = this.game;
    if (this.done) return;

    // the gate opens for the bearer of the key
    if (this.hasKey && !g.world.gateOpening) {
      const d = Math.hypot(g.player.pos.x - PLACES.gate.x, g.player.pos.z - PLACES.gate.z);
      if (d < 12) g.openGate();
    }
    // the Bone King wakes when the hero enters the courtyard
    if (!this.bossAwake && g.world.gateOpen > 0.6) {
      const p = g.player.pos;
      if (Math.abs(p.x) < 38 && p.z < PLACES.gate.z - 3) {
        this.bossAwake = true;
        g.bossFight(this.boss);
      }
    }

    const stage = this.current();
    if (stage.complete()) {
      const cp = stage.checkpoint;
      g.checkpoint.set(cp.x, terrainHeight(cp.x, cp.z), cp.z);
      if (this.stage === this.stages.length - 1) {
        this.done = true;
        g.victory();
        return;
      }
      g.questComplete(stage.title);
      this.stage++;
    }
  }

  objective() {
    if (this.done) return null;
    const t = this.current().target();
    return new THREE.Vector3(t.x, 0, t.z);
  }
}
