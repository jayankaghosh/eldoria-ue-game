"""Durations of the coin, whoosh and heartbeat recordings, to pick suitable ones."""

import unreal

for root in ["/Game/InterfaceAndItemSounds/WAV", "/Game/WhooshSFXPackLite/Wavs", "/Game/HealthDamageSFX/Wavs/Heartbeat"]:
    for path in sorted(unreal.EditorAssetLibrary.list_assets(root, recursive=True)):
        name = path.split(".")[-1]
        if root.endswith("WAV") and "Coins" not in name and "Item_Sell" not in name:
            continue
        a = unreal.load_asset(path)
        if isinstance(a, unreal.SoundWave):
            unreal.log(f"ELDORIA_INFO {name} {a.get_editor_property('duration'):.2f}s")
