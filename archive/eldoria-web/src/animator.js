import * as THREE from 'three';

// Thin wrapper over AnimationMixer: cross-fades loops and runs one-shot clips with callbacks.
export class Animator {
  constructor(root, clips) {
    this.mixer = new THREE.AnimationMixer(root);
    this.clips = new Map(clips.map((c) => [c.name, c]));
    this.actions = new Map();
    this.current = null;
    this.currentName = '';
    this.onDone = null;
    this.mixer.addEventListener('finished', (e) => {
      if (e.action === this.current && this.onDone) {
        const cb = this.onDone;
        this.onDone = null;
        cb();
      }
    });
  }

  action(name) {
    let a = this.actions.get(name);
    if (!a) {
      const clip = this.clips.get(name);
      if (!clip) throw new Error(`Missing animation ${name}`);
      a = this.mixer.clipAction(clip);
      this.actions.set(name, a);
    }
    return a;
  }

  duration(name) {
    return this.clips.get(name)?.duration ?? 1;
  }

  play(name, { fade = 0.2, speed = 1, once = false, onDone = null, restart = false } = {}) {
    const next = this.action(name);
    if (this.current === next && !restart) {
      next.timeScale = speed;
      return next;
    }
    next.reset();
    next.timeScale = speed;
    next.setLoop(once ? THREE.LoopOnce : THREE.LoopRepeat, Infinity);
    next.clampWhenFinished = once;
    next.enabled = true;
    next.setEffectiveWeight(1);
    if (this.current && this.current !== next) next.crossFadeFrom(this.current, fade, false);
    next.play();
    this.current = next;
    this.currentName = name;
    this.onDone = onDone;
    return next;
  }

  // 0..1 progress of the current clip
  progress() {
    if (!this.current) return 0;
    return this.current.time / this.current.getClip().duration;
  }

  update(dt) {
    this.mixer.update(dt);
  }
}
