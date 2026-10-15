#include "SurvivalFieldObjective.h"
#include "RunReportSubsystem.h"
#include "EncounterWall.h"
#include "EnemySpawner.h"
#include "SurvivorPlayerController.h"
#include "ExperienceComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "RunChallengeSubsystem.h"
#include "MinimapMarkerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "CombatAudio.h"
#include "UObject/ConstructorHelpers.h"
#include "CombatGround.h"
#include "ObjectiveInteractionComponent.h"
#include "ObjectiveGuidanceSubsystem.h"

ASurvivalFieldObjective::ASurvivalFieldObjective()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Ring=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Circle")); Ring->SetupAttachment(RootComponent);
    Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision); Ring->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/Materials/M_EnemyRoleRing"));
    Ring->SetStaticMesh(Plane.Object); Ring->SetMaterial(0,Material.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HoldMaterial(TEXT("/Game/HeavensDivide/Materials/M_EmberCircleProgress.M_EmberCircleProgress"));
    HoldRingMaterial=HoldMaterial.Object;
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Instructions")); Label->SetupAttachment(RootComponent);
    Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(38); Label->SetRelativeLocation(FVector(0,0,220));
    Label->SetTextRenderColor(FColor(255,225,160));
    Label->SetVisibility(false);
    ActivationMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ActivationMesh"));
    ActivationMesh->SetupAttachment(RootComponent);
    ActivationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shrine(TEXT("/Game/Assets/Environment/Objectives/BloodShrine.BloodShrine"));
    ActivationMesh->SetStaticMesh(Shrine.Object);
    ActivationMesh->SetRelativeScale3D(FVector(4.f)); // Match the saved Blood Shrine's presentation.
    ObjectiveInteraction=CreateDefaultSubobject<UObjectiveInteractionComponent>(TEXT("ObjectiveInteraction"));
    ObjectiveInteraction->SetupAttachment(RootComponent);
    ObjectiveInteraction->ConfigureDefaults(FText::FromString(TEXT("Ember Circle")),300.f,1.f,240.f);
    ObjectiveInteraction->SetPresentationVisual(ActivationMesh);
    ActivationMesh->SetHiddenInGame(true);
    Marker=CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("Minimap"));
}
bool ASurvivalFieldObjective::InitializeWallTrap(AEnemySpawner* Spawner)
{
    if (!Spawner) return false;
    bWallTrap=true;
    Ring->SetVisibility(false);
    ActivationMesh->SetHiddenInGame(true);
    ObjectiveInteraction->HidePrompt();
    TArray<FVector> Positions;
    const int32 Count=FMath::Clamp(FMath::CeilToInt(2*PI*Radius/200.f),16,64);
    for (int32 I=0; I<Count; ++I)
    {
        const float A=2*PI*I/Count; FVector Position;
        if (!Spawner->ValidateEncounterLocation(GetActorLocation()+FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius,AEncounterWall::StaticClass(),Position)) return false;
        if (!CombatGround::IsInsideArena(GetWorld(),Position,140.f)) return false;
        Positions.Add(Position);
    }
    const int32 RockOffset=FMath::RandRange(0,10000);
    for (int32 I=0; I<Count; ++I)
    {
        FActorSpawnParameters Params; Params.Owner=this;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Wall=GetWorld()->SpawnActor<AEncounterWall>(Positions[I],FRotator(0,I*360.f/Count,0),Params);
        if (!Wall) { for (auto Existing:Walls) Existing->Destroy(); Walls.Reset(); return false; }
        Wall->ConfigureRock(RockOffset+I);
        Walls.Add(Wall);
    }
    Marker->DisplayName=FText::FromString(TEXT("Break the seal"));
    Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));
    RingMaterial=Ring->CreateDynamicMaterialInstance(0);
    if (RingMaterial) RingMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.3f,.08f));
    UCombatAudioLibrary::PlayEvent(this,TEXT("BossWarning"),GetActorLocation(),false,.65f);
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    // Reserve the objective while the ring rises, without pausing its reveal.
    if(PC) PC->TryBeginObjective(this);
    return true;
}
void ASurvivalFieldObjective::BeginWall()
{
    bIntroPending=false;
    Age=0;
    // Reset the survival clock without restarting the completed reveal wave.
    WallWaveStartAge=-(WallWaveSeconds+WallRiseSeconds);
    // The ambient wall event only reserves the objective slot during its introduction.
    if(auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0))) PC->EndObjective(this);
}
void ASurvivalFieldObjective::InitializeHoldZone()
{
    Radius=HoldZoneRadius;
    bHoldInitialized=true;
    Ring->SetVisibility(false);
    ActivationMesh->SetHiddenInGame(false);
    Marker->DisplayName=FText::FromString(TEXT("Ember circle"));
    Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));
    Ring->SetMaterial(0,HoldRingMaterial);
    RingMaterial=Ring->CreateDynamicMaterialInstance(0);
    UpdateHoldPresentation();
    Label->SetText(FText::GetEmpty());
    Label->SetVisibility(false);
    if(auto* Guidance=GetWorld()->GetSubsystem<UObjectiveGuidanceSubsystem>()) Guidance->Announce(this);
}
bool ASurvivalFieldObjective::CanInteract_Implementation(APawn* InteractingPawn) const
{
    const auto* PC=InteractingPawn?Cast<ASurvivorPlayerController>(InteractingPawn->GetController()):nullptr;
    return bHoldInitialized && !bWallTrap && !bHoldActive && !bComplete && PC
        && PC->IsRunInProgress() && !PC->IsPlayerDead() && !PC->IsSelectingUpgrade()
        && !UGameplayStatics::IsGamePaused(this) && !PC->IsAnyObjectiveActive()
        && ObjectiveInteraction && ObjectiveInteraction->IsInRange(InteractingPawn);
}
void ASurvivalFieldObjective::Interact_Implementation(APawn* InteractingPawn)
{
    if (!CanInteract_Implementation(InteractingPawn)) return;
    auto* PC=Cast<ASurvivorPlayerController>(InteractingPawn->GetController());
    if (!PC->TryBeginObjective(this)) return;
    bHoldActive=true;
    Ring->SetVisibility(true);
    bIntroPending=true;
    Age=0;
    Marker->SetMarkerState(EMinimapMarkerState::Active);
    ObjectiveInteraction->HidePrompt();
    const FText Message=FText::Format(NSLOCTEXT("Objectives","SurvivalIntro","Survive inside for {0} seconds"),
        FText::AsNumber(FMath::RoundToInt(HoldSeconds)));
    if (!PC->ShowObjectiveIntro(this,Message,FLinearColor(.25f,.72f,.53f),FSimpleDelegate::CreateUObject(this,&ASurvivalFieldObjective::BeginHold))) BeginHold();
}
void ASurvivalFieldObjective::BeginHold()
{
    bIntroPending=false;
}
bool ASurvivalFieldObjective::ContainsPlayer(const FVector& Point) const
{
    return FMath::Abs(Point.Z-GetActorLocation().Z)<240 && FVector::DistSquared2D(Point,GetActorLocation())<=FMath::Square(Radius);
}
void ASurvivalFieldObjective::AdvanceHold(float Delta, bool bInside)
{
    if (bWallTrap || !bHoldActive || bIntroPending || bComplete || !FMath::IsFinite(Delta) || Delta<=0
        || UGameplayStatics::IsGamePaused(this) || UGameplayStatics::GetGlobalTimeDilation(this)<.01f) return;
    Progress=FMath::Clamp(Progress+(bInside?Delta:-Delta),0.f,HoldSeconds);
    UpdateHoldPresentation();
    if (Progress>=HoldSeconds) CompleteHold();
}
void ASurvivalFieldObjective::UpdateHoldPresentation()
{
    if (RingMaterial) RingMaterial->SetScalarParameterValue(TEXT("Progress"),FMath::Clamp(Progress/FMath::Max(HoldSeconds,.01f),0.f,1.f));
}
void ASurvivalFieldObjective::CompleteHold()
{
    if (bComplete) return;
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || PC->IsPlayerDead() || !PC->GetExperienceComponent()) return;
    bComplete=true; // Commit before AddXP can synchronously open an upgrade menu.
    PC->EndObjective(this);
    Marker->SetMarkerState(EMinimapMarkerState::Completed);
    RunReports::ObjectiveCompleted(this);
    RunChallenges::AwardCurrencies(this,EmberReward,1);
    const int32 XP=2*PC->GetExperienceComponent()->GetXPToNextLevel();
    PC->GetExperienceComponent()->AddXP(XP);
    UCombatAudioLibrary::PlayEvent(this,TEXT("UrnBreak"),GetActorLocation(),false,.7f);
    Destroy();
}
void ASurvivalFieldObjective::Tick(float Delta)
{
    Super::Tick(Delta);
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || !PC->GetPawn() || PC->IsPlayerDead()) { Destroy(); return; }
    // The intro deliberately freezes a fully visible, solid ring.
    if (bIntroPending || UGameplayStatics::IsGamePaused(this) || UGameplayStatics::GetGlobalTimeDilation(this)<.01f) return;
    // Neither a menu's near-zero time dilation nor another objective spends progress/lifetime.
    if (PC->IsAnyObjectiveActive() && PC->GetActiveObjective()!=this)
    {
        if (bWallTrap) for (auto Wall:Walls) if (IsValid(Wall)) Wall->SetSealed(false);
        bSealed=false;
        WallWaveStartAge=-1.f;
        return;
    }
    if (bComplete || bIntroPending) return;
    Age+=Delta;
    if (bWallTrap)
    {
        if (!bSealed)
        {
            // Do not raise a wall through a pawn who used the warning to move away.
            bool bOccupied=false;
            for (auto Wall:Walls) if (IsValid(Wall) && !Wall->IsDead())
                bOccupied |= Wall->Barrier->Bounds.GetBox().ExpandBy(60).IsInside(PC->GetPawn()->GetActorLocation());
            if (!bOccupied)
            {
                bSealed=true;
                WallWaveStartAge=Age;
            }
        }
        if (bSealed)
        {
            bool bAllRaised=true;
            for (int32 I=0;I<Walls.Num();++I)
            {
                AEncounterWall* Wall=Walls[I];
                if (!IsValid(Wall) || Wall->IsDead()) continue;
                // The player can reach a later segment while the wave is rising.
                if (Wall->Barrier->GetCollisionEnabled()==ECollisionEnabled::NoCollision
                    && Wall->Barrier->Bounds.GetBox().ExpandBy(60).IsInside(PC->GetPawn()->GetActorLocation())) { bAllRaised=false;continue; }
                const float Delay=WallWaveSeconds*I/FMath::Max(1,Walls.Num()-1);
                const float Reveal=(Age-WallWaveStartAge-Delay)/FMath::Max(.05f,WallRiseSeconds);
                Wall->SetRevealProgress(Reveal);
                bAllRaised &= Reveal>=1.f;
            }
            if(bAllRaised && !bWallIntroShown)
            {
                bWallIntroShown=true;
                if(PC->GetActiveObjective()==this || PC->TryBeginObjective(this))
                {
                    bIntroPending=true;
                    if(!PC->ShowObjectiveIntro(this,NSLOCTEXT("Objectives","WallIntro","Survive or break out.. if you can.."),FLinearColor(.85f,.42f,.16f),
                        FSimpleDelegate::CreateUObject(this,&ASurvivalFieldObjective::BeginWall))) BeginWall();
                }
                else BeginWall();
            }
        }
        if (!bIntroPending && Age>=FMath::Max(1.f,WallLifetime)) Destroy();
    }
    else
    {
        const bool bInside=ContainsPlayer(PC->GetPawn()->GetActorLocation());
        AdvanceHold(Delta,bInside);
        if (!bComplete)
        {
            Marker->SetMarkerState(bHoldActive?EMinimapMarkerState::Active:EMinimapMarkerState::Available);
            if (Age>105) Destroy();
        }
    }
}
void ASurvivalFieldObjective::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0))) PC->EndObjective(this);
    if (ObjectiveInteraction) ObjectiveInteraction->HidePrompt();
    for (auto Wall:Walls) if (IsValid(Wall)) Wall->Destroy();
    Super::EndPlay(Reason);
}
