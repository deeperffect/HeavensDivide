#include "BalanceCatalog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "UpgradeDefinition.h"
#include "PlayerUpgradeComponent.h"
#include "PlayerUpgradeRules.h"
#include "SurvivorPlayerController.h"
#include "CharacterBase.h"
#include "EnemyBase.h"
#include "BloodshiftBalanceData.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "ScopedTransaction.h"

namespace
{
FString BaselineFile() { return FPaths::ProjectConfigDir()/TEXT("BloodshiftBalanceBaseline.json"); }
TSharedPtr<FJsonObject> ReadBaseline()
{
    FString Text; TSharedPtr<FJsonObject> Root;
    if (FFileHelper::LoadFileToString(Text,*BaselineFile()))
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root);
    return Root;
}
bool Editable(const FProperty* P)
{
    return P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_EditConst|CPF_Transient|CPF_Deprecated)
        && !P->HasMetaData(TEXT("DeprecatedProperty"));
}
FString Friendly(FString S)
{
    S.RemoveFromStart(TEXT("BP_")); S.RemoveFromStart(TEXT("DA_"));
    return FName::NameToDisplayString(S,false);
}
}

void FBalanceCatalog::Refresh()
{
    Entries.Reset();
    TSet<FString> Seen;
    auto Add=[&](UObject* Object,UBlueprint* BP,FString Name,FString Group,FString Notes,bool Inactive=false)
    {
        if (!Object || Seen.Contains(Object->GetPathName())) return;
        bool Any=false;
        for (TFieldIterator<FProperty> P(Object->GetClass());P;++P) if (Editable(*P)) { Any=true; break; }
        if (!Any) return;
        Seen.Add(Object->GetPathName());
        auto Entry=MakeShared<FBalanceEntry>();
        Entry->Object.Reset(Object); Entry->Blueprint=BP; Entry->Name=Name; Entry->Group=Group;
        Entry->Notes=Notes; Entry->bInactive=Inactive;
        Entry->Search=Name+TEXT(" ")+Group+TEXT(" ")+Object->GetPathName();
        if (auto* U=Cast<UUpgradeDefinition>(Object))
        {
            Entry->Search+=TEXT(" ")+U->UpgradeId.ToString()+TEXT(" ")+U->BuildFamilyId.ToString()+TEXT(" ")+U->Description.ToString();
            for(const auto& Parameter:U->BalanceParameters) Entry->Search+=TEXT(" ")+Parameter.Key.ToString();
        }
        if(auto* Rules=Cast<UBloodshiftBalanceData>(Object))
            for(const auto& Skill:Rules->Skills)Entry->Search+=TEXT(" ")+Skill.Name+TEXT(" ")+Skill.Id.ToString();
        for (TFieldIterator<FProperty> P(Object->GetClass());P;++P)
            if (Editable(*P)) Entry->Search+=TEXT(" ")+P->GetName();
        Entries.Add(Entry);
    };
    Add(const_cast<UBloodshiftBalanceData*>(UBloodshiftBalanceData::Get()),nullptr,TEXT("Permanent skills, hazards & rewards"),TEXT("Shared rules"),TEXT("Permanent skill bonuses, ranks, costs and prerequisites; shared encounter limits and reward pacing. Stable skill IDs must be preserved. Skill text supports {Percent} and {Magnitude}. Restart your run after saving."));
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    Registry.SearchAllAssets(true);
    FARFilter Filter; Filter.PackagePaths.Add(TEXT("/Game/HeavensDivide")); Filter.bRecursivePaths=true;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UUpgradeDefinition::StaticClass()->GetClassPathName()); Filter.bRecursiveClasses=true;
    TArray<FAssetData> Assets; Registry.GetAssets(Filter,Assets);
    auto* PCClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC=PCClass?Cast<ASurvivorPlayerController>(PCClass->GetDefaultObject()):nullptr;
    const auto* Upgrades=PC?PC->GetPlayerUpgrades():nullptr;
    const FString AssetNotes=TEXT("Edits the saved upgrade directly. Restart your run to test consistently. Fractions use 0.15 = 15%; distances are cm; times are seconds. Rarity Magnitudes drive rolled cards; Stat Modifiers drive non-rolled stat cards. Runtime Balance drives special mechanics. Text without {tokens} is authored copy and should be reviewed after number changes.");
    for (const auto& Asset:Assets)
    {
        if (Asset.IsInstanceOf(UUpgradeDefinition::StaticClass()))
        {
            auto* U=Cast<UUpgradeDefinition>(Asset.GetAsset()); if (!U) continue;
            const bool Inactive=!Upgrades || Upgrades->FindUpgradeDefinition(U->UpgradeId)!=U
                || PlayerUpgradeRules::IsRetiredUpgrade(U->UpgradeId) || PlayerUpgradeRules::IsRemovedSamuraiUpgrade(U->UpgradeId);
            FString Owner=U->InvestmentOwner==EUpgradeInvestmentOwner::Samurai?TEXT("Samurai"):
                U->InvestmentOwner==EUpgradeInvestmentOwner::Ninja?TEXT("Ninja"):TEXT("Shared");
            if(U->Category==EUpgradeCategory::Cursed) Owner=TEXT("Blood pacts");
            else if(U->Category==EUpgradeCategory::Synergy) Owner=TEXT("Synergies");
            FString Group=TEXT("Upgrades / ")+Owner;
            const FName Id=U->UpgradeId;
            FString Family=U->BuildFamilyId.IsNone()?FString():U->BuildFamilyId.ToString();
            const FString IdText=Id.ToString();
            if(IdText.StartsWith(TEXT("Barrage"))||Id==TEXT("NeedleRain")) Family=TEXT("Barrage");
            else if(IdText.StartsWith(TEXT("Fang"))||Id==TEXT("ReturningFang")) Family=TEXT("Returning Fang");
            else if(IdText.StartsWith(TEXT("Shuriken"))||Id==TEXT("GreatShuriken")) Family=TEXT("Great Shuriken");
            else if(IdText.StartsWith(TEXT("Iaijutsu"))) Family=TEXT("Iaijutsu");
            else if(IdText.StartsWith(TEXT("Crescent"))||Id==TEXT("ReturningBlade")||Family==TEXT("BladeWave")) Family=TEXT("Crescent");
            else if(IdText.StartsWith(TEXT("Blood"))||IdText.StartsWith(TEXT("DoubleCut"))||Id==TEXT("LingeringWounds")||Family==TEXT("Bleed")) Family=TEXT("Blood");
            const bool IsStance=Id==TEXT("BattleStance")||Id==TEXT("Iaijutsu")||Id==TEXT("BladeWave")||Id==TEXT("ReturningFang")||Id==TEXT("BarrageStance")||Id==TEXT("GreatShuriken");
            if (IsStance) Group=TEXT("Stances / ")+Owner;
            else if (U->Role==EUpgradeRole::Starter) Group=TEXT("Abilities / ")+Owner;
            else if (!Family.IsEmpty()) Group+=TEXT(" / ")+Family;
            Add(U,nullptr,U->DisplayName.IsEmpty()?Friendly(U->GetName()):U->DisplayName.ToString(),Group,AssetNotes,Inactive);
            continue;
        }
        if(Asset.AssetClassPath!=UBlueprint::StaticClass()->GetClassPathName()) continue;
        auto* BP=Cast<UBlueprint>(Asset.GetAsset()); if (!BP || !BP->GeneratedClass) continue;
        auto* CDO=BP->GeneratedClass->GetDefaultObject();
        // Only the project's gameplay objects; animation/UI/material Blueprints are not balance sources.
        bool Gameplay=false;
        for (UClass* C=CDO->GetClass();C;C=C->GetSuperClass())
            if (C->GetOutermost()->GetName()==TEXT("/Script/HeavensDivide") && C->IsChildOf(AActor::StaticClass())) { Gameplay=true; break; }
        if (!Gameplay || BP->GetName().Contains(TEXT("Test"))) continue;
        FString Group=CDO->IsA<ACharacterBase>()?TEXT("Characters"):
            CDO->IsA<AEnemyBase>()?TEXT("Enemies & bosses"):
            CDO->IsA<ASurvivorPlayerController>()?TEXT("Player systems"):TEXT("World & encounters");
        const FString Name=Friendly(BP->GetName());
        const FString Notes=TEXT("Edits this Blueprint's saved defaults. Components have their own entries below the same group. Restart your run after saving. Placed level instances with explicit overrides retain those overrides.");
        Add(CDO,BP,Name+TEXT(" / Defaults"),Group,Notes);
        if(auto* Actor=Cast<AActor>(CDO))
        {
            TInlineComponentArray<UActorComponent*> Components(Actor);
            for(auto* Component:Components)
            {
                if (Component->GetClass()->GetOutermost()->GetName()!=TEXT("/Script/HeavensDivide")) continue;
                Add(Component,BP,Name+TEXT(" / ")+Friendly(Component->GetName()),Group,Notes);
            }
        }
    }
    Entries.Sort([](const auto& A,const auto& B){return A->Group==B->Group?A->Name<B->Name:A->Group<B->Group;});
}

