using UnrealBuildTool;
using System.IO;

public class BloodshiftBalanceEditor : ModuleRules
{
    public BloodshiftBalanceEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "HeavensDivide"));
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "HeavensDivide", "Slate", "SlateCore",
            "UnrealEd", "PropertyEditor", "ToolMenus", "AssetRegistry", "Json",
            "InputCore", "ContentBrowser", "RenderCore", "UMG"
        });
    }
}
