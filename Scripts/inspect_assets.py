"""Prints the facts needed to hook Paragon characters into Eldoria: skeletons, montage slots, clip lengths, mesh bounds."""

import unreal

G = "/Game/ParagonGreystone/Characters/Heroes/Greystone"
S = "/Game/ParagonSevarog/Characters/Heroes/Sevarog"

ASSETS = [
    f"{G}/Meshes/Greystone",
    f"{G}/Skins/Dragonlord/Meshes/Greystone_Dragonlord",
    f"{G}/Skins/Novaborn/Meshes/Greystone_Novaborn",
    f"{G}/Skins/Tough/Meshes/Greystone_Tough",
    f"{G}/Skins/WhiteTiger/Meshes/Greystone_WhiteTiger",
    f"{G}/Greystone_AnimBlueprint",
    f"{G}/Animations/Attack_PrimaryA_Montage",
    f"{G}/Animations/Attack_PrimaryB_Montage",
    f"{G}/Animations/Attack_PrimaryC_Montage",
    f"{G}/Animations/HitReact_Front",
    f"{G}/Animations/Death",
    f"{S}/Meshes/Sevarog",
    f"{S}/Sevarog_AnimBlueprint",
    f"{S}/Animations/Swing1_Medium_Montage",
    f"{S}/Animations/Swing2_Medium_Montage",
    f"{S}/Animations/Hitreact_Front",
    f"{S}/Animations/Death_front",
]


def report(path):
    asset = unreal.load_asset(path)
    if asset is None:
        unreal.log(f"ELDORIA_INFO {path} MISSING")
        return
    kind = type(asset).__name__
    info = [kind]
    if isinstance(asset, unreal.SkeletalMesh):
        b = asset.get_bounds()
        info.append(f"skeleton={asset.skeleton.get_path_name()}")
        info.append(f"bounds_origin={b.origin} extent={b.box_extent}")
    if isinstance(asset, unreal.AnimMontage):
        tracks = asset.get_editor_property("slot_anim_tracks")
        info.append("slots=" + ",".join(str(t.slot_name) for t in tracks))
        info.append(f"length={asset.get_play_length():.2f}")
    elif isinstance(asset, unreal.AnimSequenceBase):
        info.append(f"length={asset.get_play_length():.2f}")
    if isinstance(asset, unreal.AnimBlueprint):
        info.append(f"target_skeleton={asset.target_skeleton.get_path_name() if asset.target_skeleton else None}")
        info.append(f"class={asset.generated_class().get_path_name()}")
    unreal.log(f"ELDORIA_INFO {path} " + " | ".join(info))


for p in ASSETS:
    try:
        report(p)
    except Exception as exc:  # noqa: BLE001
        unreal.log(f"ELDORIA_INFO {p} ERROR {exc}")
