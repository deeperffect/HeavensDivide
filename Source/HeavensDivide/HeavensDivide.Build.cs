// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavensDivide : ModuleRules
{
	public HeavensDivide(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "AnimationBudgetAllocator", "NavigationSystem", "Niagara", "AssetRegistry", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
