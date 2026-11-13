"""Lists the non-scenery actors in the Eldoria map's persistent and Gameplay levels."""

import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/Eldoria")
skip = ("StaticMeshActor", "DecalActor", "Emitter", "SphereReflectionCapture", "BlockingVolume", "GroupActor",
        "SpotLight", "PointLight", "InstancedFoliageActor", "BP_SplineMeshActor_C", "BP_FenceTool_C", "BP_Path_C",
        "ProceduralFoliageVolume", "BP_Lantern_C", "NiagaraActor", "AmbientSound", "BP_Crow_C", "BP_AudioActor_RandomAmb_C")
seen = {}
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    lvl = a.get_level().get_outer().get_name()
    cls = a.get_class().get_name()
    if cls in skip or lvl not in ("Eldoria", "Gameplay", "Performance", "Atmospherics", "PostProcess"):
        continue
    key = (lvl, cls)
    seen[key] = seen.get(key, 0) + 1
    extra = ""
    if isinstance(a, unreal.CameraActor):
        extra = f" auto_activate={a.get_editor_property('auto_activate_for_player')}"
    if isinstance(a, unreal.LevelSequenceActor):
        s = a.get_editor_property("playback_settings")
        extra = f" autoplay={s.get_editor_property('auto_play') if s else '?'} seq={a.get_sequence()}"
    if seen[key] <= 3 and (extra or cls.startswith("BP_") or "Camera" in cls or "Sequence" in cls or "Trigger" in cls):
        unreal.log(f"ELDORIA_INFO {lvl} {cls} {a.get_actor_label()}{extra}")
for (lvl, cls), n in sorted(seen.items()):
    unreal.log(f"ELDORIA_INFO count {lvl} {cls} {n}")
