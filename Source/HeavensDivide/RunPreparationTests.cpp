#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RunLoadoutSubsystem.h"
#include "RunStanceCatalog.h"
#include "RunChallengeSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "HeavensDivideMetaSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "StanceSelectionWidget.h"
#include "PlayerUpgradeComponent.h"
#include "PlayerUpgradeRules.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "MinimapWidget.h"
#include "MinimapBounds.h"
#include "MinimapRegistrySubsystem.h"
#include "MinimapMarkerComponent.h"
#include "RunObjectiveDirector.h"
#include "ObjectiveDirectorRunStateSubsystem.h"
#include "ObjectiveSpawnPoint.h"
#include "EnemySpawner.h"
#include "BloodShrine.h"
#include "HealingPickup.h"
#include "MagnetPickup.h"
#include "SamuraiTechniqueTrial.h"
#include "NinjaTechniqueTrial.h"
#include "TwinSoulTrial.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "RenderingThread.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

namespace
{
UWorld* PreparationWorld(const FString& Slot)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    // Prepend the private slot so subsystem initialization cannot touch a player profile.
    FCommandLine::Set(*(TEXT("-MetaSaveSlot=")+Slot+TEXT(" ")+PreviousCommandLine));
    auto* GI=NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    FCommandLine::Set(*PreviousCommandLine);
    auto* W=GI->GetWorld();
    W->InitializeActorsForPlay(FURL());
    return W;
}
UClass* PreparationControllerClass()
{
    return LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPreparationTest,"HeavensDivide.Progression.RunPreparation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRunPreparationTest::RunTest(const FString&)
{
    const FString Slot=TEXT("Automation_RunPreparation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* W=PreparationWorld(Slot);
    auto* GI=W->GetGameInstance();
    auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    Meta->TestSaveSlot=Slot; Meta->CreateFreshSave();
    auto* Loadout=GI->GetSubsystem<URunLoadoutSubsystem>();
    TestEqual(TEXT("New Samurai defaults to Blood"),Loadout->GetSamuraiStance(),FName(TEXT("BattleStance")));
    TestEqual(TEXT("New Ninja defaults to Barrage"),Loadout->GetNinjaStance(),FName(TEXT("BarrageStance")));
    TestEqual(TEXT("New loadout starts with one synergy"),Loadout->GetSelectedSynergies().Num(),1);
    TestTrue(TEXT("Tag Team is selected by default"),Loadout->GetSelectedSynergies().Contains(TEXT("TagTeam")));
    TestTrue(TEXT("Default pair is available"),Loadout->SetStances(TEXT("BattleStance"),TEXT("BarrageStance")));
    TestFalse(TEXT("Locked Samurai cannot enter a loadout"),Loadout->SetStances(TEXT("Iaijutsu"),TEXT("BarrageStance")));
    TestFalse(TEXT("Locked Ninja cannot enter a loadout"),Loadout->SetStances(TEXT("BattleStance"),TEXT("ReturningFang")));
    Meta->CurrentSave->SoulSigils=1000;
    for(const auto& Stance:RunStances::All)
        if(!RunStances::IsDefaultUnlocked(Stance.Id)) TestTrue(TEXT("Stance deliberately purchased for loadout coverage"),Meta->PurchaseStance(Stance.Id));
    TestFalse(TEXT("Cross-character loadout rejected"),Loadout->SetStances(TEXT("ReturningFang"),TEXT("Iaijutsu")));
    TestFalse(TEXT("Unknown stance rejected"),Loadout->SetStances(TEXT("invalid"),TEXT("ReturningFang")));
    for(int32 A=0;A<3;++A)for(int32 B=3;B<6;++B)
    {
        auto* PC=W->SpawnActor<ASurvivorPlayerController>(PreparationControllerClass());
        auto* U=PC->GetPlayerUpgrades();
        // Execute the saved Blueprint startup graph as well: legacy debug grants
        // here used to replace the menu's Ninja choice before native initialization.
        PC->ProcessEvent(PC->FindFunctionChecked(TEXT("ReceiveBeginPlay")),nullptr);
        for(const auto& Stance:RunStances::All)TestFalse(TEXT("Blueprint startup does not force a stance"),U->HasUpgradeId(Stance.Id));
        TestTrue(TEXT("All nine stance pairs accepted"),Loadout->SetStances(RunStances::All[A].Id,RunStances::All[B].Id));
        TestTrue(TEXT("Chosen pair granted to fresh run"),Loadout->ApplyToFreshRun(U));
        TestTrue(TEXT("Selected Tag Team is active at run start"),U->HasUpgradeId(TEXT("TagTeam")));
        const int32 Steel=U->GetSamuraiMasteryPoints(),Shadow=U->GetNinjaMasteryPoints();
        TestTrue(TEXT("Applying twice is idempotent"),Loadout->ApplyToFreshRun(U));
        TestEqual(TEXT("No duplicate Samurai mastery"),U->GetSamuraiMasteryPoints(),Steel);
        TestEqual(TEXT("No duplicate Ninja mastery"),U->GetNinjaMasteryPoints(),Shadow);
        FPlayerUpgradeRunState Snapshot;U->CaptureRunState(Snapshot);
        auto* Next=W->SpawnActor<ASurvivorPlayerController>(PreparationControllerClass());
        Next->GetPlayerUpgrades()->RestoreRunState(Snapshot);
        TestTrue(TEXT("Samurai stance survives travel"),Next->GetPlayerUpgrades()->HasUpgradeId(RunStances::All[A].Id));
        TestTrue(TEXT("Ninja stance survives travel"),Next->GetPlayerUpgrades()->HasUpgradeId(RunStances::All[B].Id));
        TestTrue(TEXT("Normal level-up opens category choices"),U->BeginUpgradeSelection(2));
        TestEqual(TEXT("Two category cards are offered"),U->GetCurrentCategoryChoices().Num(),2);
        const auto Category=U->GetCurrentCategoryChoices()[0];
        const int32 ExpectedChoices=FMath::Min(3,U->GetEligibleUpgradesForCategory(Category).Num());
        TestTrue(TEXT("Offered category can be selected"),U->SelectCategory(Category,3));
        TestEqual(TEXT("Selected category fills its offer from unlocked eligible cards"),U->GetCurrentUpgradeChoices().Num(),ExpectedChoices);
        const auto Synergies=U->GetEligibleUpgradesForCategory(EUpgradeCategory::Synergy);
        TestTrue(TEXT("Starting synergies do not consume level-up choices"),Synergies.IsEmpty());
        TestFalse(TEXT("Synergy is excluded from level-up categories"),U->GetCurrentCategoryChoices().Contains(EUpgradeCategory::Synergy));
        TestTrue(TEXT("Category draft permits unlocked draft tools"),U->IsDraftToolOffer());
        for(auto* Card:U->GetCurrentUpgradeChoices())
        {
            TestFalse(TEXT("Stances never consume normal level-ups"),PlayerUpgradeRules::IsTrialBuildStarter(Card));
            TestFalse(TEXT("Pacts remain shrine rewards"),PlayerUpgradeRules::IsBloodShrineUpgrade(Card));
            TestTrue(TEXT("Draft respects prerequisites"),U->CanAcquireUpgrade(Card));
            TestEqual(TEXT("Choices belong to the selected category"),Card->Category,Category);
        }
        const FName MajorIds[]={TEXT("DoubleCut"),TEXT("IaijutsuChain"),TEXT("ReturningBlade"),TEXT("FangTwin"),TEXT("NeedleRain"),TEXT("ShurikenTwin")};
        for(int32 I:{A,B}) TestTrue(TEXT("Attack-changing techniques stay in level-ups"),
            U->GetEligibleUpgradesForCategory(I<3?EUpgradeCategory::Samurai:EUpgradeCategory::Ninja)
                .ContainsByPredicate([&](auto* C){return C->UpgradeId==MajorIds[I];}));
        PC->Destroy();Next->Destroy();
    }
    Meta->CreateFreshSave();
    auto* ResetPC=W->SpawnActor<ASurvivorPlayerController>(PreparationControllerClass());
    TestFalse(TEXT("Profile reset cannot reapply a now locked loadout"),Loadout->ApplyToFreshRun(ResetPC->GetPlayerUpgrades()));
    TestTrue(TEXT("Default pair can replace stale selection after reset"),Loadout->SetStances(TEXT("BattleStance"),TEXT("BarrageStance")));
    TestTrue(TEXT("Default pair applies after reset"),Loadout->ApplyToFreshRun(ResetPC->GetPlayerUpgrades()));
    ResetPC->Destroy();
    auto* Source=W->SpawnActor<AEnemySpawner>();
    Source->DispatchBeginPlay();Source->SetSpawningEnabled(false);
    ++GFrameCounter;W->GetTimerManager().Tick(0.f);
    auto* Director=W->SpawnActor<ARunObjectiveDirector>();
    Director->RunTimeSource=Source;Director->BloodShrineClass=ABloodShrine::StaticClass();
    for(int32 I=0;I<6;++I)Director->SpawnPoints.Add(W->SpawnActor<AObjectiveSpawnPoint>(FVector(I*1600,1800,0),FRotator::ZeroRotator));
    Director->bEnableCharacterTrials=true; // Stale Blueprint overrides cannot restore retired trials.
    Director->SamuraiTrialClass=ASamuraiTechniqueTrial::StaticClass();
    Director->NinjaTrialClass=ANinjaTechniqueTrial::StaticClass();
    Director->TwinSoulTrialClass=ATwinSoulTrial::StaticClass();
    Director->ScheduleMilestones();
    for(int32 I:{1,2,3})
    {
        TestFalse(TEXT("Retired trials have no timers"),W->GetTimerManager().IsTimerActive(Director->MilestoneTimers[I]));
        Director->ExecuteMilestoneInternal(I,true);
        TestFalse(TEXT("Retired trial milestones never execute"),W->GetSubsystem<UObjectiveDirectorRunStateSubsystem>()->HasExecuted(I));
    }
    Director->DebugSpawnGuaranteedCharacterTrial();Director->DebugSpawnTwinSoulTrial();
    TestFalse(TEXT("Legacy character-trial debug does nothing"),bool(TActorIterator<ASamuraiTechniqueTrial>(W)));
    TestFalse(TEXT("Legacy Ninja-trial debug does nothing"),bool(TActorIterator<ANinjaTechniqueTrial>(W)));
    TestFalse(TEXT("Legacy Twin Soul debug does nothing"),bool(TActorIterator<ATwinSoulTrial>(W)));
    TestNull(TEXT("Legacy trial class cannot be spawned by director"),Director->SpawnObjective(ATwinSoulTrial::StaticClass(),TEXT("Retired")));
    for(UClass* TrialClass:{ASamuraiTechniqueTrial::StaticClass(),ANinjaTechniqueTrial::StaticClass(),ATwinSoulTrial::StaticClass()})
    {
        auto* LegacyTrial=W->SpawnActor<AActor>(TrialClass);
        LegacyTrial->DispatchBeginPlay();
        TestTrue(TEXT("Legacy placed trials remove themselves"),LegacyTrial->IsActorBeingDestroyed());
    }
    Director->CancelPendingMilestones();
    auto SetTime=[&](float T)
    { ++GFrameCounter;W->GetTimerManager().Tick(T-Source->GetRunTimeSeconds()+.001f);TestEqual(TEXT("Authoritative run clock advanced"),Source->GetRunTimeSeconds(),T); };
    auto Count=[&](){int32 N=0;for(TActorIterator<ABloodShrine> It(W);It;++It)++N;return N;};
    SetTime(179);Director->ExecuteMilestone(0);TestEqual(TEXT("No early shrine"),Count(),0);
    SetTime(180);Director->ExecuteMilestone(0);TestEqual(TEXT("First shrine at three minutes"),Count(),1);
    SetTime(299);Director->ExecuteMilestone(4);TestEqual(TEXT("Second shrine waits"),Count(),1);
    SetTime(300);Director->ExecuteMilestone(4);TestEqual(TEXT("Second shrine at five minutes"),Count(),2);
    Director->ExecuteMilestone(0);Director->ExecuteMilestone(4);TestEqual(TEXT("Shrine milestones do not duplicate"),Count(),2);
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GI->Shutdown();
    UGameplayStatics::DeleteGameInSlot(Slot,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPreparationVisualTest,"HeavensDivide.UI.RunPreparationAndMinimap",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FRunPreparationVisualTest::RunTest(const FString&)
{
    const FString Slot=TEXT("Automation_RunPreparationVisual_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* W=PreparationWorld(Slot);
    auto* GI=W->GetGameInstance();
    auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    Meta->TestSaveSlot=Slot;Meta->CreateFreshSave();
    auto* Challenge=GI->GetSubsystem<URunChallengeSubsystem>();
    auto* Loadout=GI->GetSubsystem<URunLoadoutSubsystem>();
    auto* Screen=CreateWidget<UStanceSelectionWidget>(GI);
    // A viewport retains this Slate root; keep it alive across setup-page rebuilds too.
    const auto ScreenSlate=Screen->TakeWidget();
    TestTrue(TEXT("Default pair is ready to play"),Screen->CanStartRun());
    TestEqual(TEXT("All six stances remain visible, including locked choices"),Screen->GetVisibleStanceCount(),6);
    TestFalse(TEXT("Locked stance is not selectable"),Screen->SelectStance(TEXT("Iaijutsu")));
    TestFalse(TEXT("Unknown map cannot become selected"),Screen->SelectMap(TEXT("MissingMap")));
    TestEqual(TEXT("Playable map remains selected"),Screen->GetSelectedMap(),FName(TEXT("FallenGrove")));
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("RunPreparation/Previews");
    IFileManager::Get().MakeDirectory(*Dir,true);
    auto Render=[&](UUserWidget* Widget,FVector2D Size,const TCHAR* Name)
    {
        FWidgetRenderer Renderer(false);
        auto Slate=Widget->TakeWidget();
#if WITH_EDITOR
        // Newly loaded synergy/stance artwork can still be compiling on a cold editor start.
        FAssetCompilingManager::Get().FinishAllCompilation();
#endif
        FlushRenderingCommands();
        auto* Target=Renderer.DrawWidget(Slate,Size);
        for(int32 I=0;I<4;++I){Slate->Invalidate(EInvalidateWidgetReason::Prepass);Widget->ForceLayoutPrepass();Renderer.DrawWidget(Target,Slate,Size,1.f/60.f);}
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
        TestTrue(TEXT("Widget render readable"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
        TestTrue(TEXT("Preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Dir/Name)));
        return Pixels;
    };
    Render(Screen,{1600,900},TEXT("NewRunDefault1600.png"));
    Render(Screen,{1280,720},TEXT("NewRunDefault1280.png"));
    Screen->SelectSetupPage(ERunSetupPage::SynergiesAndCombo);
    TestEqual(TEXT("Powers subpage opens on the retained widget"),Screen->GetSetupPage(),ERunSetupPage::SynergiesAndCombo);
    TestEqual(TEXT("Only owned default synergy appears"),Screen->GetVisibleSynergyCount(),1);
    TestTrue(TEXT("Default Tag Team is visibly selected"),Screen->IsSynergySelected(TEXT("TagTeam")));
    TestFalse(TEXT("Locked synergy cannot be selected through the UI"),Screen->SetSynergySelected(TEXT("Handoff"),true));
    TestFalse(TEXT("Locked combo cannot be selected"),Screen->SelectComboAbility(TEXT("BloodMoon")));
    TestFalse(TEXT("Future combo cannot be selected"),Screen->SelectComboAbility(TEXT("Veilbreaker")));
    TestTrue(TEXT("Begin run remains available on powers page"),Screen->CanStartRun());
    Render(Screen,{1600,900},TEXT("NewRunPowersDefault1600.png"));
    Render(Screen,{1280,720},TEXT("NewRunPowersDefault1280.png"));
    // Render the actual popup contents so locked choices and their artwork are reviewable.
    for(int32 Kind=0;Kind<3;++Kind)
    {
        FWidgetRenderer Renderer(false);
        const auto Choices=Screen->MakeLoadoutChoices(Kind);
        const FVector2D Size(460,320);
        auto* Target=Renderer.DrawWidget(Choices,Size);
        FlushRenderingCommands();TArray<FColor> Pixels;
        TestTrue(TEXT("Loadout popup render readable"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(460,320,Pixels,PNG);
        FFileHelper::SaveArrayToFile(PNG,*(Dir/FString::Printf(TEXT("LockedChoices%d.png"),Kind)));
    }
    TestTrue(TEXT("Default synergy can be unequipped"),Screen->SetSynergySelected(TEXT("TagTeam"),false));
    TestTrue(TEXT("Combo can explicitly stay unequipped"),Screen->SelectComboAbility(NAME_None));
    Screen->Open();Screen->SelectSetupPage(ERunSetupPage::SynergiesAndCombo);
    TestTrue(TEXT("An intentionally empty synergy loadout survives reopen"),Loadout->GetSelectedSynergies().IsEmpty());
    TestTrue(TEXT("An explicitly unequipped combo survives reopen"),Loadout->GetSelectedComboAbility().IsNone());
    TestTrue(TEXT("Tag Team can be selected again"),Screen->SetSynergySelected(TEXT("TagTeam"),true));
    Meta->CurrentSave->SoulSigils=100000;
    Meta->CurrentSave->SoulEmbers=100000;
    for(auto* Definition:Meta->GetSynergyUpgradeDefinitions())
        if(!Meta->IsUpgradeMetaEligible(Definition))
            TestTrue(TEXT("Purchase extra synergy for loadout menu"),Meta->PurchaseSynergy(Definition->MetaUnlockId.IsNone()?Definition->UpgradeId:Definition->MetaUnlockId));
    TestTrue(TEXT("Purchase combo deliberately for menu coverage"),Meta->PurchaseSkill(TEXT("Ability.ComboUnlock")));
    for(const auto& Stance:RunStances::All)
        if(!RunStances::IsDefaultUnlocked(Stance.Id)) TestTrue(TEXT("Purchase extra stance for menu"),Meta->PurchaseStance(Stance.Id));
    Screen->Open();
    TestEqual(TEXT("Reopening shows newly purchased stances"),Screen->GetVisibleStanceCount(),6);
    TestTrue(TEXT("Purchased Samurai selectable"),Screen->SelectStance(TEXT("Iaijutsu")));
    TestTrue(TEXT("Purchased Ninja selectable"),Screen->SelectStance(TEXT("GreatShuriken")));
    for(int32 Rank=0;Rank<3;++Rank)
    {TMap<FName,int32> Completed;for(const auto& M:RunChallenges::Modifiers())Completed.Add(M.Id,Rank);Meta->RecordDifficultyVictory(TEXT("FallenGrove"),Completed);}
    TestTrue(TEXT("Menu health challenge editable"),Challenge->SetModifierRank(TEXT("Health"),3));
    TestTrue(TEXT("Menu damage challenge editable"),Challenge->SetModifierRank(TEXT("Damage"),1));
    TestTrue(TEXT("Menu speed challenge editable"),Challenge->SetModifierRank(TEXT("Speed"),2));
    TestTrue(TEXT("Challenge summary uses selected rank total"),FMath::IsNearlyEqual(Challenge->GetRewardMultiplier(),1.48f));
    Render(Screen,{1600,900},TEXT("Stances1600.png"));
    Render(Screen,{1280,720},TEXT("Stances1280.png"));
    Screen->SelectSetupPage(ERunSetupPage::SynergiesAndCombo);
    TestEqual(TEXT("Newly purchased synergies become visible"),Screen->GetVisibleSynergyCount(),Loadout->GetAvailableSynergies().Num());
    for(auto* Definition:Loadout->GetAvailableSynergies())
    {
        const bool bShouldFit=Screen->IsSynergySelected(Definition->UpgradeId)||Loadout->GetSelectedSynergies().Num()<URunLoadoutSubsystem::MaxSelectedSynergies;
        TestEqual(TEXT("Synergy UI enforces the active slot count"),Screen->SetSynergySelected(Definition->UpgradeId,true),bShouldFit);
    }
    TestEqual(TEXT("All available slots can be filled"),Loadout->GetSelectedSynergies().Num(),FMath::Min(URunLoadoutSubsystem::MaxSelectedSynergies,Loadout->GetAvailableSynergies().Num()));
    TestTrue(TEXT("Explicit None survives buying the combo unlock"),Loadout->GetSelectedComboAbility().IsNone());
    TestTrue(TEXT("Owned combo can be selected"),Screen->SelectComboAbility(TEXT("BloodMoon")));
    TestTrue(TEXT("Owned combo can be unequipped"),Screen->SelectComboAbility(NAME_None));
    Screen->SelectSetupPage(ERunSetupPage::ArenaAndStances);
    Screen->SelectSetupPage(ERunSetupPage::SynergiesAndCombo);
    TestTrue(TEXT("Page switches preserve unequipped combo"),Loadout->GetSelectedComboAbility().IsNone());
    TestTrue(TEXT("Owned combo can be reselected"),Screen->SelectComboAbility(TEXT("BloodMoon")));
    TestFalse(TEXT("Future combo remains unavailable after purchases"),Screen->SelectComboAbility(TEXT("CrimsonEclipse")));
    TestEqual(TEXT("Rejected future combo preserves current selection"),Loadout->GetSelectedComboAbility(),FName(TEXT("BloodMoon")));
    TestEqual(TEXT("Samurai choice survives setup page switches"),Screen->GetSelectedSamurai(),FName(TEXT("Iaijutsu")));
    TestEqual(TEXT("Ninja choice survives setup page switches"),Screen->GetSelectedNinja(),FName(TEXT("GreatShuriken")));
    Render(Screen,{1600,900},TEXT("NewRunPowersUnlocked1600.png"));
    Render(Screen,{1280,720},TEXT("NewRunPowersUnlocked1280.png"));
    // Exercise the actual Begin Run handler before ticking travel, preserving the
    // isolated test world while checking the destination and selected loadout.
    Screen->ConfirmRun();
    TestEqual(TEXT("Begin Run travels to Old Village"),GEngine->GetWorldContextFromWorldChecked(W).TravelURL,FString(TEXT("/Game/Maps/Lvl_OldVillage")));
    TestEqual(TEXT("Begin Run applies chosen Samurai stance"),Loadout->GetSamuraiStance(),FName(TEXT("Iaijutsu")));
    TestEqual(TEXT("Begin Run applies chosen Ninja stance"),Loadout->GetNinjaStance(),FName(TEXT("GreatShuriken")));
    GEngine->GetWorldContextFromWorldChecked(W).TravelURL.Empty();
    for (const auto& Entry : TArray<TPair<FName,FString>>{
        {TEXT("MoonlitForest"), TEXT("/Game/Maps/Lvl_MoonlitForest")},
        {TEXT("HollowMarsh"), TEXT("/Game/Maps/Lvl_SunkenSanctuary")},
        {TEXT("CinderCitadel"), TEXT("/Game/Maps/Lvl_EmberQuarry")}})
    {
        Screen->bStarting = false;
        TestTrue(TEXT("New destination can be selected"),Screen->SelectMap(Entry.Key));
        Screen->ConfirmRun();
        TestEqual(TEXT("Begin Run uses selected destination"),GEngine->GetWorldContextFromWorldChecked(W).TravelURL,Entry.Value);
        GEngine->GetWorldContextFromWorldChecked(W).TravelURL.Empty();
    }
    auto* PC=W->SpawnActor<ASurvivorPlayerController>(PreparationControllerClass());
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* Bounds=W->SpawnActor<AMinimapBounds>();
    Bounds->PlayableArea->SetBoxExtent(FVector(5000,5000,100));
    auto* Registry=W->GetSubsystem<UMinimapRegistrySubsystem>();Registry->SetBounds(Bounds);
    auto* Player=W->SpawnActor<ACharacterBase>();
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Player);
    auto* Shrine=W->SpawnActor<AActor>(FVector(2400,1900,0),FRotator::ZeroRotator);
    auto* Marker=NewObject<UMinimapMarkerComponent>(Shrine);Shrine->AddInstanceComponent(Marker);Marker->RegisterComponent();Registry->RegisterMarker(Marker);
    auto* Map=CreateWidget<UMinimapWidget>(PC);Map->InitializeMinimap(PC);
    auto* Heal=W->SpawnActor<AHealingPickup>(FVector(-1900,2400,0),FRotator::ZeroRotator);
    auto* Magnet=W->SpawnActor<AMagnetPickup>(FVector(1800,1200,0),FRotator::ZeroRotator);
    Registry->RegisterMarker(Heal->FindComponentByClass<UMinimapMarkerComponent>());
    Registry->RegisterMarker(Magnet->FindComponentByClass<UMinimapMarkerComponent>());
    const auto Before=Render(Map,{250,250},TEXT("MinimapBefore.png"));
    Player->SetActorLocation(FVector(1800,-1600,0));Player->SetActorRotation(FRotator(0,90,0));
    const auto After=Render(Map,{250,250},TEXT("MinimapAfter.png"));
    int32 Changed=0;for(int32 I=0;I<FMath::Min(Before.Num(),After.Num());++I)if(Before[I]!=After[I])++Changed;
    TestTrue(TEXT("Player movement repaints minimap"),Changed>50);
    Marker->SetMarkerState(EMinimapMarkerState::Completed);TestFalse(TEXT("Completed objective disappears"),Marker->IsMarkerVisible());
    Registry->UnregisterMarker(Marker);TestEqual(TEXT("Only pickup markers remain"),Registry->GetMarkers().Num(),2);
    Registry->UnregisterMarker(Heal->FindComponentByClass<UMinimapMarkerComponent>());
    Registry->UnregisterMarker(Magnet->FindComponentByClass<UMinimapMarkerComponent>());
    TestTrue(TEXT("Removed markers unregister"),Registry->GetMarkers().IsEmpty());
    Screen->ReleaseSlateResources(true);Map->ReleaseSlateResources(true);
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GI->Shutdown();
    UGameplayStatics::DeleteGameInSlot(Slot,0);
    return true;
}
#endif
