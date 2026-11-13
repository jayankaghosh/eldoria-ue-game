"""Reports durations of the village footstep recordings and finds chest/coin/potion-like meshes."""

import unreal

for name in sorted(unreal.EditorAssetLibrary.list_assets("/Game/Audio/Footsteps")):
    a = unreal.load_asset(name)
    if isinstance(a, unreal.SoundWave):
        unreal.log(f"ELDORIA_INFO {a.get_name()} {a.get_editor_property('duration'):.2f}s")
for name in ["/Game/Audio/_Content/emt_cloths_flap_01", "/Game/Audio/_Content/emt_cloths_flap_02"]:
    a = unreal.load_asset(name)
    unreal.log(f"ELDORIA_INFO {a.get_name()} {a.get_editor_property('duration'):.2f}s")

reg = unreal.AssetRegistryHelpers.get_asset_registry()
for kw in ["coin", "gold", "chest", "potion", "bottle", "flask", "jug", "carafe", "treasure"]:
    hits = []
    for d in reg.get_assets_by_path("/Game", recursive=True):
        n = str(d.asset_name)
        if kw in n.lower() and str(d.asset_class_path.asset_name) in ("StaticMesh", "MaterialInstanceConstant", "Material"):
            hits.append(f"{d.package_name}({d.asset_class_path.asset_name})")
    unreal.log(f"ELDORIA_INFO search {kw}: {hits[:12]}")
