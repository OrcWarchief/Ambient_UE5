// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Ambient_UE5 : ModuleRules
{
	public Ambient_UE5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
            "GameplayTags",
            "AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Ambient_UE5",
			"Ambient_UE5/Variant_Platforming",
			"Ambient_UE5/Variant_Platforming/Animation",
			"Ambient_UE5/Variant_Combat",
			"Ambient_UE5/Variant_Combat/AI",
			"Ambient_UE5/Variant_Combat/Animation",
			"Ambient_UE5/Variant_Combat/Gameplay",
			"Ambient_UE5/Variant_Combat/Interfaces",
			"Ambient_UE5/Variant_Combat/UI",
			"Ambient_UE5/Variant_SideScrolling",
			"Ambient_UE5/Variant_SideScrolling/AI",
			"Ambient_UE5/Variant_SideScrolling/Gameplay",
			"Ambient_UE5/Variant_SideScrolling/Interfaces",
			"Ambient_UE5/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
