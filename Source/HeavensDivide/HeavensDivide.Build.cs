// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavensDivide : ModuleRules
{
	public HeavensDivide(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "MediaAssets", "AnimationBudgetAllocator", "NavigationSystem", "Niagara", "AssetRegistry", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineCameras", "AIModule", "ApplicationCore", "NiagaraAnimNotifies" });
		PrivateDependencyModuleNames.Add("OnlineSubsystem");
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("Steamworks");
			DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
			PublicDefinitions.Add("WITH_BLOODSHIFT_STEAM=1");
		}
		else PublicDefinitions.Add("WITH_BLOODSHIFT_STEAM=0");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "RenderCore", "MetasoundEngine", "NiagaraEditor", "MaterialEditor", "ClothingSystemEditor", "ClothingSystemEditorInterface", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface", "ChaosCloth" });
		}
	}
}
