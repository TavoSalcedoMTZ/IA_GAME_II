using UnrealBuildTool;

// Modulo de editor
public class AI_GAMEEditor : ModuleRules
{
	public AI_GAMEEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry",
			"AIModule", "GameplayTags", "NavigationSystem", "RHI",
			"StateTreeModule", "StateTreeEditorModule", "GameplayStateTreeModule", "PropertyBindingUtils",
			"AI_GAME"
		});
	}
}
