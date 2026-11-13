"""Builds the Eldoria map: lighting, world blockout and every gameplay actor.

Run inside the Unreal Editor (Tools > Execute Python Script) or headless:
  UnrealEditor-Cmd Eldoria.uproject -run=pythonscript -script=Scripts/build_level.py

Coordinates are in centimetres. North (towards the castle) is +X.
The blockout shapes are placeholders that get replaced with Fab / Megascans environment art.
"""

import math
import unreal

MAP_PATH = "/Game/Maps/Eldoria"

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

PLANE = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")

ENEMY = unreal.load_class(None, "/Script/Eldoria.EnemyCharacter")
CHEST = unreal.load_class(None, "/Script/Eldoria.Chest")
GATE = unreal.load_class(None, "/Script/Eldoria.CastleGate")
COIN = unreal.load_class(None, "/Script/Eldoria.CoinPickup")
POTION = unreal.load_class(None, "/Script/Eldoria.HealthPickup")


def prop(obj, name, value):
    """Sets an editor property, logging instead of failing if an engine version renamed it."""
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:  # noqa: BLE001 - keep building the level
        unreal.log_warning(f"Skipped {name} on {type(obj).__name__}: {exc}")


def spawn(cls, x, y, z=0.0, yaw=0.0, label=None):
    actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    if label:
        actor.set_actor_label(label)
    return actor


def block(mesh, x, y, z, sx, sy, sz, yaw=0.0, label=None):
    """A static mesh blockout piece; engine primitives are 100 cm across."""
    actor = spawn(unreal.StaticMeshActor, x, y, z, yaw, label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(sx, sy, sz))
    return actor


def tag(actor, name):
    prop(actor, "tags", [unreal.Name(name)])


def marker(x, y, z, name):
    t = spawn(unreal.TargetPoint, x, y, z, label=name)
    tag(t, name)
    return t


def enemy(x, y, z, archetype, group, boss=False, dormant=True, yaw=0.0):
    e = spawn(ENEMY, x, y, z + 100.0, yaw, label=f"{group}_{archetype}")
    prop(e, "archetype", unreal.Name(archetype))
    prop(e, "group", unreal.Name(group))
    prop(e, "boss", boss)
    prop(e, "dormant", dormant)
    return e


def chest(x, y, z, coins, potion=False, key=False, yaw=0.0):
    c = spawn(CHEST, x, y, z + 30.0, yaw, label="Chest_RoyalKey" if key else "Chest")
    prop(c, "coins", coins)
    prop(c, "has_potion", potion)
    prop(c, "has_royal_key", key)
    return c


