"""Exports the sword pack's recordings to WAV so their timing can be analysed."""

import unreal

OUT = "/private/tmp/claude-502/-Users-joy--claude-spares-spare-ksORJK/c64473a9-812a-49d4-860f-2fd646f99455/scratchpad/swordwav"
for path in unreal.EditorAssetLibrary.list_assets("/Game/RealisticSwordSoundEffects/Wavs", recursive=True):
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.SoundWave):
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = f"{OUT}/{asset.get_name()}.wav"
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log(f"ELDORIA_INFO exported {asset.get_name()} {ok}")
