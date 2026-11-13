import * as THREE from 'three';
import { cloneModel, animationsOf } from './assets.js';
import { Animator } from './animator.js';

export const TYPES = {
  minion: {
    model: 'minion', name: 'Skeleton', hp: 45, speed: 3.6, dmg: 9, range: 2.0, aggro: 15, cooldown: 1.5, coins: 3,
    weapon: 'Skeleton_Blade', attacks: ['1H_Melee_Attack_Chop', '1H_Melee_Attack_Slice_Diagonal'], run: 'Running_A',
  },
  rogue: {
    model: 'rogue', name: 'Skeleton Rogue', hp: 40, speed: 5.4, dmg: 8, range: 1.9, aggro: 17, cooldown: 1.0, coins: 4,
    weapon: 'Skeleton_Blade', offhand: 'Skeleton_Blade', attacks: ['Dualwield_Melee_Attack_Slice', 'Dualwield_Melee_Attack_Stab'], run: 'Running_B',
  },
  warrior: {
    model: 'warrior', name: 'Skeleton Warrior', hp: 95, speed: 3.1, dmg: 16, range: 2.3, aggro: 14, cooldown: 1.9, coins: 6,
    weapon: 'Skeleton_Axe', offhand: 'Skeleton_Shield_Large_A', attacks: ['1H_Melee_Attack_Chop', '1H_Melee_Attack_Slice_Horizontal'], run: 'Running_A', armored: true,
  },
  mage: {
    model: 'mage', name: 'Skeleton Mage', hp: 45, speed: 3.0, dmg: 12, range: 17, aggro: 22, cooldown: 2.6, coins: 5,
    weapon: 'Skeleton_Staff', attacks: ['Spellcast_Shoot'], run: 'Running_A', ranged: true,
  },
  boss: {
    model: 'warrior', name: 'Mordrak, the Bone King', hp: 760, speed: 3.9, dmg: 24, range: 4.0, aggro: 22, cooldown: 1.4, coins: 50,
    weapon: 'Skeleton_Axe', offhand: 'Skeleton_Shield_Large_A', scale: 1.9, run: 'Running_A', boss: true, armored: true,
    attacks: ['1H_Melee_Attack_Chop', '2H_Melee_Attack_Spin', '1H_Melee_Attack_Jump_Chop', '1H_Melee_Attack_Slice_Horizontal'],
  },
};

const tmp = new THREE.Vector3();

class Enemy {
  constructor(manager, type, x, z, opts) {
    this.manager = manager;
    this.cfg = TYPES[type];
    this.type = type;
    this.group = opts.group ?? null;
    const cfg = this.cfg;
    this.model = cloneModel(cfg.model);
    this.scale = cfg.scale ?? 1;
    this.model.scale.setScalar(this.scale);
    this.attach(cfg.weapon, 'handslot.r');
    if (cfg.offhand) this.attach(cfg.offhand, 'handslot.l');
    if (cfg.boss) {
      // a crown of embers marks the Bone King
      this.model.traverse((o) => {
        if (o.isMesh && o.material) {
          o.material = o.material.clone();
          o.material.color.multiplyScalar(0.7);
          o.material.emissive = new THREE.Color(0x3a0606);
        }
      });
    }
    manager.scene.add(this.model);
    this.anim = new Animator(this.model, animationsOf(cfg.model));

    this.home = new THREE.Vector3(x, 0, z);
    this.pos = new THREE.Vector3(x, manager.physics.ground(x, z, 100).y, z);
    this.home.y = this.pos.y;
    this.vel = new THREE.Vector3();
    this.facing = opts.facing ?? Math.random() * Math.PI * 2;
    this.maxHp = cfg.hp;
    this.hp = cfg.hp;
    this.cooldown = 1 + Math.random();
    this.radius = 0.6 * this.scale;
    this.flash = 0;
    this.deadTime = 0;
    this.summons = 0;

    if (opts.dormant) {
      this.state = 'dormant';
      this.anim.play('Skeletons_Inactive_Floor_Pose', { fade: 0 });
    } else {
      this.state = 'idle';
      this.anim.play(cfg.boss ? 'Idle_Combat' : 'Idle', { fade: 0 });
    }
    this.sync();
  }

  attach(key, bone) {
    const slot = this.model.getObjectByName(bone);
    if (!slot) return;
    const w = cloneModel(key);
    slot.add(w);
  }

  get alive() {
    return this.state !== 'dead';
  }

  get awake() {
    return this.state !== 'dormant' && this.state !== 'rising';
  }

