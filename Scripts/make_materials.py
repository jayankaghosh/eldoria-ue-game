"""Creates Eldoria's own materials: polished gold for coins, and blood (screen overlay and ground decals)
built from the Megascans blood spatter / stain atlases."""

import unreal

PATH = "/Game/Eldoria/Materials"
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def constant(mat, value, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    e.set_editor_property("r", value)
    return e


def color(mat, rgb, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    e.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def gold():
    name = "M_Gold"
    if unreal.EditorAssetLibrary.does_asset_exist(f"{PATH}/{name}"):
        unreal.EditorAssetLibrary.delete_asset(f"{PATH}/{name}")
    mat = tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    mel.connect_material_property(color(mat, (1.0, 0.71, 0.29), -400, -100), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(constant(mat, 1.0, -400, 50), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(constant(mat, 0.22, -400, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # a faint glow so coins read from a distance
    mel.connect_material_property(color(mat, (0.30, 0.18, 0.04), -400, 250), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"ELDORIA_INFO created {mat.get_path_name()}")


def texture_sample(mat, path, x, y, sampler=None):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
    e.set_editor_property("texture", unreal.load_asset(path))
    if sampler is not None:
        e.set_editor_property("sampler_type", sampler)
    return e


def scalar_param(mat, name, value, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def multiply(mat, a, a_out, b, b_out, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, a_out, e, "A")
    mel.connect_material_expressions(b, b_out, e, "B")
    return e


def fresh(name):
    if unreal.EditorAssetLibrary.does_asset_exist(f"{PATH}/{name}"):
        unreal.EditorAssetLibrary.delete_asset(f"{PATH}/{name}")
    return tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())


SPATTER = "/Game/Megascans/Atlases/HighVelocityBloodSpatter/T_HighVelocityBloodSpatter_"
STAIN = "/Game/Megascans/Atlases/BloodStain/T_BloodStain_"


def blood_screen():
    """Splatter drawn over the screen edges when the hero is hurt. 'Amount' fades it."""
    mat = fresh("M_BloodScreen")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    albedo = texture_sample(mat, SPATTER + "A", -800, -100)
    mask = texture_sample(mat, SPATTER + "O", -800, 200, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    # darker and deeper red than the scan, as blood reads on a lens
    tint = color(mat, (1.5, 0.16, 0.12), -800, -300)
    col = multiply(mat, albedo, "RGB", tint, "", -500, -150)
    amount = scalar_param(mat, "Amount", 1.0, -800, 400)
    opacity = multiply(mat, mask, "R", amount, "", -500, 250)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"ELDORIA_INFO created {mat.get_path_name()}")


def blood_decal(name, atlas):
    """Wet blood projected onto the ground. 'Fade' takes it away over time."""
    mat = fresh(name)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    albedo = texture_sample(mat, atlas + "A", -800, -200)
    mask = texture_sample(mat, atlas + "O", -800, 100, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    normal = texture_sample(mat, atlas + "N", -800, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tint = color(mat, (0.6, 0.12, 0.1), -800, -400)
    col = multiply(mat, albedo, "RGB", tint, "", -500, -250)
    fade = scalar_param(mat, "Fade", 1.0, -800, 650)
    opacity = multiply(mat, mask, "R", fade, "", -500, 150)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    # fresh blood is wet and glossy
    mel.connect_material_property(constant(mat, 0.12, -500, 450), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"ELDORIA_INFO created {mat.get_path_name()}")


import sys
if "--blood-only" not in sys.argv:
    gold()
blood_screen()
blood_decal("M_BloodSpatterDecal", SPATTER)
blood_decal("M_BloodPoolDecal", STAIN)