def build():
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

    # ---------- sky and light: physically based sun, atmosphere, clouds, fog ----------
    sun = spawn(unreal.DirectionalLight, 0, 0, 2000, label="Sun")
    sun.set_actor_rotation(unreal.Rotator(0.0, -38.0, 35.0), False)
    light = sun.light_component
    prop(light, "intensity", 9.0)
    prop(light, "atmosphere_sun_light", True)
    prop(light, "light_color", unreal.Color(255, 244, 228, 255))

    spawn(unreal.SkyAtmosphere, 0, 0, 0, label="SkyAtmosphere")
    spawn(unreal.VolumetricCloud, 0, 0, 0, label="Clouds")
    sky = spawn(unreal.SkyLight, 0, 0, 500, label="SkyLight")
    prop(sky.light_component, "real_time_capture", True)
    prop(sky.light_component, "mobility", unreal.ComponentMobility.MOVABLE)
    fog = spawn(unreal.ExponentialHeightFog, 0, 0, -200, label="HeightFog")
    prop(fog.component, "fog_density", 0.012)
    prop(fog.component, "enable_volumetric_fog", True)

    ppv = spawn(unreal.PostProcessVolume, 0, 0, 0, label="PostProcess")
    prop(ppv, "unbound", True)
    settings = ppv.settings
    prop(settings, "override_auto_exposure_bias", True)
    prop(settings, "auto_exposure_bias", 0.5)
    prop(settings, "override_vignette_intensity", True)
    prop(settings, "vignette_intensity", 0.35)
    prop(settings, "override_bloom_intensity", True)
    prop(settings, "bloom_intensity", 0.4)
    prop(ppv, "settings", settings)

    # ---------- ground and landmarks (blockout) ----------
    block(PLANE, 0, 0, 0, 600, 600, 1, label="Ground")
    block(CUBE, 10400, 0, -200, 80, 90, 9, label="CastlePlateau")
    block(SPHERE, 2600, 8400, -1800, 40, 40, 24, label="RuinsHill")
    block(CUBE, 10400, 0, 700, 18, 20, 30, label="CastleKeep")

    # castle walls with a gate opening in the middle
    wall_x = 6800
    for y in (-3500, -2100, 2100, 3500):
        block(CUBE, wall_x, y, 750, 4, 14, 8, label="CastleWall")
    block(CUBE, wall_x, -900, 750, 4, 6, 8, label="GatePillar")
    block(CUBE, wall_x, 900, 750, 4, 6, 8, label="GatePillar")
    for x in (8200, 9700, 11200, 12700):
        block(CUBE, x, -4200, 750, 14, 4, 8, label="CastleWall")
        block(CUBE, x, 4200, 750, 14, 4, 8, label="CastleWall")
    gate = spawn(GATE, wall_x, 0, 700, label="CastleGate")

    # village houses and a few trees as stand-ins
    for i, (x, y) in enumerate([(-5200, -1500), (-6000, 1600), (-4400, 1500), (-7600, -1300), (-6600, -2600), (-3400, -2400)]):
        block(CUBE, x, y, 300, 6, 7, 6, yaw=i * 23.0, label="House")
    for i in range(40):
        a = i * 2.39996
        r = 9000 + (i % 7) * 900
        block(CYLINDER, math.cos(a) * r, math.sin(a) * r - 6000, 400, 0.8, 0.8, 8, label="Tree")

    # ---------- the sky shrine: a spiral of stepping stones above the ruins ----------
    center_x, center_y = 4600, 7200
    radius = 860.0
    angle = math.atan2(8400 - center_y, 2600 - center_x)
    height = 900.0
    for i in range(10):
        x = center_x + math.cos(angle) * radius
        y = center_y + math.sin(angle) * radius
        block(CUBE, x, y, height, 4.6, 4.6, 0.5, label=f"SkyStone_{i}")
        spawn(COIN, x, y, height + 120.0)
        angle -= 0.84
        height += 210.0
    shrine_x = center_x + math.cos(angle) * (radius + 650)
    shrine_y = center_y + math.sin(angle) * (radius + 650)
    block(CUBE, shrine_x, shrine_y, height, 9, 9, 0.6, label="Shrine")
    chest(shrine_x, shrine_y, height + 30.0, 25, key=True)
    marker(shrine_x, shrine_y, height + 100.0, "Objective_Shrine")

    # ---------- the hero ----------
    spawn(unreal.PlayerStart, -4000, 0, 120, yaw=0.0, label="PlayerStart")

    # ---------- the western warband ----------
    wx, wy = 400, -8600
    marker(wx, wy, 100, "Objective_WestCamp")
    for dx, dy, kind in [(-300, 400, "Grunt"), (400, -400, "Grunt"), (200, 500, "Grunt"), (-500, -400, "Grunt"), (700, 200, "Brute")]:
        enemy(wx + dx, wy + dy, 0, kind, "WestCamp")
    chest(wx - 1000, wy - 200, 0, 14, potion=True)

    # ---------- the old ruins ----------
    rx, ry = 2600, 8400
    marker(rx, ry, 500, "Objective_Ruins")
    for dx, dy, kind in [(-300, 400, "Brute"), (400, -300, "Brute"), (-600, -300, "Grunt"), (600, 500, "Grunt"), (0, -800, "Grunt"), (800, 100, "Grunt")]:
        enemy(rx + dx, ry + dy, 400, kind, "Ruins")
    chest(rx + 1200, ry - 400, 400, 14, potion=True)

    # ---------- the castle ----------
    marker(wall_x, 0, 200, "Objective_Gate")
    marker(8400, 0, 200, "Objective_Boss")
    enemy(8600, 0, 500, "Boss", "Castle", boss=True, yaw=180.0)
    enemy(8000, -900, 500, "Brute", "Castle", yaw=180.0)
    enemy(8000, 900, 500, "Brute", "Castle", yaw=180.0)

    # roaming patrols that are already awake
    for x, y in [(-1000, -4000), (2000, 4000), (0, 2000), (3000, -3000)]:
        enemy(x, y, 0, "Grunt", "Roamers", dormant=False)

    # coins along the roads and a few potions
    for i in range(1, 40):
        spawn(COIN, -4000 + i * 270, math.sin(i * 0.7) * 150, 80)
    for i in range(1, 22):
        spawn(COIN, -1200 + i * 75, -i * 360, 80)
        spawn(COIN, 600 + i * 95, i * 360, 80)
    for x, y in [(-2000, -2000), (1500, 2500), (300, -7400), (2600, 7000), (6000, -600), (6000, 600)]:
        spawn(POTION, x, y, 80)
    chest(-3600, 400, 0, 8)

    # navigation for the enemies
    nav = spawn(unreal.NavMeshBoundsVolume, 2000, 0, 0, label="NavBounds")
    nav.set_actor_scale3d(unreal.Vector(300, 300, 40))

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
    unreal.log("Eldoria map built and saved to " + MAP_PATH)


build()
