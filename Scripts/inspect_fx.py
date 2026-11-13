"""Lists material parameters on the enemy meshes and impact/death effects in the Paragon packs."""

import unreal

meshes = [
    "/Game/ParagonMinions/Characters/Minions/Dusk_Minions/Meshes/Minion_Lane_Melee_Dusk",
    "/Game/ParagonMinions/Characters/Buff/Buff_Red/Meshes/Buff_Red",
    "/Game/ParagonSevarog/Characters/Heroes/Sevarog/Meshes/Sevarog",
]
for path in meshes:
    m = unreal.load_asset(path)
    for i, slot in enumerate(m.get_editor_property("materials")):
        mat = slot.get_editor_property("material_interface")
        if not mat:
            continue
        scalars = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
        vectors = unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
        unreal.log(f"ELDORIA_INFO {path.split('/')[-1]}[{i}] {mat.get_name()} scalars={list(map(str, scalars))[:40]} vectors={list(map(str, vectors))[:15]}")

for root in ["/Game/ParagonGreystone/FX", "/Game/ParagonMinions/FX", "/Game/ParagonMinions/Characters/Global/FX", "/Game/ParagonSevarog/FX"]:
    names = [a.split("/")[-1].split(".")[0] for a in unreal.EditorAssetLibrary.list_assets(root, recursive=True)]
    hits = [n for n in names if any(k in n.lower() for k in ("impact", "hit", "death", "dissolve", "blood", "spark", "melee", "slash", "trail"))]
    unreal.log(f"ELDORIA_INFO FX {root}: {sorted(set(hits))[:60]}")
