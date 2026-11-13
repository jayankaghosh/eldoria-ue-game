import * as THREE from 'three';
import { cloneModel, animationsOf } from './assets.js';
import { Animator } from './animator.js';
import { WATER_LEVEL } from './layout.js';

const WALK = 3.2;
const RUN = 6.4;
const SPRINT = 9.5;
const JUMP_V = 11;
const DOUBLE_JUMP_V = 10;
const GRAVITY = 28;
const RADIUS = 0.55;

const COMBO = [
  { anim: '1H_Melee_Attack_Slice_Diagonal', hit: 0.38, dmg: 20, speed: 1.45 },
  { anim: '1H_Melee_Attack_Slice_Horizontal', hit: 0.38, dmg: 22, speed: 1.45 },
  { anim: '1H_Melee_Attack_Chop', hit: 0.42, dmg: 36, speed: 1.3 },
];

const HIDDEN_PARTS = ['1H_Sword_Offhand', '2H_Sword', 'Rectangle_Shield', 'Spike_Shield', 'Round_Shield'];

export class Player {
  constructor(scene, physics, events) {
    this.physics = physics;
    this.events = events;
    this.model = cloneModel('knight');
    this.model.traverse((o) => {
      if (HIDDEN_PARTS.includes(o.name)) o.visible = false;
    });
    this.root = new THREE.Group();
    this.root.add(this.model);
    scene.add(this.root);
    this.anim = new Animator(this.model, animationsOf('knight'));

    this.pos = new THREE.Vector3();
    this.vel = new THREE.Vector3();
    this.facing = 0;
    this.maxHp = 100;
    this.reset(new THREE.Vector3(), 0);
  }

  reset(pos, facing) {
    this.pos.copy(pos);
    this.vel.set(0, 0, 0);
    this.facing = facing;
    this.hp = this.maxHp;
    this.state = 'free';
    this.onGround = true;
    this.coyote = 0;
    this.jumps = 0;
    this.combo = 0;
    this.queued = false;
    this.hitDone = false;
    this.invuln = 0;
    this.blocking = false;
    this.stepDist = 0;
    this.standingOn = null;
    this.airTime = 0;
    this.anim.play('Idle', { fade: 0 });
    this.sync();
  }

  get alive() {
    return this.state !== 'dead';
  }

  get isInvulnerable() {
    return this.invuln > 0 || this.state === 'dodge';
  }

  forward(out = new THREE.Vector3()) {
    return out.set(Math.sin(this.facing), 0, Math.cos(this.facing));
  }

  // ---------- combat ----------

  startAttack() {
    const step = COMBO[this.combo];
    this.state = 'attack';
    this.hitDone = false;
    this.queued = false;
    this.anim.play(step.anim, {
      fade: 0.08,
      speed: step.speed,
      once: true,
      restart: true,
      onDone: () => {
        if (this.state !== 'attack') return;
        if (this.queued && this.combo < COMBO.length - 1) {
          this.combo++;
          this.startAttack();
        } else {
          this.combo = 0;
          this.state = 'free';
        }
      },
    });
    this.events.emit('swing', this.combo);
  }

  takeDamage(amount, from) {
    if (!this.alive || this.isInvulnerable) return false;
    const toEnemy = new THREE.Vector3().subVectors(from, this.pos).setY(0).normalize();
    const facingEnemy = this.forward().dot(toEnemy) > 0.3;
    if (this.blocking && facingEnemy) {
      amount *= 0.15;
      this.anim.play('Block_Hit', { fade: 0.05, once: true, restart: true, speed: 1.4, onDone: () => this.anim.play('Blocking', { fade: 0.1 }) });
      this.events.emit('blocked');
      this.vel.addScaledVector(toEnemy, -4);
    } else {
      this.events.emit('hurt');
      this.vel.addScaledVector(toEnemy, -7);
      this.vel.y = Math.max(this.vel.y, 3);
      this.onGround = false;
      if (this.state !== 'dead') {
        this.state = 'hit';
        this.anim.play(Math.random() < 0.5 ? 'Hit_A' : 'Hit_B', { fade: 0.05, once: true, restart: true, speed: 1.5, onDone: () => this.state === 'hit' && (this.state = 'free') });
      }
    }
    this.hp = Math.max(0, this.hp - amount);
    this.invuln = 0.45;
    if (this.hp <= 0) this.die();
    return true;
  }

