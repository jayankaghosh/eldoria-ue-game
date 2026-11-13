"""Imports the story narration (Audio/Recorded/Narration, made by Scripts/make_narration.sh) into
/Game/Audio/Recorded/Narration. Narration plays as a UI sound so it is never attenuated by distance."""

import os
import unreal

SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Audio", "Recorded", "Narration")
DEST = "/Game/Audio/Recorded/Narration"
tasks = []
for name in sorted(os.listdir(SOURCE)):
    if name.lower().endswith(".wav"):
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SOURCE, name)
        t.destination_path = DEST
        t.automated = True
        t.replace_existing = True
        t.save = True
        tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        w = unreal.load_asset(p)
        unreal.log(f"ELDORIA_INFO {p} {w.get_editor_property('duration'):.2f}s")
