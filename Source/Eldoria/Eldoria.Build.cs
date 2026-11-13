using UnrealBuildTool;

public class Eldoria : ModuleRules
{
	public Eldoria(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"AIModule", "NavigationSystem", "DeveloperSettings", "UMG", "Slate", "SlateCore", "RenderCore", "RHI", "EldoriaLoadingScreen", "Niagara"
		});
	}
}
