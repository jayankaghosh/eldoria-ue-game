"""Imports Audio/Source/*.wav into /Game/Audio (run after Scripts/make_sounds.py),
and Audio/Recorded/<Folder>/*.wav into /Game/Audio/Recorded/<Folder>."""

import os
import unreal

SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Audio", "Source")
DEST = "/Game/Audio"

tasks = []
for name in sorted(os.listdir(SOURCE)):
    if not name.lower().endswith(".wav"):
        continue
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, name)
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)

RECORDED = os.path.join(os.path.dirname(SOURCE), "Recorded")
for folder in sorted(os.listdir(RECORDED)) if os.path.isdir(RECORDED) else []:
    for name in sorted(os.listdir(os.path.join(RECORDED, folder))):
        if not name.lower().endswith(".wav"):
            continue
        task = unreal.AssetImportTask()
        task.filename = os.path.join(RECORDED, folder, name)
        task.destination_path = f"{DEST}/Recorded/{folder}"
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# the ambience bed loops for the whole game
ambience = unreal.load_asset(f"{DEST}/AMB_Countryside")
if ambience:
    ambience.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_loaded_asset(ambience)

for asset in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True):
    if "/Footsteps/" in asset and "/Recorded/" not in asset:
        continue
    unreal.log(f"ELDORIA_INFO imported {asset}")
