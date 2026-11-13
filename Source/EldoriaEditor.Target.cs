using UnrealBuildTool;

public class EldoriaEditorTarget : TargetRules
{
	public EldoriaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "Eldoria", "EldoriaLoadingScreen" });
	}
}
