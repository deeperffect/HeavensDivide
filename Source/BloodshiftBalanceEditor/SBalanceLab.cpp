#include "SBalanceLab.h"
#include "BalanceLabDirector.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"
#include "PlayerUpgradeRules.h"
#include "TesterBalanceSettings.h"
#include "HealthComponent.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "PlayInEditorDataTypes.h"
#include "Engine/World.h"
#include "UnrealEngine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

namespace
{
const TCHAR* LabMap = TEXT("/Game/Maps/Lvl_BalanceLab");
TSharedRef<SVerticalBox> Section(const TSharedRef<SVerticalBox>& Parent, const TCHAR* Title, const TCHAR* Description)
{
    auto Content = SNew(SVerticalBox);
    Parent->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
    [SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(12)
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title)).Font(FAppStyle::GetFontStyle("NormalFontBold"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)[SNew(STextBlock).Text(FText::FromString(Description)).AutoWrapText(true)]
            +SVerticalBox::Slot().AutoHeight()[Content]]];
    return Content;
}
TSharedRef<SWidget> NumberField(const TCHAR* Label, int32& Value, int32 Min, int32 Max)
{
    return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(FText::FromString(Label))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(75)
            [SNew(SSpinBox<int32>).MinValue(Min).MaxValue(Max).MinSliderValue(Min).MaxSliderValue(Max)
                .Value_Lambda([&Value]{ return Value; }).OnValueChanged_Lambda([&Value](int32 NewValue){ Value = NewValue; })]];
}
}

