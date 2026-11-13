"""Reports what the Eldoria map holds: foliage per type (count, size), the village's buildings and where
each enemy group stands. Used to plan foliage thinning and enemy placement."""

import collections
import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/Eldoria")
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_sub.get_all_level_actors()

foliage = collections.Counter()
heights = {}
levels = collections.Counter()
for a in actors:
    if a.get_class().get_name() != "InstancedFoliageActor":
        continue
    lvl = a.get_outer().get_outer().get_name()
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.get_editor_property("static_mesh")
        n = c.get_instance_count()
        if not m or n == 0:
            continue
        name = m.get_name()
        foliage[name] += n
        levels[lvl] += n
        heights[name] = m.get_bounds().box_extent.z * 2
for name, n in foliage.most_common(60):
    unreal.log(f"ELDORIA_INFO foliage {name}: {n} instances, ~{heights[name]:.0f}cm tall")
unreal.log(f"ELDORIA_INFO foliage per level: {dict(levels)}")

houses = collections.Counter()
house_pos = []
for a in actors:
    if a.get_class().get_name() not in ("StaticMeshActor",):
        continue
    sm = a.static_mesh_component.get_editor_property("static_mesh")
    if not sm:
        continue
    n = sm.get_name()
    ext = sm.get_bounds().box_extent
    if ext.x * ext.y > 400 * 400 and ext.z > 300:
        houses[n] += 1
        house_pos.append(a.get_actor_location())
for n, c in houses.most_common(25):
    unreal.log(f"ELDORIA_INFO big mesh {n} x{c}")
if house_pos:
    xs = [p.x for p in house_pos]; ys = [p.y for p in house_pos]
    unreal.log(f"ELDORIA_INFO buildings span x {min(xs):.0f}..{max(xs):.0f}, y {min(ys):.0f}..{max(ys):.0f}, centre ({sum(xs)/len(xs):.0f},{sum(ys)/len(ys):.0f}) count={len(house_pos)}")
    # coarse clusters on a 4000cm grid
    grid = collections.Counter((round(p.x / 4000), round(p.y / 4000)) for p in house_pos)
    unreal.log("ELDORIA_INFO building cells: " + ", ".join(f"({x*4000},{y*4000}):{n}" for (x, y), n in grid.most_common(15)))

for a in actors:
    cn = a.get_class().get_name()
    if cn in ("EnemyCharacter", "PlayerStart", "Chest"):
        p = a.get_actor_location()
        g = a.get_editor_property("group") if cn == "EnemyCharacter" else ""
        unreal.log(f"ELDORIA_INFO {cn} {a.get_actor_label()} group={g} at ({p.x:.0f},{p.y:.0f},{p.z:.0f})")
