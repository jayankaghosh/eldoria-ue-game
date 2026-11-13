"""Builds the Eldoria map on top of the Megascans Medieval Village.

The village (terrain, buildings, foliage, lighting, atmosphere, audio) streams in from its own sublevels;
this script adds Eldoria's quest content to the persistent level: the warband camp in the open fields
outside the village, a patrol in its streets, the old ruins and the sky-shrine climb in the eastern
highlands, and Mordrak in the village square. Fights stay out of the forest: every enemy stands on open
ground (the game also clears undergrowth around them at load, see AEldoriaGameMode::ThinFoliage).

Run headless:
  UnrealEditor-Cmd Eldoria.uproject -run=pythonscript -script=Scripts/build_village_level.py
"""

import math
import random
import unreal

SOURCE_MAP = "/Game/Maps/MedievalVillage_P"
MAP_PATH = "/Game/Maps/Eldoria"
# the village's own first-person player, benchmark cameras, cinematics and commentary are not used
SKIP_SUBLEVELS = {"DeveloperCommentary", "Cinematics", "Gameplay", "Performance"}
MS = "/Game/Megascans/3D_Assets"

random.seed(11)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

ENEMY = unreal.load_class(None, "/Script/Eldoria.EnemyCharacter")
CHEST = unreal.load_class(None, "/Script/Eldoria.Chest")
COIN = unreal.load_class(None, "/Script/Eldoria.CoinPickup")
POTION = unreal.load_class(None, "/Script/Eldoria.HealthPickup")
GAME_MODE = unreal.load_class(None, "/Script/Eldoria.EldoriaGameMode")
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
FIRE = unreal.load_asset("/Game/Effects/P_Fire")
LANTERN = unreal.load_class(None, "/Game/Blueprints/BP_Lantern.BP_Lantern_C")

world = None


def log(msg):
    unreal.log(f"ELDORIA_INFO {msg}")


def prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:  # noqa: BLE001
        unreal.log_warning(f"Skipped {name}: {exc}")


# ---------- terrain queries ----------

