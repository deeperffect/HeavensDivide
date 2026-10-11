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
        TestEqual(TEXT("Only default synergy is offered before synergy purchases"),Synergies.Num(),1);
        if(!Synergies.IsEmpty())TestEqual(TEXT("Default synergy is Tag Team"),Synergies[0]->UpgradeId,FName(TEXT("TagTeam")));
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
    auto* Screen=CreateWidget<UStanceSelectionWidget>(GI);
    Screen->TakeWidget();
    TestTrue(TEXT("Default pair is ready to play"),Screen->CanStartRun());
    TestEqual(TEXT("Only the two default stances are displayed"),Screen->GetVisibleStanceCount(),2);
    TestFalse(TEXT("Locked stance is not selectable"),Screen->SelectStance(TEXT("Iaijutsu")));
    TestFalse(TEXT("Placeholder cannot become selected"),Screen->SelectMap(TEXT("CinderCitadel")));
    TestEqual(TEXT("Playable map remains selected"),Screen->GetSelectedMap(),FName(TEXT("FallenGrove")));
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("RunPreparation/Previews");
    IFileManager::Get().MakeDirectory(*Dir,true);
    auto Render=[&](UUserWidget* Widget,FVector2D Size,const TCHAR* Name)
    {
        FWidgetRenderer Renderer(false);
        auto Slate=Widget->TakeWidget();
        auto* Target=Renderer.DrawWidget(Slate,Size);
        for(int32 I=0;I<4;++I){Slate->Invalidate(EInvalidateWidgetReason::Prepass);Widget->ForceLayoutPrepass();Renderer.DrawWidget(Target,Slate,Size,1.f/60.f);}
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
        TestTrue(TEXT("Widget render readable"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
        TestTrue(TEXT("Preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Dir/Name)));
        return Pixels;
    };
    Render(Screen,{1600,900},TEXT("NewRunDefault1600.png"));
    Render(Screen,{1280,720},TEXT("NewRunDefault1280.png"));
    Meta->CurrentSave->SoulSigils=1000;
    for(const auto& Stance:RunStances::All)
        if(!RunStances::IsDefaultUnlocked(Stance.Id)) TestTrue(TEXT("Purchase extra stance for menu"),Meta->PurchaseStance(Stance.Id));
    Screen->Open();
    TestEqual(TEXT("Reopening shows newly purchased stances"),Screen->GetVisibleStanceCount(),6);
    TestTrue(TEXT("Purchased Samurai selectable"),Screen->SelectStance(TEXT("Iaijutsu")));
    TestTrue(TEXT("Purchased Ninja selectable"),Screen->SelectStance(TEXT("GreatShuriken")));
    TestTrue(TEXT("Menu health challenge editable"),Challenge->SetModifierRank(TEXT("Health"),3));
    TestTrue(TEXT("Menu damage challenge editable"),Challenge->SetModifierRank(TEXT("Damage"),1));
    TestTrue(TEXT("Menu speed challenge editable"),Challenge->SetModifierRank(TEXT("Speed"),2));
    TestTrue(TEXT("Challenge summary uses selected rank total"),FMath::IsNearlyEqual(Challenge->GetRewardMultiplier(),1.48f));
    Render(Screen,{1600,900},TEXT("Stances1600.png"));
    Render(Screen,{1280,720},TEXT("Stances1280.png"));
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
