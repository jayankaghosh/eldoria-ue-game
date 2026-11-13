import * as THREE from 'three';
import { cloneModel } from './assets.js';

const tmp = new THREE.Vector3();

export class Pickups {
  constructor(scene, physics, events, fx) {
    this.scene = scene;
    this.physics = physics;
    this.events = events;
    this.fx = fx;
    this.items = [];
    this.chests = [];
    this.time = 0;
    this.totalCoins = 0;
  }

  add(kind, x, y, z, opts = {}) {
    const model = cloneModel(kind === 'coin' ? 'coin' : kind === 'potion' ? 'potion' : 'key');
    const holder = new THREE.Group();
    holder.add(model);
    if (kind === 'coin') {
      model.rotation.x = Math.PI / 2;
      model.scale.setScalar(2.2);
    } else if (kind === 'potion') {
      model.scale.setScalar(1.3);
    } else {
      model.scale.setScalar(2.2);
    }
    model.traverse((o) => {
      if (o.isMesh) {
        o.castShadow = true;
        o.receiveShadow = false;
      }
    });
    holder.position.set(x, y, z);
    this.scene.add(holder);
    const item = { kind, holder, model, pos: holder.position, vel: opts.vel ? opts.vel.clone() : null, phase: Math.random() * 6, age: 0, baseY: y };
    if (kind === 'coin' && !opts.drop) this.totalCoins++;
    this.items.push(item);
    return item;
  }

  coin(x, z, y, opts) {
    const g = y ?? this.physics.ground(x, z, 200).y;
    return this.add('coin', x, g + 1.0, z, opts);
  }

  coinBurst(pos, n) {
    for (let i = 0; i < n; i++) {
      const a = Math.random() * Math.PI * 2;
      const s = 2 + Math.random() * 3;
      this.add('coin', pos.x, pos.y + 1.2, pos.z, { drop: true, vel: new THREE.Vector3(Math.cos(a) * s, 6 + Math.random() * 4, Math.sin(a) * s) });
    }
  }

  potion(x, z, y) {
    const g = y ?? this.physics.ground(x, z, 200).y;
    return this.add('potion', x, g + 0.9, z);
  }

  chest(x, z, rot, contents, opts = {}) {
    const gold = !!opts.gold;
    const obj = cloneModel(gold ? 'chest_gold' : 'chest');
    const y = opts.y ?? this.physics.ground(x, z, 200).y;
    obj.position.set(x, y, z);
    obj.rotation.y = rot;
    obj.scale.setScalar(gold ? 1.1 : 0.9);
    this.scene.add(obj);
    const lid = obj.getObjectByName(gold ? 'chest_gold_lid' : 'chest_lid');
    this.physics.addBox(x, z, 0.85, 0.7, -rot, y - 1, y + 1.1);
    const c = { obj, lid, pos: obj.position, rot, contents, opened: false, open: 0, glow: null };
    if (gold) {
      const light = new THREE.PointLight(0xffc35a, 10, 8, 2);
      light.position.set(x, y + 1.8, z);
      this.scene.add(light);
      c.glow = light;
    }
    this.chests.push(c);
    return c;
  }

  nearestChest(pos) {
    let best = null, bd = 2.8;
    for (const c of this.chests) {
      if (c.opened) continue;
      const d = c.pos.distanceTo(pos);
      if (d < bd) {
        bd = d;
        best = c;
      }
    }
    return best;
  }

  openChest(c) {
    c.opened = true;
    this.events.emit('chestOpen', c);
    setTimeout(() => {
      const top = c.pos.clone().add(new THREE.Vector3(0, 1, 0));
      this.fx.burst(top, 'coin', 40, { up: 1.5 });
      if (c.contents.coins) this.coinBurst(c.pos, c.contents.coins);
      if (c.contents.potion) this.add('potion', c.pos.x, c.pos.y + 1.4, c.pos.z, { vel: new THREE.Vector3(0, 6, 1.5) });
      if (c.contents.key) {
        const k = this.add('key', c.pos.x, c.pos.y + 1.6, c.pos.z);
        k.isKey = true;
      }
    }, 450);
  }

  update(dt, player) {
    this.time += dt;
    const target = tmp.copy(player.pos).setY(player.pos.y + 1);
    for (let i = this.items.length - 1; i >= 0; i--) {
      const it = this.items[i];
      it.age += dt;
      if (it.vel) {
        it.vel.y -= 22 * dt;
        it.pos.addScaledVector(it.vel, dt);
        const g = this.physics.ground(it.pos.x, it.pos.z, it.pos.y + 1).y + 0.5;
        if (it.pos.y < g) {
          it.pos.y = g;
          it.vel.y = Math.abs(it.vel.y) * 0.35;
          it.vel.x *= 0.6;
          it.vel.z *= 0.6;
          if (Math.abs(it.vel.y) < 1) {
            it.vel = null;
            it.baseY = g + 0.4;
          }
        }
      } else {
        it.pos.y = it.baseY + Math.sin(this.time * 2.4 + it.phase) * 0.15;
      }
      it.model.rotation.y = this.time * (it.kind === 'coin' ? 3 : 1.4) + it.phase;
      if (it.kind === 'coin') it.model.rotation.z = this.time * 3 + it.phase;

      if (!player.alive || it.age < 0.4) continue;
      const d = it.pos.distanceTo(target);
      // coins drift toward the hero once close
      if (it.kind === 'coin' && d < 3.2) {
        it.vel = null;
        it.pos.lerp(target, Math.min(1, dt * 10));
        it.baseY = it.pos.y;
      }
      if (d < 1.3) {
        if (it.kind === 'potion' && player.hp >= player.maxHp) continue;
        this.collect(it);
        it.holder.removeFromParent();
        this.items.splice(i, 1);
      }
    }

    for (const c of this.chests) {
      if (c.opened && c.open < 1) {
        c.open = Math.min(1, c.open + dt * 2.2);
        const e = 1 - Math.pow(1 - c.open, 3);
        if (c.lid) c.lid.rotation.x = -1.9 * e;
      }
      if (c.glow) c.glow.intensity = c.opened ? Math.max(0, c.glow.intensity - dt * 4) : 8 + Math.sin(this.time * 3) * 2;
    }
  }

  collect(it) {
    if (it.kind === 'coin') {
      this.fx.burst(it.pos, 'coin', 10);
      this.events.emit('coin');
    } else if (it.kind === 'potion') {
      this.fx.burst(it.pos, 'heal', 30);
      this.events.emit('potion');
    } else if (it.isKey) {
      this.fx.burst(it.pos, 'coin', 60, { speed: 1.5 });
      this.events.emit('key');
    }
  }
}
