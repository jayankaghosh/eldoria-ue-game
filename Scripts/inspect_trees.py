"""Lists foliage meshes in the Eldoria map with instance counts, collision state and simple-collision shapes."""

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/Eldoria")
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
stats = {}
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        mesh = comp.static_mesh
        if not mesh:
            continue
        key = mesh.get_path_name()
        e = stats.setdefault(key, [0, set(), 0, mesh])
        e[0] += comp.get_instance_count()
        e[1].add(str(comp.get_collision_enabled()).split(".")[-1].rstrip(">").split(":")[0])
for key, (count, coll, _, mesh) in sorted(stats.items(), key=lambda kv: -kv[1][0]):
    try:
        simple = sm_sub.get_simple_collision_count(mesh)
    except Exception:  # noqa: BLE001
        simple = "?"
    b = mesh.get_bounds().box_extent
    unreal.log(f"ELDORIA_INFO {key.split('/')[-1]} n={count} collision={sorted(coll)} simple={simple} size=({b.x*2:.0f},{b.y*2:.0f},{b.z*2:.0f})")