  wake() {
    if (this.state !== 'dormant') return;
    this.state = 'rising';
    this.manager.events.emit('rise', this);
    this.anim.play('Skeletons_Awaken_Floor', { fade: 0.1, once: true, speed: 1.3, onDone: () => this.state === 'rising' && (this.state = 'chase') });
  }

  takeDamage(dmg, from, knock = 4) {
    if (!this.alive) return;
    if (this.state === 'dormant') this.wake();
    this.hp -= dmg;
    this.flash = 0.15;
    this.manager.alertGroup(this);
    if (this.hp <= 0) {
      this.hp = 0;
      this.die();
      return;
    }
    tmp.subVectors(this.pos, from).setY(0).normalize();
    const resist = this.cfg.boss ? 0.15 : this.cfg.armored ? 0.5 : 1;
    this.vel.addScaledVector(tmp, knock * resist);
    const stagger = this.cfg.boss ? Math.random() < 0.12 : this.state !== 'attack' || Math.random() < 0.5;
    if (stagger && this.awake) {
      this.state = 'hit';
      this.anim.play(Math.random() < 0.5 ? 'Hit_A' : 'Hit_B', { fade: 0.05, once: true, restart: true, speed: 1.4, onDone: () => this.state === 'hit' && (this.state = 'chase') });
    }
  }

  die() {
    this.state = 'dead';
    const anim = this.cfg.boss ? 'Death_A' : Math.random() < 0.6 ? 'Death_C_Skeletons' : 'Death_B';
    this.anim.play(anim, { fade: 0.1, once: true });
    this.manager.events.emit('enemyDied', this);
  }

  update(dt, player) {
    const cfg = this.cfg;
    this.flash = Math.max(0, this.flash - dt);
    this.cooldown -= dt;
    const toPlayer = tmp.subVectors(player.pos, this.pos);
    const dy = Math.abs(toPlayer.y);
    toPlayer.y = 0;
    const dist = toPlayer.length();

    if (this.state === 'dead') {
      this.deadTime += dt;
      if (this.deadTime > 4) this.pos.y -= dt * 0.6;
      this.anim.update(dt);
      this.sync();
      return this.deadTime < 6.5;
    }

    if (this.state === 'dormant') {
      if (player.alive && dist < cfg.aggro * 0.75 && dy < 4) {
        this.wake();
        this.manager.alertGroup(this);
      }
      this.anim.update(dt);
      this.sync();
      return true;
    }

    let wish = 0;
    const face = (yaw, rate) => {
      let d = yaw - this.facing;
      d = Math.atan2(Math.sin(d), Math.cos(d));
      this.facing += d * Math.min(1, rate * dt);
    };
    const yawToPlayer = Math.atan2(toPlayer.x, toPlayer.z);

    switch (this.state) {
      case 'idle':
        if (player.alive && dist < cfg.aggro && dy < 5) this.state = 'chase';
        this.anim.play(cfg.boss ? 'Idle_Combat' : 'Idle');
        break;
      case 'chase': {
        if (!player.alive) {
          this.state = 'return';
          break;
        }
        const leash = this.pos.distanceTo(this.home);
        if (leash > (cfg.boss ? 30 : 45) || dist > cfg.aggro * 2.2) {
          this.state = 'return';
          break;
        }
        face(yawToPlayer, 8);
        if (cfg.ranged) {
          if (dist < 7) wish = -0.6;
          else if (dist > cfg.range) wish = 1;
          if (dist <= cfg.range && this.cooldown <= 0) this.attack();
          else this.anim.play(wish > 0 ? cfg.run : wish < 0 ? 'Walking_Backwards' : 'Idle_Combat', { fade: 0.2 });
        } else if (dist > cfg.range * 0.9) {
          wish = 1;
          this.anim.play(dist > 6 ? cfg.run : 'Walking_D_Skeletons', { fade: 0.2, speed: dist > 6 ? 1 : 1.3 });
        } else if (this.cooldown <= 0 && dy < 2.5) {
          this.attack();
        } else {
          this.anim.play('Idle_Combat', { fade: 0.2 });
        }
        if (cfg.boss && this.summons < 2 && this.hp < this.maxHp * (this.summons === 0 ? 0.6 : 0.3)) this.summon();
        break;
      }
      case 'attack': {
        face(yawToPlayer, this.cfg.boss ? 2.5 : 4);
        const p = this.anim.progress();
        if (!this.struck && p >= this.hitAt) {
          this.struck = true;
          if (cfg.ranged) {
            this.manager.castBolt(this, player);
          } else {
            const spin = this.attackName.includes('Spin');
            const reach = cfg.range + 0.8 + (spin ? 1.2 : 0);
            const fwd = new THREE.Vector3(Math.sin(this.facing), 0, Math.cos(this.facing));
            const inFront = spin || fwd.dot(toPlayer.clone().normalize()) > 0.2;
            if (dist < reach && inFront && dy < 2.2) player.takeDamage(cfg.dmg, this.pos);
            this.manager.events.emit('enemySwing', this);
            if (cfg.boss) this.manager.events.emit('bossSlam', this);
          }
        }
        if (this.attackName === '1H_Melee_Attack_Jump_Chop' && p < this.hitAt) wish = 1.3;
        break;
      }
      case 'summon':
        break;
      case 'hit':
        break;
      case 'return': {
        const back = new THREE.Vector3().subVectors(this.home, this.pos).setY(0);
        if (back.length() < 1.5) {
          this.state = 'idle';
          this.hp = this.maxHp;
        } else {
          face(Math.atan2(back.x, back.z), 6);
          wish = 0.7;
          this.anim.play('Walking_D_Skeletons', { fade: 0.2 });
          this.hp = Math.min(this.maxHp, this.hp + dt * 20);
        }
        if (player.alive && dist < cfg.aggro * 0.6) this.state = 'chase';
        break;
      }
    }

    // movement
    const speed = cfg.speed * wish;
    const tvx = Math.sin(this.facing) * speed;
    const tvz = Math.cos(this.facing) * speed;
    const a = Math.min(1, 8 * dt);
    this.vel.x += (tvx - this.vel.x) * a;
    this.vel.z += (tvz - this.vel.z) * a;
    this.pos.x += this.vel.x * dt;
    this.pos.z += this.vel.z * dt;
    this.manager.physics.resolve(this.pos, this.radius, 2 * this.scale);
    const g = this.manager.physics.ground(this.pos.x, this.pos.z, this.pos.y + 0.5).y;
    this.pos.y += (g - this.pos.y) * Math.min(1, dt * 12);

    this.anim.update(dt);
    this.sync();
    return true;
  }

