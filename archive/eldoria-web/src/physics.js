import { terrainHeight, PLAY_RADIUS } from './layout.js';

const STEP_UP = 0.6;

// Simple static world collision: vertical cylinders and oriented boxes.
// Boxes marked walkable can be stood on, which is what makes the platforms work.
export class Physics {
  constructor() {
    this.circles = [];
    this.boxes = [];
  }

  // vr is the visual radius (e.g. a tree canopy) the camera should stay out of
  addCircle(x, z, r, top = Infinity, vr = r) {
    const c = { x, z, r, top, vr, enabled: true };
    this.circles.push(c);
    return c;
  }

  addBox(x, z, hw, hd, rot = 0, bottom = -100, top = 100, walkable = false) {
    const b = { x, z, hw, hd, rot, cos: Math.cos(rot), sin: Math.sin(rot), bottom, top, walkable, enabled: true, dx: 0, dz: 0, dy: 0 };
    this.boxes.push(b);
    return b;
  }

  moveBox(b, x, y, z) {
    const h = b.top - b.bottom;
    b.dx = x - b.x;
    b.dz = z - b.z;
    b.dy = y - b.top;
    b.x = x;
    b.z = z;
    b.top = y;
    b.bottom = y - h;
  }

  local(b, x, z) {
    const dx = x - b.x, dz = z - b.z;
    return [dx * b.cos + dz * b.sin, -dx * b.sin + dz * b.cos];
  }

  // Highest walkable surface under (x, z) that is not above the feet plus a small step.
  ground(x, z, y, radius = 0.3) {
    let g = terrainHeight(x, z);
    let on = null;
    for (const b of this.boxes) {
      if (!b.enabled || !b.walkable) continue;
      const [lx, lz] = this.local(b, x, z);
      if (Math.abs(lx) <= b.hw + radius * 0.5 && Math.abs(lz) <= b.hd + radius * 0.5 && b.top <= y + STEP_UP && b.top > g) {
        g = b.top;
        on = b;
      }
    }
    return { y: g, on };
  }

  // Distance along the ray from `from` toward `to` that stays clear of solid objects.
  cameraClear(from, to, pad = 0.4) {
    const dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
    const len = Math.hypot(dx, dy, dz);
    const steps = Math.ceil(len / 0.35);
    for (let i = 1; i <= steps; i++) {
      const t = i / steps;
      const x = from.x + dx * t, y = from.y + dy * t, z = from.z + dz * t;
      for (const c of this.circles) {
        if (!c.enabled || y > c.top) continue;
        if (Math.abs(x - c.x) > c.vr + pad || Math.abs(z - c.z) > c.vr + pad) continue;
        if (Math.hypot(x - c.x, z - c.z) < c.vr + pad) return Math.max(0, (i - 1) / steps) * len;
      }
      for (const b of this.boxes) {
        if (!b.enabled || y > b.top + pad || y < b.bottom) continue;
        const [lx, lz] = this.local(b, x, z);
        if (Math.abs(lx) < b.hw + pad && Math.abs(lz) < b.hd + pad) return Math.max(0, (i - 1) / steps) * len;
      }
    }
    return len;
  }

  // Pushes a cylinder (feet at pos.y, height h) out of every overlapping collider.
  resolve(pos, radius, h = 2) {
    for (const c of this.circles) {
      if (!c.enabled || pos.y >= c.top - 0.05) continue;
      const dx = pos.x - c.x, dz = pos.z - c.z;
      const d = Math.hypot(dx, dz);
      const min = c.r + radius;
      if (d < min && d > 1e-5) {
        pos.x = c.x + (dx / d) * min;
        pos.z = c.z + (dz / d) * min;
      }
    }
    for (const b of this.boxes) {
      if (!b.enabled) continue;
      if (pos.y >= b.top - 0.05 || pos.y + h <= b.bottom) continue;
      if (b.walkable && pos.y >= b.top - STEP_UP) continue;
      const [lx, lz] = this.local(b, pos.x, pos.z);
      const cx = Math.max(-b.hw, Math.min(b.hw, lx));
      const cz = Math.max(-b.hd, Math.min(b.hd, lz));
      let nx = lx - cx, nz = lz - cz;
      const d = Math.hypot(nx, nz);
      let ox, oz;
      if (d > 1e-5) {
        if (d >= radius) continue;
        ox = cx + (nx / d) * radius;
        oz = cz + (nz / d) * radius;
      } else {
        // centre is inside the box: leave along the shallowest side
        const px = b.hw - Math.abs(lx), pz = b.hd - Math.abs(lz);
        if (px < pz) {
          ox = Math.sign(lx || 1) * (b.hw + radius);
          oz = lz;
        } else {
          ox = lx;
          oz = Math.sign(lz || 1) * (b.hd + radius);
        }
      }
      pos.x = b.x + ox * b.cos - oz * b.sin;
      pos.z = b.z + ox * b.sin + oz * b.cos;
    }
    const r = Math.hypot(pos.x, pos.z);
    if (r > PLAY_RADIUS) {
      pos.x *= PLAY_RADIUS / r;
      pos.z *= PLAY_RADIUS / r;
    }
  }
}