void FBalanceCatalog::Changed(const FBalanceEntry& Entry)
{
    Entry.Object->MarkPackageDirty();
    if(auto* BP=Entry.Blueprint.Get()) FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
}

bool FBalanceCatalog::Save(const TArray<TSharedPtr<FBalanceEntry>>& InEntries,FString& Result)
{
    TSet<UPackage*> Packages; TSet<UBlueprint*> Blueprints;
    for (const auto& E:InEntries)
        if(E->Object.IsValid() && E->Object->GetOutermost()->IsDirty())
        { Packages.Add(E->Object->GetOutermost()); if(E->Blueprint.IsValid()) Blueprints.Add(E->Blueprint.Get()); }
    // Back up disk versions BEFORE any compile/save. Never replace existing backups.
    const FString Backup=FPaths::ProjectSavedDir()/TEXT("BalanceBackups")/(FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    for(auto* P:Packages)
    {
        FString Source;
        if(FPackageName::DoesPackageExist(P->GetName(),&Source))
        {
            const FString Dest=Backup/P->GetName().RightChop(6)+TEXT(".uasset");
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
            if(IFileManager::Get().Copy(*Dest,*Source,false)!=COPY_OK) { Result=TEXT("Backup failed. Nothing saved."); return false; }
        }
    }
    for(auto* BP:Blueprints)
    {
        FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection);
        if(BP->Status==BS_Error) { Result=TEXT("Blueprint compile failed: ")+BP->GetName()+TEXT(". Nothing saved."); return false; }
    }
    int32 Count=0;
    for(auto* P:Packages)
    {
        const FString Filename=FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
        if(!UPackage::SavePackage(P,nullptr,*Filename,Args)) { Result=TEXT("Save failed: ")+P->GetName(); return false; }
        ++Count;
    }
    Result=FString::Printf(TEXT("Saved %d packages. Restart your run to test. Previous disk versions: Saved/BalanceBackups."),Count);
    return true;
}

