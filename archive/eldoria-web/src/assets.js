import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { RGBELoader } from 'three/examples/jsm/loaders/RGBELoader.js';
import * as SkeletonUtils from 'three/examples/jsm/utils/SkeletonUtils.js';

THREE.Cache.enabled = true;

const BASE = 'assets/';

const CHARACTERS = {
  knight: 'characters/Knight.glb',
  minion: 'characters/Skeleton_Minion.glb',
  warrior: 'characters/Skeleton_Warrior.glb',
  rogue: 'characters/Skeleton_Rogue.glb',
  mage: 'characters/Skeleton_Mage.glb',
};

const WEAPONS = ['Skeleton_Blade', 'Skeleton_Axe', 'Skeleton_Staff', 'Skeleton_Shield_Small_A', 'Skeleton_Shield_Large_A'];

const MEDIEVAL = [
  'building_castle_blue', 'building_home_A_blue', 'building_home_B_blue', 'building_tavern_blue', 'building_church_blue',
  'building_windmill_blue', 'building_well_blue', 'building_blacksmith_blue', 'building_market_blue', 'building_tower_A_blue',
  'building_tower_B_blue', 'building_barracks_blue', 'building_tower_A_red', 'building_barracks_red', 'building_tower_catapult_red',
  'wall_straight', 'wall_straight_gate', 'wall_corner_A_outside', 'fence_wood_straight', 'fence_stone_straight',
  'building_destroyed', 'building_bridge_A',
  'tree_single_A', 'tree_single_B', 'trees_A_large', 'trees_A_medium', 'trees_B_large', 'trees_B_medium', 'trees_A_small',
  'rock_single_A', 'rock_single_B', 'rock_single_C', 'rock_single_D', 'rock_single_E',
  'mountain_A_grass_trees', 'mountain_B_grass_trees', 'mountain_C_grass', 'hill_single_A', 'hill_single_B',
  'cloud_big', 'cloud_small', 'waterplant_A', 'waterlily_A',
  'barrel', 'crate_A_big', 'crate_B_small', 'sack', 'tent', 'weaponrack', 'flag_blue', 'flag_red', 'wheelbarrow',
  'resource_lumber', 'target',
];

const DUNGEON = ['coin', 'coin_stack_small', 'chest_gold', 'chest', 'torch_lit', 'banner_patternA_blue', 'banner_shield_red', 'pillar', 'stairs', 'floor_tile_large', 'barrel_large', 'potion', 'key'];

export const assets = { models: {}, hdr: null };

export async function loadAssets(onProgress) {
  const manager = new THREE.LoadingManager();
  manager.onProgress = (_url, loaded, total) => onProgress?.(loaded / total);
  const gltfLoader = new GLTFLoader(manager);
  const jobs = [];

  const add = (key, path) =>
    jobs.push(
      gltfLoader.loadAsync(BASE + path).then((gltf) => {
        gltf.scene.traverse((o) => {
          if (o.isMesh) {
            o.castShadow = true;
            o.receiveShadow = true;
            if (o.material) {
              o.material.roughness = Math.max(o.material.roughness ?? 0.8, 0.65);
              o.material.metalness = Math.min(o.material.metalness ?? 0, 0.2);
            }
          }
        });
        assets.models[key] = gltf;
      })
    );

  for (const [k, p] of Object.entries(CHARACTERS)) add(k, p);
  for (const w of WEAPONS) add(w, `weapons/${w}.gltf`);
  for (const m of MEDIEVAL) add(m, `medieval/${m}.gltf`);
  for (const d of DUNGEON) add(d, `dungeon/${d}.glb`);

  jobs.push(
    new RGBELoader(manager).loadAsync(BASE + 'sky/sky.hdr').then((tex) => {
      tex.mapping = THREE.EquirectangularReflectionMapping;
      assets.hdr = tex;
    })
  );

  await Promise.all(jobs);
}

// Static props share geometry/material; animated characters need a deep skeleton clone.
export function cloneModel(key) {
  const gltf = assets.models[key];
  if (!gltf) throw new Error(`Missing model ${key}`);
  return CHARACTERS[key] ? SkeletonUtils.clone(gltf.scene) : gltf.scene.clone(true);
}

export function animationsOf(key) {
  return assets.models[key].animations;
}

// Collects every mesh of a static model with its transform relative to the model root,
// so the model can be drawn many times through InstancedMesh.
export function meshParts(key) {
  const root = assets.models[key].scene;
  root.updateMatrixWorld(true);
  const inv = new THREE.Matrix4().copy(root.matrixWorld).invert();
  const parts = [];
  root.traverse((o) => {
    if (o.isMesh) parts.push({ geometry: o.geometry, material: o.material, matrix: new THREE.Matrix4().multiplyMatrices(inv, o.matrixWorld) });
  });
  return parts;
}
