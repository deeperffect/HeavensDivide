#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatBlueprintAudit, "HeavensDivide.Combat.BlueprintAudit",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatBlueprintAudit::RunTest(const FString &)
{
    auto &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    FARFilter Filter;
    Filter.PackagePaths.Add(TEXT("/Game/HeavensDivide"));
    Filter.bRecursivePaths = true;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    FString Report;
    int32 Compiled = 0;
    for (const auto &Asset : Assets)
    {
        auto *BP = Cast<UBlueprint>(Asset.GetAsset());
        if (!BP)
            continue;
        Report += TEXT("\nBLUEPRINT ") + BP->GetPathName() + TEXT("\n");
        TArray<UEdGraph *> Graphs;
        BP->GetAllGraphs(Graphs);
        bool bChanged = false;
        if (FParse::Param(FCommandLine::Get(), TEXT("CleanupCombatBlueprints")) &&
            (BP->GetName() == TEXT("BP_Ninja") || BP->GetName() == TEXT("BP_Samurai") ||
             BP->GetName() == TEXT("BP_NinjaProjectile")))
        {
            TArray<UEdGraphNode *> EmptyEvents;
            for (auto *Graph : Graphs)
                for (auto Node : Graph->Nodes)
                    if (Node && Node->GetClass()->GetFName() == TEXT("K2Node_Event") &&
                        !Node->Pins.ContainsByPredicate(
                            [](const UEdGraphPin *Pin) { return Pin && !Pin->LinkedTo.IsEmpty(); }))
                        EmptyEvents.Add(Node);
            if (!EmptyEvents.IsEmpty())
            {
                const FString Filename = FPackageName::LongPackageNameToFilename(
                    BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
                const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/CombatBlueprintCleanup") /
                                       (BP->GetName() + TEXT(".uasset"));
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
                if (!IFileManager::Get().FileExists(*Backup))
                    TestEqual(TEXT("Blueprint backup created"), IFileManager::Get().Copy(*Backup, *Filename), COPY_OK);
                for (auto *Node : EmptyEvents)
                    FBlueprintEditorUtils::RemoveNode(BP, Node, true);
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
                bChanged = true;
                AddInfo(
                    FString::Printf(TEXT("Removed %d unconnected events from %s"), EmptyEvents.Num(), *BP->GetName()));
            }
        }
        for (auto *Graph : Graphs)
            for (auto NodePtr : Graph->Nodes)
            {
                auto *Node = NodePtr.Get();
                if (!Node)
                    continue;
                Report += Graph->GetName() + TEXT(" | ") + Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() +
                          TEXT(" | ") + Node->GetClass()->GetName() + TEXT("\n");
                for (TFieldIterator<FProperty> It(Node->GetClass()); It; ++It)
                {
                    const FString Name = It->GetName();
                    if (!Name.Contains(TEXT("Reference")) && !Name.Contains(TEXT("Member")))
                        continue;
                    FString Value;
                    It->ExportText_InContainer(0, Value, Node, Node, Node, PPF_None);
                    Report += Name + TEXT("=") + Value + TEXT("\n");
                }
            }
        if (BP->GetPathName().Contains(TEXT("/PlayerCharacters/")) ||
            BP->GetName() == TEXT("BP_SurvivorPlayerController"))
        {
            FKismetEditorUtilities::CompileBlueprint(BP);
            TestTrue(*BP->GetPathName(), BP->Status != BS_Error);
            ++Compiled;
            if (bChanged && BP->Status != BS_Error)
            {
                const FString Filename = FPackageName::LongPackageNameToFilename(
                    BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
                FSavePackageArgs Args;
                Args.TopLevelFlags = RF_Public | RF_Standalone;
                TestTrue(TEXT("Cleaned Blueprint saved"),
                         UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, Args));
            }
        }
    }
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("CombatBlueprintReferences.txt")));
    TestTrue(TEXT("Combat Blueprints were compiled"), Compiled >= 3);
    AddInfo(FString::Printf(TEXT("Audited %d Blueprint assets; compiled %d combat Blueprints without saving."),
                            Assets.Num(), Compiled));
    return true;
}
#endif