def trace(x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 40000), unreal.Vector(x, y, -40000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    actor = t[9]
    return t[4].z, (actor.get_class().get_name() if actor else ""), t[7].z


def ground(x, y):
    h = trace(x, y)
    return h[0] if h else 0.0


def is_open(x, y, clear):
    h = trace(x, y)
    if not h or "Landscape" not in h[1] or h[2] < 0.93:
        return False
    for k in range(8):
        a = k * math.pi / 4
        o = trace(x + math.cos(a) * clear, y + math.sin(a) * clear)
        if not o or "Landscape" not in o[1] or abs(o[0] - h[0]) > 160:
            return False
    return True


def open_spot(x, y, clear=700, search=6000):
    """Nearest flat, unobstructed landscape point to (x, y)."""
    step = 400
    for r in range(0, search, step):
        n = max(1, int(2 * math.pi * r / step))
        for k in range(n):
            a = 2 * math.pi * k / n
            px, py = x + math.cos(a) * r, y + math.sin(a) * r
            if is_open(px, py, clear):
                return px, py, ground(px, py)
    log(f"no open spot near ({x},{y}); using it anyway")
    return x, y, ground(x, y)


# ---------- open ground: away from the forest ----------

TREE_CELL = 2000.0
tree_cells = {}
# fight sites already taken, so posts don't pile onto the same patch of open ground
taken = []


def map_trees():
    """Buckets every tall foliage tree into 20 m cells, to tell forest from field."""
    for a in actor_sub.get_all_level_actors():
        if a.get_class().get_name() != "InstancedFoliageActor":
            continue
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            if not m or m.get_bounds().box_extent.z * 2 < 500:
                continue
            for i in range(c.get_instance_count()):
                p = c.get_instance_transform(i, True).translation
                key = (int(p.x // TREE_CELL), int(p.y // TREE_CELL))
                tree_cells[key] = tree_cells.get(key, 0) + 1
    log(f"mapped {sum(tree_cells.values())} trees")


def trees_near(x, y):
    cx, cy = int(x // TREE_CELL), int(y // TREE_CELL)
    return sum(tree_cells.get((cx + i, cy + j), 0) for i in (-1, 0, 1) for j in (-1, 0, 1))


def field_spot(x, y, clear=700, search=4000, max_trees=None):
    """The most open flat landscape point within `search` of (x, y): fewest trees around, nearest first."""
    best = None
    step = 500
    for r in range(0, search + 1, step):
        n = max(1, int(2 * math.pi * r / step))
        for k in range(n):
            a = 2 * math.pi * k / n
            px, py = x + math.cos(a) * r, y + math.sin(a) * r
            t = trees_near(px, py)
            if best is not None and t >= best[0]:
                continue
            if any(math.hypot(px - qx, py - qy) < 2000 for qx, qy in taken):
                continue
            if is_open(px, py, clear):
                best = (t, px, py)
    if best is None or (max_trees is not None and best[0] > max_trees):
        return None
    log(f"field spot near ({x:.0f},{y:.0f}): ({best[1]:.0f},{best[2]:.0f}), {best[0]} trees around")
    return best[1], best[2], ground(best[1], best[2])


def village_centre():
    houses = [a.get_actor_location() for a in actor_sub.get_all_level_actors()
              if a.get_class().get_name() == "StaticMeshActor" and "House" in a.get_actor_label() and "Fence" not in a.get_actor_label()]
    vx = sum(p.x for p in houses) / len(houses)
    vy = sum(p.y for p in houses) / len(houses)
    radius = max(math.hypot(p.x - vx, p.y - vy) for p in houses)
    log(f"village centre ({vx:.0f},{vy:.0f}), radius {radius:.0f}, {len(houses)} house pieces")
    return vx, vy, radius


# ---------- spawning ----------

def spawn(cls, x, y, z, yaw=0.0, label=None):
    a = actor_sub.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    if label:
        a.set_actor_label(label)
    return a


def mesh(path, x, y, z, yaw=0.0, size=None, scale=1.0, pitch=0.0, roll=0.0, label=None):
    """Places a Megascans mesh. `size` scales it so its largest horizontal side is that many cm."""
    sm = unreal.load_asset(path)
    if not sm:
        log(f"missing mesh {path}")
        return None
    a = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(roll, pitch, yaw))
    a.static_mesh_component.set_static_mesh(sm)
    if size:
        e = sm.get_bounds().box_extent
        scale = size / max(1.0, 2 * max(e.x, e.y))
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    if label:
        a.set_actor_label(label)
    return a


def ms(name, idx=""):
    return f"{MS}/{name}/SM_{name}{idx}"


def collider(x, y, top, hx, hy, label):
    """Invisible walkable slab so platforms never depend on the visual mesh's collision."""
    a = spawn(unreal.StaticMeshActor, x, y, top - 25, label=label)
    a.static_mesh_component.set_static_mesh(CUBE)
    a.set_actor_scale3d(unreal.Vector(hx / 50.0, hy / 50.0, 0.5))
    a.set_actor_hidden_in_game(True)
    return a


def face(x, y, tx, ty):
    """Yaw that makes something at (x, y) look toward (tx, ty)."""
    return math.degrees(math.atan2(ty - y, tx - x))


def enemy(x, y, archetype, group, boss=False, dormant=True, yaw=0.0):
    z = ground(x, y)
    e = spawn(ENEMY, x, y, z + 110, yaw, label=f"{group}_{archetype}")
    prop(e, "archetype", unreal.Name(archetype))
    prop(e, "group", unreal.Name(group))
    prop(e, "boss", boss)
    prop(e, "dormant", dormant)
    return e


def chest(x, y, z, coins, potion=False, key=False, yaw=0.0):
    c = spawn(CHEST, x, y, z + 35, yaw, label="Chest_RoyalKey" if key else "Chest")
    prop(c, "coins", coins)
    prop(c, "has_potion", potion)
    prop(c, "has_royal_key", key)
    return c


def marker(x, y, z, name):
    t = spawn(unreal.TargetPoint, x, y, z, label=name)
    prop(t, "tags", [unreal.Name(name)])


def fire(x, y, z, light=4000.0):
    if FIRE:
        em = spawn(unreal.Emitter, x, y, z + 20, label="Campfire")
        em.particle_system_component.set_template(FIRE)
    pl = spawn(unreal.PointLight, x, y, z + 120, label="FireLight")
    lc = pl.point_light_component
    prop(lc, "intensity", light)
    prop(lc, "light_color", unreal.Color(255, 150, 70, 255))
    prop(lc, "attenuation_radius", 1600.0)


def guard_posts(ax, ay, bx, by, pattern, name):
    """Evenly spaced guard posts between two points. `pattern` lists the enemies at each post;
    guards stand side by side across the path, facing where the hero comes from."""
    dx, dy = bx - ax, by - ay
    length = math.hypot(dx, dy)
    nx, ny = -dy / length, dx / length
    for i, guards in enumerate(pattern):
        t = (i + 1) / (len(pattern) + 1)
        # the most open ground near this point; the game clears a meadow around it at load
        px, py, _ = field_spot(ax + dx * t, ay + dy * t, clear=350, search=2500) or open_spot(ax + dx * t, ay + dy * t, clear=350, search=1500)
        taken.append((px, py))
        for j, kind in enumerate(guards):
            off = (j - (len(guards) - 1) / 2) * 260
            gx, gy = px + nx * off, py + ny * off
            enemy(gx, gy, kind, f"{name}{i + 1}", dormant=False, yaw=face(gx, gy, ax, ay))
        log(f"{name}{i + 1}: {guards} at ({px:.0f},{py:.0f})")


def coin_line(ax, ay, bx, by, spacing=900):
    d = math.hypot(bx - ax, by - ay)
    for i in range(1, int(d / spacing)):
        if i % 5 == 0:
            continue
        t = i * spacing / d
        x = ax + (bx - ax) * t + math.sin(i * 1.3) * 120
        y = ay + (by - ay) * t + math.cos(i * 1.1) * 120
        spawn(COIN, x, y, ground(x, y) + 100)


# ---------- build ----------

def build():
    global world
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
    unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, MAP_PATH)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

    # remember the village's spawn point before its Gameplay sublevel goes away
    start = [a for a in actor_sub.get_all_level_actors() if a.get_class().get_name() == "PlayerStart"][0]
    sx, sy, sz = start.get_actor_location().x, start.get_actor_location().y, start.get_actor_location().z
    start_yaw = start.get_actor_rotation().yaw

    # the persistent level is current after loading, so new actors land in it
    for lvl in unreal.EditorLevelUtils.get_levels(world):
        name = lvl.get_outer().get_name()
        if name in SKIP_SUBLEVELS:
            ok = unreal.EditorLevelUtils.remove_level_from_world(lvl)
            log(f"removed sublevel {name}: {ok}")
        else:
            log(f"keeping level {name}")
    prop(world.get_world_settings(), "default_game_mode", GAME_MODE)

    for a in actor_sub.get_all_level_actors():
        if a.get_class().get_name() == "TextRenderActor":
            actor_sub.destroy_actor(a)
    spawn(unreal.PlayerStart, sx, sy, sz + 20, start_yaw, label="PlayerStart")
    log(f"player start ({sx:.0f},{sy:.0f})")

    map_trees()
    vx, vy, vr = village_centre()

    # --- the warband camp in the open fields just outside the village, on the hero's side of it ---
    dx, dy = sx - vx, sy - vy
    d = math.hypot(dx, dy)
    ax, ay = vx + dx / d * (vr + 3500), vy + dy / d * (vr + 3500)
    cx, cy, cz = field_spot(ax, ay, clear=1100, search=4000) or open_spot(ax, ay, clear=1100)
    taken.append((cx, cy))
    log(f"west camp ({cx:.0f},{cy:.0f},{cz:.0f})")
    marker(cx, cy, cz + 100, "Objective_WestCamp")
    fire(cx, cy, cz)
    for k in range(6):
        a = k * math.pi / 3
        mesh(ms("MossyLog"), cx + math.cos(a) * 260, cy + math.sin(a) * 260, cz, yaw=math.degrees(a) + 90, size=230)
    mesh(ms("CoalA"), cx, cy, cz, size=120)
    mesh(ms("FallenPineTree"), cx - 900, cy + 500, cz - 20, yaw=30, size=1400)
    for dx, dy, name in [(550, -350, "WoodenBarrelA"), (620, -200, "WoodenBarrelB"), (-500, -600, "WoodenAmmoCrate"),
                         (-650, 420, "WoodenBox"), (480, 520, "Axe"), (-300, 700, "TreeLog"), (720, 380, "WoodenWheelbarrow")]:
        x, y = cx + dx, cy + dy
        mesh(ms(name), x, y, ground(x, y), yaw=random.uniform(0, 360))
    for k, kind in enumerate(["Grunt", "Grunt", "Grunt", "Brute"]):
        a = k * math.pi / 2 + math.pi / 4
        x, y = cx + math.cos(a) * 480, cy + math.sin(a) * 480
        enemy(x, y, kind, "WestCamp", yaw=face(x, y, cx, cy))
    chest(cx - 1100, cy - 150, ground(cx - 1100, cy - 150), 14, potion=True, yaw=40)

    # --- a patrol in the village streets ---
    for k in range(3):
        spot = field_spot(vx + math.cos(k * 2.1) * vr * 0.45, vy + math.sin(k * 2.1) * vr * 0.45, clear=250, search=2500)
        if spot:
            enemy(spot[0], spot[1], "Grunt", "VillagePatrol", dormant=False, yaw=random.uniform(0, 360))

    # --- the old ruins in the eastern highlands ---
    rx, ry, rz = field_spot(15500, -23000, clear=1200, search=4000) or open_spot(15500, -23000, clear=1200)
    taken.append((rx, ry))
    log(f"ruins ({rx:.0f},{ry:.0f},{rz:.0f})")
    marker(rx, ry, rz + 200, "Objective_Ruins")
    for k in range(6):
        a = k * math.pi / 3 + 0.3
        x, y = rx + math.cos(a) * 1300, ry + math.sin(a) * 1300
        piece = ms("CastleWall") if k % 2 == 0 else ms("MossyStoneWallA" if k % 3 else "MossyStoneWallB")
        mesh(piece, x, y, ground(x, y) - 30, yaw=math.degrees(a) + 90, size=900 if k % 2 == 0 else 700)
    mesh(ms("DamagedCastleStairs"), rx - 700, ry + 900, ground(rx - 700, ry + 900) - 10, yaw=200, size=600)
    for i, (dx, dy) in enumerate([(900, 1100), (-1200, -800), (300, -1400)]):
        x, y = rx + dx, ry + dy
        mesh(ms("GraniteRockAssembly") if i == 0 else ms("IcelandicMossyRock"), x, y, ground(x, y) - 50, yaw=random.uniform(0, 360), size=900)
    for dx, dy in [(-400, 500), (-250, 650), (-550, 700)]:
        x, y = rx + dx, ry + dy
        mesh(ms("GraveA", "_00"), x, y, ground(x, y), yaw=random.uniform(-15, 15) + 90, size=110)
    for k, kind in enumerate(["Brute", "Grunt", "Brute", "Grunt"]):
        a = k * math.pi / 2
        x, y = rx + math.cos(a) * 600, ry + math.sin(a) * 600
        enemy(x, y, kind, "Ruins", yaw=face(x, y, rx, ry))
    chest(rx + 1000, ry - 300, ground(rx + 1000, ry - 300), 14, potion=True, yaw=-60)

    # --- the sky shrine: floating stones spiralling up from the ruins ---
    center_x, center_y = rx - 2000, ry - 1600
    radius = 860.0
    angle = math.atan2(ry - center_y, rx - center_x)
    height = rz + 190.0
    for i in range(10):
        x = center_x + math.cos(angle) * radius
        y = center_y + math.sin(angle) * radius
        collider(x, y, height, 240, 240, f"SkyStone_{i}")
        mesh(ms("JapaneseShrineStoneFloorA"), x, y, height - 30, yaw=math.degrees(angle), size=500)
        mesh(ms("IcelandicMossyRock"), x, y, height - 60, yaw=random.uniform(0, 360), size=520, pitch=180)
        angle -= 0.84
        height += 205.0
    shx = center_x + math.cos(angle) * (radius + 650)
    shy = center_y + math.sin(angle) * (radius + 650)
    shz = height - 60
    collider(shx, shy, shz, 460, 460, "Shrine")
    for dx, dy in [(-230, -230), (230, -230), (-230, 230), (230, 230)]:
        mesh(ms("JapaneseShrineStoneFloorA"), shx + dx, shy + dy, shz - 30, size=470)
    mesh(ms("GraniteRockAssembly"), shx, shy, shz - 120, size=1000, pitch=180)
    if LANTERN:
        for dx, dy in [(-380, 0), (380, 0)]:
            spawn(LANTERN, shx + dx, shy + dy, shz, label="ShrineLantern")
    pl = spawn(unreal.PointLight, shx, shy, shz + 250, label="ShrineGlow")
    prop(pl.point_light_component, "intensity", 6000.0)
    prop(pl.point_light_component, "light_color", unreal.Color(255, 205, 120, 255))
    chest(shx, shy, shz, 25, key=True)
    marker(shx, shy, shz + 150, "Objective_Shrine")
    log(f"shrine ({shx:.0f},{shy:.0f},{shz:.0f})")

    # --- Mordrak in the village square ---
    bx, by, bz = open_spot(2500, -6000, clear=900, search=5000)
    taken.append((bx, by))
    log(f"boss ({bx:.0f},{by:.0f},{bz:.0f})")
    marker(bx, by, bz + 150, "Objective_Boss")
    enemy(bx, by, "Boss", "Castle", boss=True, yaw=math.degrees(math.atan2(sy - by, sx - bx)))
    enemy(bx - 600, by + 500, "Brute", "Castle", yaw=face(bx - 600, by + 500, sx, sy))
    enemy(bx + 600, by + 500, "Brute", "Castle", yaw=face(bx + 600, by + 500, sx, sy))
    fire(bx - 900, by - 400, bz, light=3000.0)
    fire(bx + 900, by - 400, bz, light=3000.0)

    # --- patrols, coins, potions and side chests ---
    # guard posts along the quest routes; the challenge grows as the hero progresses
    guard_posts(sx, sy, cx, cy, [["Grunt"], ["Grunt", "Grunt"], ["Grunt"]], "RoadA_Post")
    guard_posts(cx, cy, rx, ry, [["Grunt", "Grunt"], ["Brute"], ["Grunt", "Grunt"]], "RoadB_Post")
    guard_posts(rx, ry, bx, by, [["Brute", "Grunt"], ["Grunt", "Grunt"], ["Brute"]], "RoadC_Post")

    # coins come only from defeated enemies (the game also clears any placed ones at load)
    for x, y in [((sx + cx) / 2, (sy + cy) / 2 + 400), ((sx + rx) / 2 + 300, (sy + ry) / 2), (cx + 900, cy + 200), (rx - 900, ry + 200), (bx, by + 1400)]:
        spawn(POTION, x, y, ground(x, y) + 100)
    for x, y, coins in [(sx - 600, sy + 300, 8), ((cx + rx) / 2, (cy + ry) / 2, 12)]:
        px, py, pz = open_spot(x, y, clear=300, search=2500)
        chest(px, py, pz, coins, yaw=random.uniform(0, 360))

    # --- navigation where fights happen (elsewhere enemies walk straight at the hero) ---
    for name, (x, y, z) in {"Camp": (cx, cy, cz), "Ruins": (rx, ry, rz), "Square": (bx, by, bz)}.items():
        nav = spawn(unreal.NavMeshBoundsVolume, x, y, z, label=f"Nav_{name}")
        nav.set_actor_scale3d(unreal.Vector(40, 40, 15))

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
    log("Eldoria village map saved")


build()
