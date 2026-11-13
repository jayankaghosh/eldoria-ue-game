// World layout and terrain shape, shared by the renderer, physics, quest logic and minimap.

export const WORLD_SIZE = 360;
export const WORLD_HALF = WORLD_SIZE / 2;
export const PLAY_RADIUS = 138;
export const WATER_LEVEL = -1.1;

export const PLACES = {
  village: { x: 0, z: 58, r: 30 },
  spawn: { x: 0, z: 40 },
  westCamp: { x: -86, z: -4, r: 16 },
  eastRuins: { x: 84, z: -26, r: 16 },
  skyTop: { x: 60, z: -62 },
  castle: { x: 0, z: -104 },
  courtyard: { x: 0, z: -84, r: 15 },
  gate: { x: 0, z: -68 },
  lake: { x: 58, z: 64, r: 22 },
};

export const PATHS = [
  [[0, 92], [0, 58], [1, 25], [3, -10], [0, -40], [0, -68], [0, -90]],
  [[1, 25], [-28, 16], [-58, 6], [-86, -4]],
  [[3, -10], [34, -16], [62, -22], [84, -26]],
  [[0, 58], [30, 60], [40, 70]],
];

// ---------- noise ----------
function hash(x, z) {
  const s = Math.sin(x * 127.1 + z * 311.7) * 43758.5453;
  return s - Math.floor(s);
}
function vnoise(x, z) {
  const xi = Math.floor(x), zi = Math.floor(z);
  const xf = x - xi, zf = z - zi;
  const u = xf * xf * (3 - 2 * xf), v = zf * zf * (3 - 2 * zf);
  const a = hash(xi, zi), b = hash(xi + 1, zi), c = hash(xi, zi + 1), d = hash(xi + 1, zi + 1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
export function fbm(x, z, oct = 4) {
  let s = 0, a = 0.5, f = 1;
  for (let i = 0; i < oct; i++) {
    s += a * vnoise(x * f, z * f);
    f *= 2.03;
    a *= 0.5;
  }
  return s;
}

const smooth = (e0, e1, x) => {
  const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0)));
  return t * t * (3 - 2 * t);
};
const mix = (a, b, t) => a + (b - a) * t;
const dist = (x, z, p) => Math.hypot(x - p.x, z - p.z);

function segDist(px, pz, ax, az, bx, bz) {
  const dx = bx - ax, dz = bz - az;
  const t = Math.max(0, Math.min(1, ((px - ax) * dx + (pz - az) * dz) / (dx * dx + dz * dz)));
  return Math.hypot(px - (ax + dx * t), pz - (az + dz * t));
}

export function pathDistance(x, z) {
  let d = Infinity;
  for (const path of PATHS) {
    for (let i = 0; i < path.length - 1; i++) {
      d = Math.min(d, segDist(x, z, path[i][0], path[i][1], path[i + 1][0], path[i + 1][1]));
    }
  }
  return d;
}

export function terrainHeight(x, z) {
  let h = fbm(x * 0.016 + 10, z * 0.016 + 3) * 7 - 3 + fbm(x * 0.07, z * 0.07, 2) * 0.7;

  // flatten the village, the camps and the roads
  h = mix(h, 0.4, smooth(PLACES.village.r + 16, PLACES.village.r, dist(x, z, PLACES.village)));
  h = mix(h, 0.8, smooth(PLACES.westCamp.r + 14, PLACES.westCamp.r, dist(x, z, PLACES.westCamp)));
  const pd = pathDistance(x, z);
  h = mix(h, h * 0.4 + 0.3, smooth(7, 2, pd));

  // the hill of the old ruins
  const dr = dist(x, z, PLACES.eastRuins);
  h = mix(h, 5.5, smooth(PLACES.eastRuins.r + 18, PLACES.eastRuins.r - 2, dr));

  // castle plateau with a road ramp in front
  const dc = dist(x, z, PLACES.castle);
  h = mix(h, 7, smooth(58, 40, dc));

  // lake basin
  h -= 5 * smooth(PLACES.lake.r + 8, PLACES.lake.r - 10, dist(x, z, PLACES.lake));

  // the kingdom is ringed by highlands
  const r = Math.hypot(x, z);
  h += smooth(128, 178, r) * 34 + smooth(150, 178, r) * fbm(x * 0.05, z * 0.05) * 14;
  return h;
}

// 0 = grass, 1 = dirt road, used for colouring and keeping props off roads
export function roadFactor(x, z) {
  return smooth(3.6, 2.2, pathDistance(x, z));
}
