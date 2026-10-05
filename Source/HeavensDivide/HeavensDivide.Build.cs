// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavensDivide : ModuleRules
{
	public HeavensDivide(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "MediaAssets", "AnimationBudgetAllocator", "NavigationSystem", "Niagara", "AssetRegistry", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineCameras", "AIModule", "ApplicationCore", "NiagaraAnimNotifies" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "MetasoundEngine", "NiagaraEditor", "MaterialEditor", "ClothingSystemEditor", "ClothingSystemEditorInterface", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface", "ChaosCloth" });
		}
	}
}
