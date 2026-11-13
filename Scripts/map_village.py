"""Prints an overhead ASCII map of the village (ground vs buildings vs slopes) plus key bounds."""

import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/MedievalVillage_P")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

for a in actors:
    cls = a.get_class().get_name()
    if cls in ("NavMeshBoundsVolume", "PlayerStart", "Landscape"):
        o, e = a.get_actor_bounds(False)
        unreal.log(f"ELDORIA_INFO {cls} {a.get_actor_label()} center=({o.x:.0f},{o.y:.0f},{o.z:.0f}) extent=({e.x:.0f},{e.y:.0f},{e.z:.0f})")

# per sublevel centres
by_level = {}
for a in actors:
    lvl = a.get_level().get_outer().get_name()
    o, e = a.get_actor_bounds(False)
    if e.x > 50000:
        continue
    by_level.setdefault(lvl, []).append(o)
for lvl, pts in by_level.items():
    cx = sum(p.x for p in pts) / len(pts)
    cy = sum(p.y for p in pts) / len(pts)
    unreal.log(f"ELDORIA_INFO level {lvl} actors={len(pts)} centre=({cx:.0f},{cy:.0f})")

# overhead map: 1 cell = 10 m
CX, CY, HALF, STEP = 2000, -8000, 22000, 1000
rows = []
for gy in range(-HALF, HALF + 1, STEP):
    row = ""
    for gx in range(-HALF, HALF + 1, STEP):
        x, y = CX + gx, CY + gy
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        if not hit:
            row += " "
            continue
        info = hit.to_tuple()
        actor = info[9] if len(info) > 9 else None
        z = info[4].z if hasattr(info[4], "z") else 0
        name = actor.get_class().get_name() if actor else ""
        if "Landscape" in name:
            row += "." if z < 0 else ":" if z < 800 else "^"
        elif "Water" in name:
            row += "~"
        else:
            row += "#"
    rows.append(row)
unreal.log("ELDORIA_INFO MAP origin=(%d,%d) cell=10m, x→ right, y↓ down" % (CX - HALF, CY - HALF))
for r in rows:
    unreal.log("ELDORIA_INFO |" + r + "|")
