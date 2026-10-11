#include "BalanceWorkspaceCommandlet.h"
#include "BalanceCatalog.h"
#include "UpgradeDefinition.h"
#include "BloodshiftBalanceData.h"
#include "Engine/Blueprint.h"
#include "AutoAttackComponent.h"
#include "SamuraiCharacter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"

int32 UBalanceWorkspaceCommandlet::Main(const FString& Params)
{
    FBalanceCatalog Catalog;Catalog.Refresh();
    auto Root=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Entries;
    int32 Upgrades=0,Parameters=0;
    for(const auto& E:Catalog.Entries)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),E->Name);Row->SetStringField(TEXT("group"),E->Group);
        Row->SetStringField(TEXT("source"),E->Object->GetPathName());Row->SetBoolField(TEXT("inactive"),E->bInactive);
        if(auto* U=Cast<UUpgradeDefinition>(E->Object.Get()))
        {
            ++Upgrades;Parameters+=U->BalanceParameters.Num();
            Row->SetStringField(TEXT("id"),U->UpgradeId.ToString());
            Row->SetStringField(TEXT("description"),U->GetBalanceDescription().ToString());
            auto Values=MakeShared<FJsonObject>();for(const auto& P:U->BalanceParameters)Values->SetNumberField(P.Key.ToString(),P.Value);
            Row->SetObjectField(TEXT("parameters"),Values);
        }
        Entries.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("entries"),Entries);Root->SetNumberField(TEXT("upgrades"),Upgrades);Root->SetNumberField(TEXT("parameters"),Parameters);
    FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
    const FString Out=FPaths::ProjectSavedDir()/TEXT("BalanceWorkspace");IFileManager::Get().MakeDirectory(*Out,true);
    FFileHelper::SaveStringToFile(Text,*(Out/TEXT("Catalog.json")));
    if(FParse::Param(*Params,TEXT("CaptureBaseline")))
        if(!FBalanceCatalog::CaptureBaseline(Catalog.Entries))return 1;
    // A separate mounted validation package exercises exactly the Blueprint CDO/component save path.
    // It lives under Saved, never inside cooked project content.
    const FString Mount=FPaths::ConvertRelativePathToFull(Out/TEXT("Persistence/"));
    IFileManager::Get().MakeDirectory(*Mount,true);FPackageName::RegisterMountPoint(TEXT("/BalanceValidation/"),Mount);
    const TCHAR* BPPath=TEXT("/BalanceValidation/BP_Persistence.BP_Persistence");
    if(FParse::Param(*Params,TEXT("WritePersistence")))
    {
        auto* Package=CreatePackage(TEXT("/BalanceValidation/BP_Persistence"));
        auto* BP=FKismetEditorUtilities::CreateBlueprint(ASamuraiCharacter::StaticClass(),Package,TEXT("BP_Persistence"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
        if(!BP||!BP->GeneratedClass)return 2;
        auto* Actor=Cast<AActor>(BP->GeneratedClass->GetDefaultObject());auto* Attack=Actor->FindComponentByClass<UAutoAttackComponent>();
        auto* Damage=FindFProperty<FFloatProperty>(Attack->GetClass(),TEXT("AttackDamage"));
        Attack->Modify();Attack->PreEditChange(Damage);Damage->SetPropertyValue_InContainer(Attack,37.25f);
        FPropertyChangedEvent Event(Damage);Attack->PostEditChangeProperty(Event);
        auto E=MakeShared<FBalanceEntry>();E->Object.Reset(Attack);E->Blueprint=BP;
        FBalanceCatalog::Changed(*E);FString Result;
        if(!FBalanceCatalog::Save({E},Result)){UE_LOG(LogTemp,Error,TEXT("%s"),*Result);return 3;}
        UE_LOG(LogTemp,Display,TEXT("BALANCE_PERSISTENCE_WRITTEN %s"),*Result);
    }
    if(FParse::Param(*Params,TEXT("VerifyPersistence")))
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,BPPath);
        auto* Actor=BP&&BP->GeneratedClass?Cast<AActor>(BP->GeneratedClass->GetDefaultObject()):nullptr;
        auto* Attack=Actor?Actor->FindComponentByClass<UAutoAttackComponent>():nullptr;
        if(!Attack||!FMath::IsNearlyEqual(Attack->GetBaseAttackDamage(),37.25f))return 4;
        UE_LOG(LogTemp,Display,TEXT("BALANCE_PERSISTENCE_RELOAD_PASS: component damage = 37.25"));
    }
    UE_LOG(LogTemp,Display,TEXT("BALANCE_CATALOG_PASS: %d entries, %d upgrades, %d named parameters, %d permanent skills"),Catalog.Entries.Num(),Upgrades,Parameters,UBloodshiftBalanceData::Get()->Skills.Num());
    return Upgrades>=140&&Parameters>=200?0:5;
}