ABalanceLabDirector* SBalanceLab::GetLab()
{
    return GEditor && GEditor->PlayWorld && GEditor->PlayWorld->WorldType == EWorldType::PIE
        ? ABalanceLabDirector::Find(GEditor->PlayWorld) : nullptr;
}
bool SBalanceLab::CanControlLab() const
{
    const auto* Lab = GetLab();
    return Lab && Lab->IsReady();
}
FReply SBalanceLab::Execute(const FString& Action, const FString& Value, int32 Count)
{
    auto* Lab = GetLab();
    if (!Lab || !Lab->IsReady())
    {
        Feedback = TEXT("Start Play In Editor on Lvl_BalanceLab and wait for the player. Restart the test after dying.");
        return FReply::Handled();
    }
    auto* PC = Cast<ASurvivorPlayerController>(Lab->GetWorld()->GetFirstPlayerController());
    FScopedConditionalWorldSwitcher WorldSwitcher(Lab->GetWorld());
    Feedback = Lab->Command(PC, Action, Value, Count);
    return FReply::Handled();
}
TSharedRef<SWidget> SBalanceLab::ActionButton(const FString& Label, const FString& Action, const FString& Value)
{
    return SNew(SButton).Text(FText::FromString(Label)).IsEnabled(this, &SBalanceLab::CanControlLab)
        .OnClicked_Lambda([this, Action, Value]{ return Execute(Action, Value); });
}
FText SBalanceLab::SessionText() const
{
    auto* Lab = GetLab();
    if (!Lab) return FText::FromString(GEditor && GEditor->PlayWorld
        ? TEXT("Another map is playing. Stop it, then open the lab map.")
        : TEXT("No lab running. Open the lab map and start a test."));
    if (!Lab->IsReady()) return FText::FromString(TEXT("Waiting for the lab player. If the player died, stop and restart the test."));
    const auto* PC = Cast<ASurvivorPlayerController>(Lab->GetWorld()->GetFirstPlayerController());
    const bool bInvulnerable = PC && PC->GetPlayerHealthComponent() && !PC->GetPlayerHealthComponent()->IsDamageEnabled();
    return FText::FromString(Lab->Report() + (bInvulnerable ? TEXT("\nInvulnerability ON") : TEXT("\nIncoming damage ON")));
}
FReply SBalanceLab::OpenMap()
{
    if (!GEditor || GEditor->PlayWorld || GEditor->GetPlaySessionRequest().IsSet()) return FReply::Handled();
    const auto* World = GEditor->GetEditorWorldContext().World();
    if (World && World->GetOutermost()->GetName() == LabMap) { Feedback = TEXT("Lab map is open. Click Start test."); return FReply::Handled(); }
    // Loading a specified map does not itself offer to save unsaved work.
    if (!FEditorFileUtils::SaveDirtyPackages(true, true, true)) { Feedback = TEXT("Map change cancelled."); return FReply::Handled(); }
    const FString Filename = FPaths::ProjectContentDir() / TEXT("Maps/Lvl_BalanceLab.umap");
    Feedback = FEditorFileUtils::LoadMap(Filename) ? TEXT("Lab map opened. Click Start test.") : TEXT("Could not open Lvl_BalanceLab. Check the Output Log.");
    return FReply::Handled();
}
FReply SBalanceLab::StartTest()
{
    if (!GEditor || GEditor->PlayWorld || GEditor->GetPlaySessionRequest().IsSet()) return FReply::Handled();
    const auto* World = GEditor->GetEditorWorldContext().World();
    if (!World || World->GetOutermost()->GetName() != LabMap) { Feedback = TEXT("Click Open lab map before starting the test."); return FReply::Handled(); }
    FRequestPlaySessionParams Params;
    Params.WorldType = EPlaySessionWorldType::PlayInEditor;
    Params.SessionDestination = EPlaySessionDestinationType::InProcess;
    GEditor->RequestPlaySession(Params);
    Feedback = TEXT("Starting the lab in a play window. Shift+F1 releases the mouse; click the game to resume control.");
    return FReply::Handled();
}
void SBalanceLab::LoadUpgrades()
{
    auto* PCClass = LoadClass<ASurvivorPlayerController>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    const auto* PC = PCClass ? Cast<ASurvivorPlayerController>(PCClass->GetDefaultObject()) : nullptr;
    const auto* Pool = PC ? PC->GetPlayerUpgrades() : nullptr;
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    Registry.ScanPathsSynchronous({TEXT("/Game/HeavensDivide/Upgrades")});
    FARFilter Filter;
    Filter.ClassPaths.Add(UUpgradeDefinition::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);
    for (const auto& Asset : Assets)
    {
        auto* Card = Cast<UUpgradeDefinition>(Asset.GetAsset());
        if (!Card || !Pool || Pool->FindUpgradeDefinition(Card->UpgradeId) != Card
            || PlayerUpgradeRules::IsRetiredUpgrade(Card->UpgradeId) || PlayerUpgradeRules::IsRemovedSamuraiUpgrade(Card->UpgradeId)) continue;
        auto Entry = MakeShared<FBalanceLabUpgrade>();
        Entry->Id = Card->UpgradeId;
        Entry->Label = Card->DisplayName.ToString() + TEXT(" (") + Card->UpgradeId.ToString() + TEXT(")");
        Entry->Description = Card->GetBalanceDescription().ToString();
        Upgrades.Add(Entry);
    }
    Upgrades.Sort([](const auto& A, const auto& B){ return A->Label < B->Label; });
    FilteredUpgrades = Upgrades;
    for (const auto& Entry : Upgrades) if (Entry->Id == TEXT("BloodCritical")) SelectedUpgrade = Entry;
    if (!SelectedUpgrade && !Upgrades.IsEmpty()) SelectedUpgrade = Upgrades[0];
}
void SBalanceLab::FilterUpgrades()
{
    FilteredUpgrades.Reset();
    for (const auto& Entry : Upgrades) if (Search.IsEmpty() || Entry->Label.Contains(Search) || Entry->Description.Contains(Search)) FilteredUpgrades.Add(Entry);
    if (!FilteredUpgrades.Contains(SelectedUpgrade)) SelectedUpgrade = FilteredUpgrades.IsEmpty() ? nullptr : FilteredUpgrades[0];
    if (UpgradePicker) { UpgradePicker->RefreshOptions(); UpgradePicker->SetSelectedItem(SelectedUpgrade); }
}
FText SBalanceLab::UpgradeText() const
{
    if (!SelectedUpgrade) return FText::FromString(TEXT("No matching upgrade. Try a different search."));
    FString Text = SelectedUpgrade->Description;
    if (const auto* Lab = GetLab(); Lab && Lab->IsReady())
    {
        const auto* PC = Cast<ASurvivorPlayerController>(Lab->GetWorld()->GetFirstPlayerController());
        auto* U = PC ? PC->GetPlayerUpgrades() : nullptr;
        auto* Card = U ? U->FindUpgradeDefinition(SelectedUpgrade->Id) : nullptr;
        if (Card)
        {
            Text = Card->GetBalanceDescription().ToString();
            Text += FString::Printf(TEXT("\nCurrent rank: %d. %s"), U->GetUpgradeLevel(Card),
                U->CanAcquireUpgrade(Card) ? TEXT("Available to grant.") : TEXT("Locked by prerequisites, exclusions or rank cap."));
        }
    }
    return FText::FromString(Text);
}
void SBalanceLab::Construct(const FArguments& Args)
{
    Targets.Add(MakeShared<FString>(TEXT("Dummy")));
    for (const auto& Row : UTesterBalanceSettings::Roster())
        if (Row.Key.Replace(TEXT(" "), TEXT("")) != TEXT("TestDummy")) Targets.Add(MakeShared<FString>(Row.Key));
    SelectedTarget = Targets[0];
    LoadUpgrades();
    const auto Content = SNew(SVerticalBox);
    ChildSlot
    [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(14, 10)
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("BALANCE LAB"))).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0, 5)[SNew(STextBlock).Text(FText::FromString(TEXT("Repeatable combat tests. These controls affect only the current lab session."))).AutoWrapText(true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0, 5)[SNew(STextBlock).Text(this, &SBalanceLab::SessionText).AutoWrapText(true).ColorAndOpacity(FLinearColor(.35f, .85f, .85f))]]
        +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot().Padding(14, 0)[Content]]
        +SVerticalBox::Slot().AutoHeight().Padding(14, 10)[SNew(STextBlock).Text_Lambda([this]{ return FText::FromString(Feedback); }).AutoWrapText(true)]];

    auto Session = Section(Content, TEXT("1. Open and play"), TEXT("Open the arena, then start a test. The player starts invulnerable beside a dummy. During play, press Shift+F1 to release the mouse and click these controls; click the game to resume moving."));
    auto SessionButtons = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6));
    Session->AddSlot().AutoHeight()[SessionButtons];
    SessionButtons->AddSlot()[SNew(SButton).Text(FText::FromString(TEXT("Open lab map")))
        .IsEnabled_Lambda([]{ return GEditor && !GEditor->PlayWorld && !GEditor->GetPlaySessionRequest().IsSet(); }).OnClicked(this, &SBalanceLab::OpenMap)];
    SessionButtons->AddSlot()[SNew(SButton).Text(FText::FromString(TEXT("Start test")))
        .IsEnabled_Lambda([]{ const auto* W = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr; return W && W->GetOutermost()->GetName() == LabMap && !GEditor->PlayWorld && !GEditor->GetPlaySessionRequest().IsSet(); }).OnClicked(this, &SBalanceLab::StartTest)];
    SessionButtons->AddSlot()[SNew(SButton).Text(FText::FromString(TEXT("Stop test"))).IsEnabled_Lambda([]{ return GetLab() != nullptr; })
        .OnClicked_Lambda([this]{ if (GEditor && GetLab()) GEditor->RequestEndPlayMap(); Feedback = TEXT("Test stopped. Start again for a fresh session."); return FReply::Handled(); })];

    auto Builds = Section(Content, TEXT("2. Choose a build"), TEXT("Choose 0-5 common damage and speed ranks, then click a stance. This resets all run upgrades, heals the player, spawns a fresh dummy and resets DPS. Permanent-stat and invulnerability settings stay as selected. Crescent and Fang speed ranks improve travel speed."));
    Builds->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[NumberField(TEXT("Damage + speed ranks"), BuildRanks, 0, 5)];
    auto BuildButtons = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6));
    Builds->AddSlot().AutoHeight()[BuildButtons];
    for (const FString Name : {TEXT("Blood"), TEXT("Crescent"), TEXT("Iaijutsu"), TEXT("Fang"), TEXT("Barrage"), TEXT("Shuriken")})
        BuildButtons->AddSlot()[SNew(SButton).Text(FText::FromString(Name)).IsEnabled(this, &SBalanceLab::CanControlLab)
            .OnClicked_Lambda([this, Name]{ return Execute(TEXT("build"), Name, BuildRanks); })];

    auto Enemies = Section(Content, TEXT("3. Choose targets"), TEXT("Dummy is stationary with very high health. Other enemies move and attack normally. Spawning replaces old targets and effects and resets DPS while preserving your build. Approach the dummy for melee tests. Pack DPS combines damage across every target."));
    Enemies->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&Targets).InitiallySelectedItem(SelectedTarget)
            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return SNew(STextBlock).Text(FText::FromString(*Item)); })
            .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type){ SelectedTarget = Item; })
            [SNew(STextBlock).Text_Lambda([this]{ return FText::FromString(SelectedTarget ? *SelectedTarget : TEXT("Choose an enemy")); })]];
    Enemies->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[NumberField(TEXT("Enemy count"), TargetCount, 1, 40)];
    Enemies->AddSlot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6))
        +SWrapBox::Slot()[SNew(SButton).Text(FText::FromString(TEXT("Spawn selected targets"))).IsEnabled(this, &SBalanceLab::CanControlLab)
            .OnClicked_Lambda([this]{ return Execute(TEXT("target"), SelectedTarget ? SelectedTarget->Replace(TEXT(" "), TEXT("")) : TEXT("Dummy"), TargetCount); })]
        +SWrapBox::Slot()[ActionButton(TEXT("Fresh single dummy"), TEXT("target"), TEXT("Dummy"))]
        +SWrapBox::Slot()[SNew(SButton).Text(FText::FromString(TEXT("12 Grunts"))).IsEnabled(this, &SBalanceLab::CanControlLab)
            .OnClicked_Lambda([this]{ return Execute(TEXT("target"), TEXT("Grunt"), 12); })]
        +SWrapBox::Slot()[ActionButton(TEXT("Clear targets"), TEXT("clear"))]];

    auto Meta = Section(Content, TEXT("4. Permanent combat stats"), TEXT("Fresh previews zero permanent combat bonuses; Full previews maximum bonuses. Saved skills are unchanged. Both keep your run upgrades and create a fresh dummy and measurement."));
    Meta->AddSlot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6))
        +SWrapBox::Slot()[ActionButton(TEXT("Fresh stats"), TEXT("meta"), TEXT("fresh"))]
        +SWrapBox::Slot()[ActionButton(TEXT("Full stats"), TEXT("meta"), TEXT("full"))]];

    auto Cards = Section(Content, TEXT("5. Add a specific upgrade"), TEXT("Search by name or ID, select a card and choose how many ranks to add. Grants follow normal prerequisites, exclusions and rank caps. After granting, use Fresh single dummy to clear existing effects before measuring."));
    Cards->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSearchBox).HintText(FText::FromString(TEXT("Search upgrades...")))
        .OnTextChanged_Lambda([this](const FText& Text){ Search = Text.ToString(); FilterUpgrades(); })];
    Cards->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [SAssignNew(UpgradePicker, SComboBox<TSharedPtr<FBalanceLabUpgrade>>).OptionsSource(&FilteredUpgrades).InitiallySelectedItem(SelectedUpgrade)
            .OnGenerateWidget_Lambda([](TSharedPtr<FBalanceLabUpgrade> Item){ return SNew(STextBlock).Text(FText::FromString(Item->Label)); })
            .OnSelectionChanged_Lambda([this](TSharedPtr<FBalanceLabUpgrade> Item, ESelectInfo::Type){ SelectedUpgrade = Item; })
            [SNew(STextBlock).Text_Lambda([this]{ return FText::FromString(SelectedUpgrade ? SelectedUpgrade->Label : TEXT("No matching upgrade")); }).AutoWrapText(true)]];
    Cards->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(STextBlock).Text(this, &SBalanceLab::UpgradeText).AutoWrapText(true).ColorAndOpacity(FLinearColor(.85f, .76f, .53f))];
    Cards->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[NumberField(TEXT("Ranks to add"), GrantRanks, 1, 10)];
    Cards->AddSlot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Grant selected upgrade"))).IsEnabled_Lambda([this]{ return CanControlLab() && SelectedUpgrade.IsValid(); })
        .OnClicked_Lambda([this]{ return Execute(TEXT("grant"), SelectedUpgrade->Id.ToString(), GrantRanks); })];

    auto Damage = Section(Content, TEXT("6. Incoming damage and combo"), TEXT("Invulnerability lets you measure attacks safely. Enable incoming damage to test dodging and survival. Fill combo only fills the meter; trigger the ability using your normal game input. Let an active combo finish before changing the sample."));
    Damage->AddSlot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6))
        +SWrapBox::Slot()[ActionButton(TEXT("Invulnerability ON"), TEXT("god"), TEXT("on"))]
        +SWrapBox::Slot()[ActionButton(TEXT("Incoming damage ON"), TEXT("god"), TEXT("off"))]
        +SWrapBox::Slot()[ActionButton(TEXT("Fill combo"), TEXT("combo"))]];

    auto Reports = Section(Content, TEXT("7. Save a measurement"), TEXT("Attack for 30-60 seconds, then save a report. The timer starts on your first hit. Reports append actual damage, aggregate DPS and upgrade ranks to Saved/BalanceLab/Measurements.txt. Compare equal durations, target counts and distances."));
    Reports->AddSlot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6))
        +SWrapBox::Slot()[ActionButton(TEXT("Save report"), TEXT("report"))]
        +SWrapBox::Slot()[SNew(SButton).Text(FText::FromString(TEXT("Open reports folder"))).OnClicked_Lambda([]
            { const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("BalanceLab")); IFileManager::Get().MakeDirectory(*Folder, true); FPlatformProcess::ExploreFolder(*Folder); return FReply::Handled(); })]];
}
