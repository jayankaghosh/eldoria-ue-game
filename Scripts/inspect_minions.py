"""Reports skeletons and clip lengths for the Paragon Minions characters."""

import unreal

M = "/Game/ParagonMinions/Characters"
PATHS = [
    f"{M}/Minions/Dusk_Minions/Meshes/Minion_Lane_Melee_Dusk",
    f"{M}/Minions/Dusk_Minions/Meshes/Minion_Lane_Super_Dusk",
    f"{M}/Minions/Dusk_Minions/Meshes/Minion_Lane_Siege_Dusk",
    f"{M}/Minions/Down_Minions/Meshes/Minion_Lane_Melee_Dawn",
    f"{M}/Minions/Down_Minions/Meshes/Minion_Lane_Super_Dawn",
    f"{M}/Minions/Down_Minions/Animations/Melee/Attack_A",
    f"{M}/Minions/Down_Minions/Animations/Melee/Attack_B",
    f"{M}/Minions/Down_Minions/Animations/Melee/Attack_C",
    f"{M}/Minions/Down_Minions/Animations/Melee/Combat_JogFwd",
    f"{M}/Minions/Down_Minions/Animations/Melee/NonCombat_Idle",
    f"{M}/Minions/Down_Minions/Animations/Melee/HitReact_Front",
    f"{M}/Minions/Down_Minions/Animations/Melee/Death_A",
    f"{M}/Buff/Buff_Black/Meshes/Buff_Black",
    f"{M}/Buff/Buff_Red/Meshes/Buff_Red",
    f"{M}/Buff/Buff_Black/Animations/BiteAttack_A",
    f"{M}/Buff/Buff_Black/Animations/Run_FWD",
    f"{M}/Buff/Buff_Black/Animations/Idle",
    f"{M}/Buff/Buff_Black/Animations/HitReaction_FWD",
]

for p in PATHS:
    a = unreal.load_asset(p)
    if a is None:
        unreal.log(f"ELDORIA_INFO {p} MISSING")
        continue
    info = [type(a).__name__]
    if isinstance(a, unreal.SkeletalMesh):
        b = a.get_bounds()
        info.append(f"skel={a.skeleton.get_name()} z={b.origin.z:.0f}+-{b.box_extent.z:.0f} xy={b.box_extent.x:.0f},{b.box_extent.y:.0f}")
    else:
        info.append(f"skel={a.get_editor_property('skeleton').get_name()} len={a.get_play_length():.2f}")
    unreal.log(f"ELDORIA_INFO {p.split('/')[-1]} " + " ".join(info))

for p in [f"{M}/Buff/Buff_Red", f"{M}/Buff/Buff_Black/Animations"]:
    names = sorted(x.split('/')[-1].split('.')[0] for x in unreal.EditorAssetLibrary.list_assets(p, recursive=True))
    unreal.log(f"ELDORIA_INFO LIST {p}: " + ", ".join(names))
