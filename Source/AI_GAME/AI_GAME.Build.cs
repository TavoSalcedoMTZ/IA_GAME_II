// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AI_GAME : ModuleRules
{
	public AI_GAME(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"NavigationSystem",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"AI_GAME",
			"AI_GAME/Variant_Horror",
			"AI_GAME/Variant_Horror/UI",
			"AI_GAME/Variant_Shooter",
			"AI_GAME/Variant_Shooter/AI",
			"AI_GAME/Variant_Shooter/UI",
			"AI_GAME/Variant_Shooter/Weapons",
			"AI_GAME/DragonRider"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