bool FBalanceCatalog::CaptureBaseline(const TArray<TSharedPtr<FBalanceEntry>>& InEntries)
{
    auto Root=MakeShared<FJsonObject>();
    for(const auto& E:InEntries)
    {
        auto Values=MakeShared<FJsonObject>();
        for(TFieldIterator<FProperty> P(E->Object->GetClass());P;++P) if(Editable(*P))
        {
            FString Value; P->ExportText_InContainer(0,Value,E->Object.Get(),nullptr,E->Object.Get(),PPF_None);
            Values->SetStringField(P->GetName(),Value);
        }
        Root->SetObjectField(E->Object->GetPathName(),Values);
    }
    FString Text; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text,*BaselineFile(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool FBalanceCatalog::HasBaseline(const FBalanceEntry& Entry)
{
    const auto Root=ReadBaseline(); return Root && Root->HasField(Entry.Object->GetPathName());
}
bool FBalanceCatalog::ResetToBaseline(const FBalanceEntry& Entry)
{
    auto Root=ReadBaseline(); const TSharedPtr<FJsonObject>* Values=nullptr;
    if(!Root || !Root->TryGetObjectField(Entry.Object->GetPathName(),Values)) return false;
    const FScopedTransaction Transaction(NSLOCTEXT("BloodshiftBalance","Reset","Reset balance entry to baseline"));
    UObject* Object=Entry.Object.Get(); Object->Modify();
    if(Entry.Blueprint.IsValid()) Entry.Blueprint->Modify();
    for(TFieldIterator<FProperty> P(Object->GetClass());P;++P) if(Editable(*P))
    {
        FString Value;
        if((*Values)->TryGetStringField(P->GetName(),Value))
        { Object->PreEditChange(*P); P->ImportText_InContainer(*Value,Object,Object,PPF_None); FPropertyChangedEvent Event(*P); Object->PostEditChangeProperty(Event); }
    }
    Changed(Entry); return true;
}
