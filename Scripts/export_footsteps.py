"""Exports the Medieval Village footstep recordings to Audio/Recorded/Footsteps so they can be
re-imported as Eldoria's own sounds (the originals route through the village's Foley mix, which is silent here)."""

import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Audio", "Recorded", "Footsteps")
for path in unreal.EditorAssetLibrary.list_assets("/Game/Audio/Footsteps", recursive=False):
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.SoundWave):
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT, f"{asset.get_name()}.wav")
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    unreal.log(f"ELDORIA_INFO exported {asset.get_name()} {unreal.Exporter.run_asset_export_task(task)}")
