"""Generates ragdoll physics assets for enemy meshes that ship without a usable one (Brute, Bone King).
The meshes themselves are left untouched; the game assigns these at death (RagdollPhysics in DefaultGame.ini)."""

import unreal

MESHES = [
    "/Game/ParagonMinions/Characters/Buff/Buff_Red/Meshes/Buff_Red",
    "/Game/ParagonSevarog/Characters/Heroes/Sevarog/Meshes/Sevarog",
]
for path in MESHES:
    mesh = unreal.load_asset(path)
    existing = path + "_PhysicsAsset"
    if unreal.EditorAssetLibrary.does_asset_exist(existing):
        unreal.log(f"ELDORIA_INFO {path} -> {existing} (already made)")
        continue
    pa = unreal.SkeletalMeshEditorSubsystem.create_physics_asset(mesh, False, 0)
    if not pa:
        unreal.log(f"ELDORIA_INFO failed {path}")
        continue
    unreal.EditorAssetLibrary.save_loaded_asset(pa)
    unreal.log(f"ELDORIA_INFO {path} -> {pa.get_path_name()}")
