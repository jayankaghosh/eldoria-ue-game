"""Logs how high the head and pelvis are through each death / knockdown clip, to see which ones end flat."""

import unreal

M = "/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/"
B = "/Game/ParagonMinions/Characters/Buff/Buff_Red/Animations/"
CLIPS = [M + n for n in ("Death_A", "Death_B", "Death_C", "Death_D", "Death_E", "KnockUp", "KnockUp_A")] + \
        [B + n for n in ("Death_Fwd", "Death_Back", "Knock_Bwd", "Knock_Fwd", "Spawn")] + \
        ["/Game/ParagonSevarog/Characters/Heroes/Sevarog/Animations/Death_front"]

opts = unreal.AnimPoseEvaluationOptions()
for path in CLIPS:
    anim = unreal.load_asset(path)
    if not anim:
        unreal.log(f"ELDORIA_INFO missing {path}")
        continue
    length = anim.get_play_length()
    rows = []
    for i in range(9):
        t = length * i / 8.0
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, opts)
        head = unreal.AnimPoseExtensions.get_bone_pose(pose, "head", unreal.AnimPoseSpaces.WORLD).translation
        pelvis = unreal.AnimPoseExtensions.get_bone_pose(pose, "pelvis", unreal.AnimPoseSpaces.WORLD).translation
        rows.append(f"{t:4.2f}:h{head.z:4.0f}/p{pelvis.z:4.0f}")
    unreal.log(f"ELDORIA_INFO {path.split('/')[-1]:12s} len={length:4.2f} " + " ".join(rows))