  die() {
    this.state = 'dead';
    this.blocking = false;
    this.anim.play('Death_A', { fade: 0.15, once: true });
    this.events.emit('died');
  }

  heal(n) {
    this.hp = Math.min(this.maxHp, this.hp + n);
  }

  // ---------- frame ----------

  update(dt, input, camYaw) {
    this.invuln = Math.max(0, this.invuln - dt);
    const alive = this.alive;

    // desired movement relative to the camera
    const wish = new THREE.Vector3();
    if (alive && (input.move.x || input.move.y)) {
      const sin = Math.sin(camYaw), cos = Math.cos(camYaw);
      // camera looks along -z when yaw = 0
      wish.set(input.move.x * cos - input.move.y * sin, 0, -input.move.x * sin - input.move.y * cos);
      if (wish.lengthSq() > 1) wish.normalize();
    }

    this.blocking = alive && input.block && this.onGround && (this.state === 'free' || this.state === 'block');

    if (alive) {
      if (input.attack) {
        if (this.state === 'free' || this.state === 'block') {
          this.combo = 0;
          this.startAttack();
        } else if (this.state === 'attack' && this.anim.progress() > 0.3) {
          this.queued = true;
        }
      }
      if (input.dodge && (this.state === 'free' || this.state === 'attack' || this.state === 'block') && this.onGround) {
        const dir = wish.lengthSq() > 0.01 ? wish.clone().normalize() : this.forward();
        this.facing = Math.atan2(dir.x, dir.z);
        this.state = 'dodge';
        this.dodgeDir = dir;
        this.anim.play('Dodge_Forward', { fade: 0.06, once: true, restart: true, speed: 1.5, onDone: () => this.state === 'dodge' && (this.state = 'free') });
        this.events.emit('dodge');
      }
      if (input.jump && (this.state === 'free' || this.state === 'attack' || this.state === 'block')) {
        if (this.onGround || this.coyote > 0) {
          this.vel.y = JUMP_V;
          this.onGround = false;
          this.coyote = 0;
          this.jumps = 1;
          this.state = 'free';
          this.anim.play('Jump_Start', { fade: 0.05, once: true, speed: 1.6, restart: true, onDone: () => !this.onGround && this.anim.play('Jump_Idle', { fade: 0.1 }) });
          this.events.emit('jump', false);
        } else if (this.jumps < 2) {
          this.vel.y = DOUBLE_JUMP_V;
          this.jumps = 2;
          this.state = 'free';
          this.anim.play('Jump_Start', { fade: 0.05, once: true, speed: 2, restart: true, onDone: () => !this.onGround && this.anim.play('Jump_Idle', { fade: 0.1 }) });
          this.events.emit('jump', true);
        }
      }
    }

    // horizontal velocity
    let maxSpeed = input.sprint ? SPRINT : RUN;
    if (this.blocking) maxSpeed = WALK * 0.6;
    if (this.state === 'attack') maxSpeed = this.onGround ? 1.2 : RUN * 0.6;
    if (this.state === 'hit' || !alive) maxSpeed = 0;
    const inWater = this.pos.y < WATER_LEVEL - 0.3;
    if (inWater) maxSpeed *= 0.55;

    if (this.state === 'dodge') {
      const t = this.anim.progress();
      const sp = 12 * (1 - t * 0.7);
      this.vel.x = this.dodgeDir.x * sp;
      this.vel.z = this.dodgeDir.z * sp;
    } else {
      const target = wish.clone().multiplyScalar(maxSpeed);
      const accel = this.onGround ? (this.state === 'hit' ? 3 : 14) : 5;
      this.vel.x += (target.x - this.vel.x) * Math.min(1, accel * dt);
      this.vel.z += (target.z - this.vel.z) * Math.min(1, accel * dt);
    }

    // turn toward travel (or keep facing during attacks unless steering)
    if (wish.lengthSq() > 0.01 && this.state !== 'dodge' && this.state !== 'hit' && alive) {
      const targetYaw = Math.atan2(wish.x, wish.z);
      let d = targetYaw - this.facing;
      d = Math.atan2(Math.sin(d), Math.cos(d));
      const turn = this.state === 'attack' ? 6 : 14;
      this.facing += d * Math.min(1, turn * dt);
    }

    // gravity + integration
    this.vel.y -= GRAVITY * dt;
    if (this.standingOn && this.onGround) {
      this.pos.x += this.standingOn.dx;
      this.pos.z += this.standingOn.dz;
    }
    const prevY = this.pos.y;
    this.pos.addScaledVector(this.vel, dt);
    this.physics.resolve(this.pos, RADIUS, 2.2);

    const g = this.physics.ground(this.pos.x, this.pos.z, Math.max(prevY, this.pos.y), RADIUS);
    if (this.pos.y <= g.y + 0.001 && this.vel.y <= 0) {
      const fallSpeed = -this.vel.y;
      this.pos.y = g.y;
      this.vel.y = 0;
      this.standingOn = g.on;
      if (!this.onGround) {
        this.onGround = true;
        this.jumps = 0;
        if (this.airTime > 0.25) {
          this.events.emit('land', fallSpeed);
          if (this.state === 'free' && alive) this.anim.play('Jump_Land', { fade: 0.05, once: true, speed: 1.8, restart: true, onDone: () => (this.landing = false) });
          this.landing = true;
        }
      }
      this.airTime = 0;
    } else if (this.pos.y > g.y + 0.25 || this.vel.y > 0) {
      if (this.onGround) this.coyote = 0.12;
      this.onGround = false;
      this.standingOn = null;
    } else {
      // walking down gentle slopes: snap to ground
      this.pos.y = g.y;
      this.vel.y = 0;
      this.standingOn = g.on;
    }
    if (!this.onGround) {
      this.airTime += dt;
      this.coyote -= dt;
    }

    // hit frame of the current swing
    if (this.state === 'attack' && !this.hitDone && this.anim.progress() >= COMBO[this.combo].hit) {
      this.hitDone = true;
      this.events.emit('strike', { damage: COMBO[this.combo].dmg, combo: this.combo });
    }

    this.locomotion(dt);
    this.anim.update(dt);
    this.sync();
  }

