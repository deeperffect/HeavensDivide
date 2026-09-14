#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EnemyBase.h"
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

namespace EnemyBlueprintAudit
{
FString Defaults(UBlueprint* Blueprint)
{
    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    TArray<UObject*> Objects;
    CDO->GetDefaultSubobjects(Objects);
    Objects.Add(CDO);
    TArray<FString> Lines;
    for (UObject* Object : Objects)
        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Edit) || It->HasAnyPropertyFlags(CPF_Transient)) continue;
            FString Value;
            It->ExportText_InContainer(0, Value, Object, Object, Object, PPF_None);
            Lines.Add(Object->GetName() + TEXT(".") + It->GetName() + TEXT("=") + Value);
        }
    Lines.Sort();
    return FString::Join(Lines, TEXT("\n"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBlueprintAudit, "HeavensDivide.Enemies.BlueprintAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyBlueprintAudit::RunTest(const FString&)
{
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    FARFilter Filter;
    Filter.PackagePaths.Add(TEXT("/Game"));
    Filter.bRecursivePaths = true;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    TArray<FString> Snapshots;
    int32 EnemyCount = 0, Compiled = 0, Removed = 0;
    for (const FAssetData& Asset : Assets)
    {
        const FString Path = Asset.PackageName.ToString();
        // Includes enemy animation Blueprints, UI, spawner, projectiles and objective enemies.
        if (!Path.Contains(TEXT("/EnemyCharacters/")) && !Path.Contains(TEXT("/TwinSoulTrial/")) &&
            !Path.Contains(TEXT("/EnemyUI/")) && !Path.EndsWith(TEXT("BP_TwinSoulTrial")) &&
            !Path.EndsWith(TEXT("BP_BossToriiGate"))) continue;
        UBlueprint* BP = Cast<UBlueprint>(Asset.GetAsset());
        if (!BP || !BP->GeneratedClass) continue;
        const bool bEnemy = BP->GeneratedClass->IsChildOf(AEnemyBase::StaticClass());
        EnemyCount += bEnemy;
        const FString Before = EnemyBlueprintAudit::Defaults(BP);
        bool bChanged = false;
        if ((bEnemy && FParse::Param(FCommandLine::Get(), TEXT("CleanupEnemyBlueprints"))) ||
            (BP->GetName() == TEXT("BP_EnemySpawner") && FParse::Param(FCommandLine::Get(), TEXT("CleanupSpawnerBlueprint"))))
        {
            TArray<UEdGraph*> Graphs;
            BP->GetAllGraphs(Graphs);
            TArray<UEdGraphNode*> EmptyEvents;
            for (UEdGraph* Graph : Graphs)
                for (auto Node : Graph->Nodes)
                {
                    if (!Node || Node->GetClass()->GetFName() != TEXT("K2Node_Event")) continue;
                    const FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
                    if (Title != TEXT("Event BeginPlay") && Title != TEXT("Event ActorBeginOverlap") &&
                        Title != TEXT("Event Tick")) continue;
                    if (!Node->Pins.ContainsByPredicate([](const UEdGraphPin* Pin)
                        { return Pin && !Pin->LinkedTo.IsEmpty(); })) EmptyEvents.Add(Node);
                }
            if (!EmptyEvents.IsEmpty())
            {
                const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
                const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/EnemyCleanup") / (Path.Mid(6) + TEXT(".uasset"));
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
                if (!IFileManager::Get().FileExists(*Backup) &&
                    !TestEqual(TEXT("Enemy backup created"), IFileManager::Get().Copy(*Backup, *Filename), COPY_OK)) return false;
                for (UEdGraphNode* Node : EmptyEvents) FBlueprintEditorUtils::RemoveNode(BP, Node, true);
                Removed += EmptyEvents.Num();
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
                bChanged = true;
            }
        }
        FKismetEditorUtilities::CompileBlueprint(BP);
        ++Compiled;
        const bool bCompiled = TestTrue(*Path, BP->Status != BS_Error);
        const FString After = EnemyBlueprintAudit::Defaults(BP);
        const bool bSameDefaults = TestTrue(*(Path + TEXT(" preserves editable defaults")), Before == After);
        Snapshots.Add(Path + TEXT("\n") + After);
        if (bChanged && bCompiled && bSameDefaults)
        {
            const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
            FSavePackageArgs Args;
            Args.TopLevelFlags = RF_Public | RF_Standalone;
            TestTrue(TEXT("Enemy Blueprint saved"), UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, Args));
        }
        AddInfo(Path + TEXT(" parent=") + GetNameSafe(BP->ParentClass));
    }
    Snapshots.Sort();
    const FString Snapshot = FString::Join(Snapshots, TEXT("\n\n"));
    const FString BaselinePath = FPaths::ProjectSavedDir() / TEXT("EnemyCleanupBaseline.txt");
    if (FParse::Param(FCommandLine::Get(), TEXT("WriteEnemyCleanupBaseline")))
        TestTrue(TEXT("Baseline saved"), FFileHelper::SaveStringToFile(Snapshot, *BaselinePath));
    else
    {
        FString Baseline;
        if (FFileHelper::LoadFileToString(Baseline, *BaselinePath))
            TestTrue(TEXT("All enemy editable defaults match pre-cleanup baseline"), Baseline == Snapshot);
    }
    FFileHelper::SaveStringToFile(Snapshot, *(FPaths::ProjectSavedDir() / TEXT("EnemyCleanupCurrent.txt")));
    TestTrue(TEXT("Enemy roster found"), EnemyCount >= 14);
    AddInfo(FString::Printf(TEXT("Compiled %d Blueprints, including %d enemies; removed %d empty events."), Compiled, EnemyCount, Removed));
    return true;
}
#endif