  attack() {
    const list = this.cfg.attacks;
    let name = list[Math.floor(Math.random() * list.length)];
    this.state = 'attack';
    this.struck = false;
    this.attackName = name;
    this.hitAt = name.includes('Spin') ? 0.55 : name.includes('Spellcast') ? 0.45 : 0.48;
    const speed = this.cfg.boss ? 0.95 : 1.1;
    this.cooldown = this.cfg.cooldown * (0.8 + Math.random() * 0.5);
    this.anim.play(name, {
      fade: 0.12, once: true, restart: true, speed,
      onDone: () => this.state === 'attack' && (this.state = 'chase'),
    });
  }

  summon() {
    this.summons++;
    this.state = 'summon';
    this.anim.play('Spellcast_Summon', {
      fade: 0.15, once: true, restart: true,
      onDone: () => {
        if (this.state !== 'summon') return;
        this.state = 'chase';
        for (let i = 0; i < 3; i++) {
          const a = this.facing + (i - 1) * 1.2 + Math.PI;
          const e = this.manager.spawn('minion', this.pos.x + Math.sin(a) * 5, this.pos.z + Math.cos(a) * 5, { dormant: true, group: this.group });
          e.wake();
        }
        this.manager.events.emit('summon', this);
      },
    });
  }

  sync() {
    this.model.position.copy(this.pos);
    this.model.rotation.y = this.facing;
    const f = this.flash > 0 ? 1 : 0;
    if (f !== this._lastFlash) {
      this._lastFlash = f;
      this.model.traverse((o) => {
        if (o.isMesh && o.material && 'emissiveIntensity' in o.material) {
          if (!o.userData.ownMat) {
            o.material = o.material.clone();
            o.userData.ownMat = true;
            o.userData.baseEmissive = o.material.emissive.clone();
          }
          o.material.emissive.copy(f ? new THREE.Color(0xff5533) : o.userData.baseEmissive);
        }
      });
    }
  }

  dispose() {
    this.model.removeFromParent();
  }
}