  locomotion(dt) {
    if (!this.alive) return;
    const hs = Math.hypot(this.vel.x, this.vel.z);
    if (this.state === 'free') {
      if (this.blocking) {
        this.anim.play('Blocking', { fade: 0.12 });
        return;
      }
      if (!this.onGround) {
        if (this.anim.currentName !== 'Jump_Start' && this.airTime > 0.15) this.anim.play('Jump_Idle', { fade: 0.15 });
        return;
      }
      if (this.landing && hs < 1) return;
      if (hs > 7.6) this.anim.play('Running_B', { fade: 0.15, speed: hs / 9.5 * 1.05 });
      else if (hs > 3.6) this.anim.play('Running_A', { fade: 0.15, speed: hs / RUN });
      else if (hs > 0.4) this.anim.play('Walking_A', { fade: 0.2, speed: Math.max(0.6, hs / WALK) });
      else this.anim.play('Idle', { fade: 0.25 });

      if (hs > 0.4) {
        this.stepDist += hs * dt;
        const stride = hs > 7 ? 2.6 : 1.9;
        if (this.stepDist > stride) {
          this.stepDist = 0;
          this.events.emit('step', hs);
        }
      }
    }
  }

  sync() {
    this.root.position.copy(this.pos);
    this.model.rotation.y = this.facing;
    // blink while invulnerable after a hit
    this.model.visible = !(this.invuln > 0 && this.state !== 'dead' && Math.sin(performance.now() * 0.04) > 0.6);
  }
}
