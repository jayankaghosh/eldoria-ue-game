"""Retargets Game Animation Sample locomotion (UEFN mannequin) onto the Greystone hero.

Builds IK_Greystone from the sample's Paragon rig (IK_TwinBlast_Retarget, same bone naming), a
UEFN -> Greystone retargeter from RTG_UEFN_to_TwinBlast, then batch-retargets the walk / run / idle
loops copied into Content/Characters/UEFN_Mannequin/Animations into /Game/Eldoria/Animations/Hero."""

import unreal

OUT = "/Game/Eldoria/Animations/Hero"
RIGS = "/Game/Eldoria/Retarget"
eal = unreal.EditorAssetLibrary
GREYSTONE = unreal.load_asset("/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone")
UEFN = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")


def fresh_copy(src, dst):
    # re-runs reuse (and re-configure) the copy made the first time; it is referenced, so can't be deleted
    if eal.does_asset_exist(dst):
        return unreal.load_asset(dst)
    return eal.duplicate_asset(src, dst)


ik = fresh_copy("/Game/Characters/Paragon/Heroes/TwinBlast/Rigs/IK_TwinBlast_Retarget", f"{RIGS}/IK_Greystone")
rig = unreal.IKRigController.get_controller(ik)
rig.set_skeletal_mesh(GREYSTONE)
for chain in rig.get_retarget_chains():
    unreal.log(f"ELDORIA_INFO chain {chain.get_editor_property('chain_name')}")
eal.save_loaded_asset(ik)

rtg = fresh_copy("/Game/Characters/Paragon/Heroes/TwinBlast/Rigs/RTG_UEFN_to_TwinBlast", f"{RIGS}/RTG_UEFN_to_Greystone")
ctl = unreal.IKRetargeterController.get_controller(rtg)
ctl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik)
ctl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik)
ctl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, GREYSTONE)
ctl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, UEFN)
# The sample's TwinBlast setup pins the feet to the mannequin's own foot positions ("Blend to Source")
# and plants them with mannequin-specific speed curves. At Greystone's size the body then travels
# further than the feet each step and the legs trail behind. Let his legs follow his own stride.
for i in range(ctl.get_num_retarget_ops()):
    if str(ctl.get_op_name(i)) in ("Blend to Source", "Speed Plant IK Goals"):
        ctl.set_retarget_op_enabled(i, False)
        unreal.log(f"ELDORIA_INFO disabled op {ctl.get_op_name(i)}")
eal.save_loaded_asset(rtg)

reg = unreal.AssetRegistryHelpers.get_asset_registry()
assets = []
for folder in ("Idle", "Walk", "Run"):
    assets += [a for a in reg.get_assets_by_path(f"/Game/Characters/UEFN_Mannequin/Animations/{folder}") if str(a.asset_class_path.asset_name) == "AnimSequence"]
unreal.log(f"ELDORIA_INFO retargeting {len(assets)} clips")

inputs = unreal.IKRetargetBatchOperationInputs()
inputs.set_editor_property("assets_to_retarget", assets)
inputs.set_editor_property("source_mesh", UEFN)
inputs.set_editor_property("target_mesh", GREYSTONE)
inputs.set_editor_property("ik_retarget_asset", rtg)
inputs.set_editor_property("search", "M_Neutral_")
inputs.set_editor_property("replace", "Hero_")
inputs.set_editor_property("target_path", OUT)
inputs.set_editor_property("use_source_path", False)
inputs.set_editor_property("include_referenced_assets", False)
inputs.set_editor_property("overwrite_existing_files", True)
for a in unreal.IKRetargetBatchOperation.run_batch_retarget(inputs):
    eal.save_asset(str(a.package_name))
    seq = unreal.load_asset(str(a.package_name))
    unreal.log(f"ELDORIA_INFO made {a.package_name} len={seq.get_play_length():.2f}")
