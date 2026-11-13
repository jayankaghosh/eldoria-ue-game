"""Shows how sounds are routed (class, submix, attenuation, modulation) to explain silent playback."""

import unreal

for path in ["/Game/Audio/Footsteps/SW_FS_Sndl_Walk_10", "/Game/Audio/Footsteps/SW_FS_Sndl_Acc_26",
             "/Game/InterfaceAndItemSounds/WAV/Coins_09_wav", "/Game/Audio/SFX_FleshHit_1",
             "/Game/WhooshSFXPackLite/Wavs/Whooshes_SFX_Pack_Lite/Extra_Fast/Standard/MOVEActv_ExtraFast_HA_WHOOSH_01_Wav"]:
    w = unreal.load_asset(path)
    parts = []
    for prop in ("sound_class_object", "sound_submix_object", "attenuation_settings", "volume", "modulation_settings"):
        try:
            v = w.get_editor_property(prop)
            parts.append(f"{prop}={v.get_name() if hasattr(v, 'get_name') else v}")
        except Exception as exc:  # noqa: BLE001
            parts.append(f"{prop}=?")
    unreal.log(f"ELDORIA_INFO {path.split('/')[-1]}: " + " | ".join(parts))
