#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ObjectiveIntroWidget.h"
#include "ObjectiveGuidanceSubsystem.h"
#include "ObjectiveGuidanceWidget.h"
#include "BossToriiGate.h"
#include "NiagaraComponent.h"
#include "ObjectiveInteractionComponent.h"
#include "MinimapMarkerComponent.h"
#include "SurvivalFieldObjective.h"
#include "BloodShrine.h"
#include "BloodShrineWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "EncounterWall.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveIntroTest,"HeavensDivide.Encounters.ObjectiveIntro",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FObjectiveIntroTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->SetGameMode(FURL());World->InitializeActorsForPlay(FURL());
    auto* PC=World->SpawnActor<ASurvivorPlayerController>();
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->PlayerState=World->SpawnActor<APlayerState>();
    auto* Pawn=World->SpawnActor<ACharacterBase>();PC->Possess(Pawn);
    auto* Zone=World->SpawnActor<ASurvivalFieldObjective>();Zone->InitializeHoldZone();
    auto* Guidance=World->GetSubsystem<UObjectiveGuidanceSubsystem>();
    TestEqual(TEXT("Circle spawn adds guidance"),Guidance->GetTrackedCount(),1);
    Guidance->Announce(Zone);TestEqual(TEXT("Repeated spawn notification is deduplicated"),Guidance->GetTrackedCount(),1);
    TestFalse(TEXT("Spawn notification does not pause gameplay"),UGameplayStatics::IsGamePaused(World));
    FVector ArrowTarget;
    TestTrue(TEXT("Arrow guides distant players to the new circle"),Guidance->FindGuidanceTarget(Zone,FVector(2000,0,0),ArrowTarget));
    TestFalse(TEXT("Arrow clears at interaction distance"),Guidance->FindGuidanceTarget(Zone,FVector(100,0,0),ArrowTarget));
    Guidance->AdvanceLifetimes(9.99f);
    TestEqual(TEXT("Spawn arrow remains for the first ten seconds"),Guidance->GetTrackedCount(),1);
    Guidance->AdvanceLifetimes(.02f);
    TestEqual(TEXT("Spawn arrow expires after ten seconds"),Guidance->GetTrackedCount(),0);
    Guidance->Announce(Zone);
    TestEqual(TEXT("Duplicate announcements cannot restart an expired arrow"),Guidance->GetTrackedCount(),0);
    TestEqual(TEXT("Circle uses 1.5 times the original radius"),Zone->Radius,1200.f);
    TestNotNull(TEXT("Circle has a real activation mesh"),Zone->ActivationMesh->GetStaticMesh().Get());
    TestFalse(TEXT("Circle has no world instruction label"),Zone->FindComponentByClass<UTextRenderComponent>()->IsVisible());
    Pawn->SetActorLocation(FVector(500,0,0));
    TestFalse(TEXT("Being inside is not enough to interact with the central mesh"),Zone->CanInteract_Implementation(Pawn));
    Pawn->SetActorLocation(FVector::ZeroVector);
    Zone->AdvanceHold(20,true);TestEqual(TEXT("Inactive circle never progresses"),Zone->GetProgress(),0.f);
    World->GetWorldSettings()->SetTimeDilation(.7f);
    Zone->Interact_Implementation(Pawn);
    TestTrue(TEXT("Circle intro freezes the world"),UGameplayStatics::IsGamePaused(World));
    TestTrue(TEXT("Circle owns the objective during the intro"),PC->GetActiveObjective()==Zone);
    TestTrue(TEXT("Circle intro is visible"),PC->IsObjectiveIntroPlaying());
    Zone->AdvanceHold(20,true);TestEqual(TEXT("Intro cannot spend survival time"),Zone->GetProgress(),0.f);
    PC->TogglePauseMenu();TestFalse(TEXT("Escape cannot stack a pause menu over the intro"),PC->IsPauseMenuOpen());
    PC->UpdateObjectiveIntro(UObjectiveIntroWidget::Duration-.01f);
    TestTrue(TEXT("World stays frozen through the outgoing smoke"),UGameplayStatics::IsGamePaused(World));
    PC->UpdateObjectiveIntro(.02f);
    TestFalse(TEXT("Completed intro resumes the world"),UGameplayStatics::IsGamePaused(World));
    TestEqual(TEXT("Intro preserves preexisting time dilation"),World->GetWorldSettings()->TimeDilation,.7f);
    Zone->AdvanceHold(10,true);
    TestEqual(TEXT("Ring is half yellow after ten seconds"),Zone->RingMaterial->K2_GetScalarParameterValue(TEXT("Progress")),.5f);
    Zone->AdvanceHold(4,false);
    TestEqual(TEXT("Time outside drains accumulated survival progress"),Zone->GetProgress(),6.f);
    TestEqual(TEXT("Yellow ring retreats with drained progress"),Zone->RingMaterial->K2_GetScalarParameterValue(TEXT("Progress")),.3f);
    TestTrue(TEXT("Controller can pause the synthetic world without a viewport"),PC->SetPause(true));
    TestTrue(TEXT("Drain check runs while the world is actually paused"),UGameplayStatics::IsGamePaused(World));
    Zone->AdvanceHold(4,false);
    TestEqual(TEXT("Paused world cannot drain circle progress"),Zone->GetProgress(),6.f);
    PC->SetPause(false);
    Zone->AdvanceHold(7,false);TestEqual(TEXT("Leaving long enough returns to blue at zero"),Zone->GetProgress(),0.f);
    TestFalse(TEXT("Active circle does not point inward while already inside"),Guidance->FindGuidanceTarget(Zone,FVector(800,0,0),ArrowTarget));
    TestTrue(TEXT("Active circle guides the player back after leaving"),Guidance->FindGuidanceTarget(Zone,FVector(1500,0,0),ArrowTarget));
    PC->EndObjective(Zone);Zone->Destroy();

    auto* Spawner=World->SpawnActor<AEnemySpawner>();
    auto* ShrineClass=LoadClass<ABloodShrine>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/Objectives/BP_BloodShrine.BP_BloodShrine_C"));
    TestNotNull(TEXT("Saved Blood Shrine Blueprint loads"),ShrineClass);
    auto* Shrine=World->SpawnActor<ABloodShrine>(ShrineClass?ShrineClass:ABloodShrine::StaticClass());Shrine->EnemySpawner=Spawner;
    TestTrue(TEXT("Blood Shrine interaction starts"),Shrine->ActivateShrine(Pawn));
    TestTrue(TEXT("Blood Shrine shares the true pause"),UGameplayStatics::IsGamePaused(World));
    TestEqual(TEXT("Shrine shows the full 30 seconds during the intro"),Shrine->GetTimeRemaining(),30.f);
    TestFalse(TEXT("Challenge timer waits for the intro"),World->GetTimerManager().IsTimerActive(Shrine->ChallengeUpdateTimer));
    PC->UpdateObjectiveIntro(UObjectiveIntroWidget::Duration);
    TestFalse(TEXT("Shrine resumes after the dissolve"),UGameplayStatics::IsGamePaused(World));
    TestEqual(TEXT("Shrine starts with all 30 seconds remaining"),Shrine->GetTimeRemaining(),30.f);
    TestTrue(TEXT("Shrine challenge timer starts after the intro"),World->GetTimerManager().IsTimerActive(Shrine->ChallengeUpdateTimer));
    auto* Enemy=World->SpawnActor<AEnemyBase>();
    Shrine->HandleEnemyKilled(Enemy);TestEqual(TEXT("Ordinary enemies do not count"),Shrine->GetCurrentBlood(),0);
    Enemy->MakeBloodbound(1,1,1,true);
    FindFProperty<FIntProperty>(AEnemyBase::StaticClass(),TEXT("BloodValue"))->SetPropertyValue_InContainer(Enemy,5);
    Shrine->HandleEnemyKilled(Enemy);TestEqual(TEXT("One Bloodbound kill counts once regardless of legacy blood value"),Shrine->GetCurrentBlood(),1);
    Shrine->EndChallenge();Shrine->Destroy();

    auto* Floor=World->SpawnActor<AActor>();auto* Ground=NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Ground);Ground->SetBoxExtent(FVector(5000,5000,10));Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);Ground->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-10));
    auto* Trap=World->SpawnActor<ASurvivalFieldObjective>(FVector(0,0,5),FRotator::ZeroRotator);
    TestTrue(TEXT("Wall event spawns a complete ring"),Trap->InitializeWallTrap(Spawner));
    TestFalse(TEXT("Wall ring rises before the intro pauses the world"),PC->IsObjectiveIntroPlaying() || UGameplayStatics::IsGamePaused(World));
    Trap->Tick(.01f);Trap->Tick(Trap->WallWaveSeconds+Trap->WallRiseSeconds+.01f);
    TestTrue(TEXT("Wall intro appears only after the reveal finishes"),PC->IsObjectiveIntroPlaying() && UGameplayStatics::IsGamePaused(World));
    for(auto Wall:Trap->Walls) TestTrue(TEXT("All walls are visible and solid behind the intro"),Wall->Stone->IsVisible() && Wall->Barrier->IsCollisionEnabled());
    const float RevealAge=Trap->Age;
    Trap->Tick(10.f);TestEqual(TEXT("Wall lifetime cannot elapse during its intro"),Trap->Age,RevealAge);
    PC->UpdateObjectiveIntro(UObjectiveIntroWidget::Duration);
    TestFalse(TEXT("Wall intro unpauses automatically"),UGameplayStatics::IsGamePaused(World));
    TestFalse(TEXT("Wall event never displays a guidance arrow"),Guidance->FindGuidanceTarget(Trap,FVector::ZeroVector,ArrowTarget));
    TestFalse(TEXT("Wall arrow disappears after escaping"),Guidance->FindGuidanceTarget(Trap,FVector(2000,0,0),ArrowTarget));
    Trap->DispatchBeginPlay();Trap->Tick(10.f);
    TestFalse(TEXT("Wall lifetime begins after its intro"),Trap->IsActorBeingDestroyed());
    Trap->Tick(10.1f);TestTrue(TEXT("Wall event expires after full gameplay duration"),Trap->IsActorBeingDestroyed());
    TestFalse(TEXT("Wall expiry releases its objective lock"),PC->IsAnyObjectiveActive());

    auto* Owner=World->SpawnActor<AActor>();PC->TryBeginObjective(Owner);
    bool bFinished=false;
    PC->ShowObjectiveIntro(Owner,FText::FromString(TEXT("Survive inside for 20 seconds")),FLinearColor::White,
        FSimpleDelegate::CreateLambda([&bFinished]{bFinished=true;}));
    Owner->Destroy();PC->UpdateObjectiveIntro(.1f);
    TestFalse(TEXT("Deleting an objective releases its intro pause"),UGameplayStatics::IsGamePaused(World));
    TestFalse(TEXT("Cancelled intro never starts a deleted objective"),bFinished);
    TestFalse(TEXT("Cancelled intro removes the overlay"),PC->IsObjectiveIntroPlaying());

    TestEqual(TEXT("Text starts invisible"),UObjectiveIntroWidget::LetterOpacity(0,.5f),0.f);
    TestTrue(TEXT("Text reveals from left to right"),UObjectiveIntroWidget::LetterOpacity(.6f,.1f)>UObjectiveIntroWidget::LetterOpacity(.6f,.9f));
    TestEqual(TEXT("Entire line stays readable during hold"),UObjectiveIntroWidget::LetterOpacity(2.f,.99f),1.f);
    TestTrue(TEXT("Text dissolves from left to right"),UObjectiveIntroWidget::LetterOpacity(3.75f,.1f)<UObjectiveIntroWidget::LetterOpacity(3.75f,.9f));
    TestEqual(TEXT("Text is gone before resuming"),UObjectiveIntroWidget::LetterOpacity(UObjectiveIntroWidget::Duration,1.f),0.f);

    // Portal alignment must use the authored pose, not the temporarily lowered
    // mesh during the interaction component's emergence animation.
    auto* Gate=World->SpawnActor<ABossToriiGate>();
    auto* GateMesh=Gate->FindComponentByClass<UStaticMeshComponent>();
    auto* Portal=Gate->FindComponentByClass<UNiagaraComponent>();
    auto* GateInteraction=Gate->FindComponentByClass<UObjectiveInteractionComponent>();
    GateMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    GateMesh->SetRelativeScale3D(FVector(4,1,8));GateMesh->SetRelativeLocation(FVector(0,0,400));
    Gate->DispatchBeginPlay();
    TestTrue(TEXT("Portal follows the gate during its rise"),Portal->GetAttachParent()==GateMesh);
    GateInteraction->TickComponent(1.f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Portal remains centered above ground after emergence"),FMath::IsNearlyEqual(Portal->GetComponentLocation().Z,304.,1.));
    Gate->Destroy();
    PC->CancelObjectiveIntro();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveIntroVisualTest,"HeavensDivide.UI.ObjectiveIntroVisuals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FObjectiveIntroVisualTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));World->InitializeActorsForPlay(FURL());
    auto* PC=World->SpawnActor<ASurvivorPlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* Widget=CreateWidget<UObjectiveIntroWidget>(PC);FWidgetRenderer Renderer(false);
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("ObjectiveIntro/Previews");
    IFileManager::Get().MakeDirectory(*Directory,true);
    for (int32 Objective=0;Objective<3;++Objective)
    {
        Widget->Present(FText::FromString(Objective==0?TEXT("You've enraged the spirits.\nKill 30 within 30 seconds"):Objective==1?TEXT("Survive inside for 20 seconds"):TEXT("Survive or break out.. if you can..")),
            Objective==0?FLinearColor(.75f,.2f,.15f):Objective==1?FLinearColor(.25f,.72f,.53f):FLinearColor(.85f,.42f,.16f));
        const auto Slate=Widget->TakeWidget();
        const float Times[]={.3f,.7f,2.f,3.65f,4.05f};
        for(int32 Frame=0;Frame<UE_ARRAY_COUNT(Times);++Frame)
        {
            Widget->SetElapsed(Times[Frame]);Widget->ForceLayoutPrepass();
            auto* Target=Renderer.DrawWidget(Slate,{1440,900});
            for(int32 Pass=0;Pass<3;++Pass) { Widget->ForceLayoutPrepass();Renderer.DrawWidget(Target,Slate,{1440,900},.016f); }
            TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
            if(TestTrue(TEXT("Intro preview can be read"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))
            {
                TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1440,900,Pixels,PNG);
                TestTrue(TEXT("Intro preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("%s_%d.png"),Objective==0?TEXT("BloodShrine"):Objective==1?TEXT("Circle"):TEXT("Walls"),Frame))));
            }
        }
    }
    auto* Guidance=CreateWidget<UObjectiveGuidanceWidget>(PC);
    Guidance->Present({{FVector2D(720,450),FVector2D(1,0),FLinearColor(1,.45f,.38f)},
        {FVector2D(720,450),FVector2D(0,-1),FLinearColor(.35f,1,.7f)}},FText::FromString(TEXT("Ember Circle")),1.f);
    const auto Slate=Guidance->TakeWidget();auto* Target=Renderer.DrawWidget(Slate,{1440,900});
    for(int32 Pass=0;Pass<3;++Pass)
    {
        ++GFrameCounter;Guidance->ForceLayoutPrepass();Renderer.DrawWidget(Target,Slate,{1440,900},.016f);
        FlushRenderingCommands();
    }
    TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
    if(TestTrue(TEXT("Spawn popup and arrows render"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))
    {
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1440,900,Pixels,PNG);
        FFileHelper::SaveArrayToFile(PNG,*(Directory/TEXT("SpawnGuidance.png")));
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
#endif
