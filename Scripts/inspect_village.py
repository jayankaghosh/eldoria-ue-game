"""Summarises the Medieval Village map: sublevels, extents, notable actors."""

import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/MedievalVillage_P")
unreal.log(f"ELDORIA_INFO world {world.get_name()}")
for lvl in unreal.EditorLevelUtils.get_levels(world):
    unreal.log(f"ELDORIA_INFO level {lvl.get_outer().get_name()}")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
counts = {}
lo = [1e12] * 3
hi = [-1e12] * 3
for a in actors:
    cls = a.get_class().get_name()
    counts[cls] = counts.get(cls, 0) + 1
    if isinstance(a, (unreal.StaticMeshActor, unreal.Landscape, unreal.LandscapeProxy)):
        o, e = a.get_actor_bounds(False)
        if e.x < 200000:
            for i, k in enumerate("xyz"):
                lo[i] = min(lo[i], getattr(o, k) - getattr(e, k))
                hi[i] = max(hi[i], getattr(o, k) + getattr(e, k))
    if cls in ("PlayerStart", "Landscape", "LandscapeStreamingProxy", "WaterBodyRiver", "WaterBodyLake", "NavMeshBoundsVolume") or "Start" in cls:
        unreal.log(f"ELDORIA_INFO actor {cls} {a.get_actor_label()} at {a.get_actor_location()}")
for cls, n in sorted(counts.items(), key=lambda kv: -kv[1])[:30]:
    unreal.log(f"ELDORIA_INFO count {cls} {n}")
unreal.log(f"ELDORIA_INFO bounds min={lo} max={hi}")
ws = world.get_world_settings()
unreal.log(f"ELDORIA_INFO gamemode {ws.get_editor_property('default_game_mode')}")
