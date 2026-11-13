using UnrealBuildTool;

// Loads before the engine finishes starting up so the splash covers startup and every map load.
// Keep it free of UObjects: nothing from the game module or its assets exists this early.
public class EldoriaLoadingScreen : ModuleRules
{
	public EldoriaLoadingScreen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "Slate", "SlateCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "MoviePlayer", "Projects" });
	}
}