// Owns every skeleton, their spell bolts and group bookkeeping for the quests.
export class Enemies {
  constructor(scene, physics, events, fx) {
    this.scene = scene;
    this.physics = physics;
    this.events = events;
    this.fx = fx;
    this.list = [];
    this.bolts = [];
    this.boltGeo = new THREE.SphereGeometry(0.28, 16, 12);
    this.boltMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0xb36bff).multiplyScalar(3) });
  }

  spawn(type, x, z, opts = {}) {
    const e = new Enemy(this, type, x, z, opts);
    this.list.push(e);
    return e;
  }

  alertGroup(src) {
    if (!src.group) return;
    for (const e of this.list) {
      if (e.group === src.group && e !== src && e.state === 'dormant') {
        setTimeout(() => e.wake(), 200 + Math.random() * 900);
      }
    }
  }

  aliveIn(group) {
    return this.list.filter((e) => e.group === group && e.alive).length;
  }

  totalIn(group) {
    return this.list.filter((e) => e.group === group).length;
  }

  castBolt(enemy, player) {
    const start = enemy.pos.clone().add(new THREE.Vector3(Math.sin(enemy.facing) * 0.8, 1.7, Math.cos(enemy.facing) * 0.8));
    const target = player.pos.clone().add(new THREE.Vector3(0, 1.1, 0));
    const vel = target.sub(start).normalize().multiplyScalar(15);
    const mesh = new THREE.Mesh(this.boltGeo, this.boltMat);
    mesh.position.copy(start);
    this.scene.add(mesh);
    this.bolts.push({ mesh, vel, life: 3, dmg: enemy.cfg.dmg, from: enemy.pos.clone() });
    this.events.emit('cast', enemy);
  }

  // sword arc test used by the hero's strikes
  strike(origin, facing, reach, arc, dmg) {
    const fwd = new THREE.Vector3(Math.sin(facing), 0, Math.cos(facing));
    const hits = [];
    for (const e of this.list) {
      if (!e.alive) continue;
      const d = new THREE.Vector3().subVectors(e.pos, origin);
      const dy = d.y;
      d.y = 0;
      const dist = d.length();
      if (dist > reach + e.radius || Math.abs(dy) > 2.5 * e.scale) continue;
      if (dist > 0.8 && fwd.dot(d.normalize()) < Math.cos(arc)) continue;
      e.takeDamage(dmg, origin);
      hits.push(e);
    }
    return hits;
  }

  nearestAwake(pos, maxDist) {
    let best = null, bd = maxDist;
    for (const e of this.list) {
      if (!e.alive || !e.awake) continue;
      const d = e.pos.distanceTo(pos);
      if (d < bd) {
        bd = d;
        best = e;
      }
    }
    return best;
  }

  update(dt, player) {
    for (let i = this.list.length - 1; i >= 0; i--) {
      const e = this.list[i];
      if (!e.update(dt, player)) {
        e.dispose();
        this.list.splice(i, 1);
      }
    }
    // keep skeletons from stacking on each other
    for (let i = 0; i < this.list.length; i++) {
      const a = this.list[i];
      if (!a.alive) continue;
      for (let j = i + 1; j < this.list.length; j++) {
        const b = this.list[j];
        if (!b.alive) continue;
        const dx = b.pos.x - a.pos.x, dz = b.pos.z - a.pos.z;
        const d = Math.hypot(dx, dz);
        const min = a.radius + b.radius;
        if (d < min && d > 1e-4) {
          const push = (min - d) / 2;
          a.pos.x -= (dx / d) * push;
          a.pos.z -= (dz / d) * push;
          b.pos.x += (dx / d) * push;
          b.pos.z += (dz / d) * push;
        }
      }
      if (player.alive) {
        const dx = player.pos.x - a.pos.x, dz = player.pos.z - a.pos.z;
        const d = Math.hypot(dx, dz);
        const min = a.radius + 0.5;
        if (d < min && d > 1e-4 && Math.abs(player.pos.y - a.pos.y) < 1.8) {
          player.pos.x = a.pos.x + (dx / d) * min;
          player.pos.z = a.pos.z + (dz / d) * min;
        }
      }
    }

    for (let i = this.bolts.length - 1; i >= 0; i--) {
      const b = this.bolts[i];
      b.life -= dt;
      b.mesh.position.addScaledVector(b.vel, dt);
      this.fx.trail(b.mesh.position);
      const hitPlayer = player.alive && b.mesh.position.distanceTo(tmp.copy(player.pos).setY(player.pos.y + 1.1)) < 0.9;
      const ground = this.physics.ground(b.mesh.position.x, b.mesh.position.z, b.mesh.position.y).y;
      if (hitPlayer || b.life <= 0 || b.mesh.position.y < ground) {
        if (hitPlayer) player.takeDamage(b.dmg, b.from);
        this.fx.burst(b.mesh.position, 'magic', 24);
        b.mesh.removeFromParent();
        this.bolts.splice(i, 1);
      }
    }
  }

  clear() {
    for (const e of this.list) e.dispose();
    this.list.length = 0;
    for (const b of this.bolts) b.mesh.removeFromParent();
    this.bolts.length = 0;
  }
}
