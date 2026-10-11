#include "SurvivalFieldObjective.h"
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

ASurvivalFieldObjective::ASurvivalFieldObjective()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Ring=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Circle")); Ring->SetupAttachment(RootComponent);
    Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision); Ring->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/Materials/M_EnemyRoleRing"));
    Ring->SetStaticMesh(Plane.Object); Ring->SetMaterial(0,Material.Object);
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Instructions")); Label->SetupAttachment(RootComponent);
    Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(38); Label->SetRelativeLocation(FVector(0,0,220));
    Label->SetTextRenderColor(FColor(255,225,160));
    Marker=CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("Minimap"));
}
void ASurvivalFieldObjective::UpdateLabel(const FString& Text)
{
    Label->SetText(FText::FromString(Text));
}
bool ASurvivalFieldObjective::InitializeWallTrap(AEnemySpawner* Spawner)
{
    if (!Spawner) return false;
    bWallTrap=true;
    TArray<FVector> Positions;
    const int32 Count=FMath::Clamp(FMath::CeilToInt(2*PI*Radius/200.f),16,64);
    for (int32 I=0; I<Count; ++I)
    {
        const float A=2*PI*I/Count; FVector Position;
        if (!Spawner->ValidateEncounterLocation(GetActorLocation()+FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius,AEncounterWall::StaticClass(),Position)) return false;
        if (!CombatGround::IsInsideArena(GetWorld(),Position,140.f)) return false;
        Positions.Add(Position);
    }
    for (int32 I=0; I<Count; ++I)
    {
        FActorSpawnParameters Params; Params.Owner=this;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Wall=GetWorld()->SpawnActor<AEncounterWall>(Positions[I],FRotator(0,I*360.f/Count,0),Params);
        if (!Wall) { for (auto Existing:Walls) Existing->Destroy(); Walls.Reset(); return false; }
        Walls.Add(Wall);
    }
    Marker->DisplayName=FText::FromString(TEXT("Break the seal"));
    Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));
    RingMaterial=Ring->CreateDynamicMaterialInstance(0);
    if (RingMaterial) RingMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,.3f,.08f));
    UpdateLabel(TEXT("WALLS RISING\nBreak a stone seal to escape"));
    UCombatAudioLibrary::PlayEvent(this,TEXT("BossWarning"),GetActorLocation(),false,.65f);
    return true;
}
void ASurvivalFieldObjective::InitializeHoldZone()
{
    Radius=800;
    Marker->DisplayName=FText::FromString(TEXT("Ember circle"));
    Ring->SetWorldScale3D(FVector(Radius/50,Radius/50,1));
    RingMaterial=Ring->CreateDynamicMaterialInstance(0);
    if (RingMaterial) RingMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.1f,.85f,.55f));
    UpdateLabel(TEXT("EMBER CIRCLE\nHold inside for 20s\nLarge XP cache / +20 Embers / +1 Sigil"));
}
bool ASurvivalFieldObjective::ContainsPlayer(const FVector& Point) const
{
    return FMath::Abs(Point.Z-GetActorLocation().Z)<240 && FVector::DistSquared2D(Point,GetActorLocation())<=FMath::Square(Radius);
}
void ASurvivalFieldObjective::AdvanceHold(float Delta, bool bInside)
{
    if (bWallTrap || bComplete || !bInside || !FMath::IsFinite(Delta) || Delta<=0) return;
    Progress=FMath::Min(HoldSeconds,Progress+Delta);
    if (Progress>=HoldSeconds) CompleteHold();
}
void ASurvivalFieldObjective::CompleteHold()
{
    if (bComplete) return;
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || PC->IsPlayerDead() || !PC->GetExperienceComponent()) return;
    bComplete=true; // Commit before AddXP can synchronously open an upgrade menu.
    Marker->SetMarkerState(EMinimapMarkerState::Completed);
    RunChallenges::AwardCurrencies(this,EmberReward,1);
    const int32 XP=2*PC->GetExperienceComponent()->GetXPToNextLevel();
    PC->GetExperienceComponent()->AddXP(XP);
    UpdateLabel(FString::Printf(TEXT("CIRCLE COMPLETE\n+%d XP / +%d Embers / +1 Sigil\nChallenge bonuses credited to wallet"),XP,EmberReward));
    UCombatAudioLibrary::PlayEvent(this,TEXT("UrnBreak"),GetActorLocation(),false,.7f);
    SetLifeSpan(4);
}
void ASurvivalFieldObjective::Tick(float Delta)
{
    Super::Tick(Delta);
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || !PC->GetPawn() || PC->IsPlayerDead()) { Destroy(); return; }
    if (PC->PlayerCameraManager) Label->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
    // Neither a menu's near-zero time dilation nor another objective spends progress/lifetime.
    if (UGameplayStatics::GetGlobalTimeDilation(this)<.01f || PC->IsAnyObjectiveActive())
    {
        if (bWallTrap) for (auto Wall:Walls) if (IsValid(Wall)) Wall->SetSealed(false);
        bSealed=false;
        return;
    }
    if (bComplete) return;
    Age+=Delta;
    if (bWallTrap)
    {
        if (Age>=1.5f && !bSealed)
        {
            // Do not raise a wall through a pawn who used the warning to move away.
            bool bOccupied=false;
            for (auto Wall:Walls) if (IsValid(Wall) && !Wall->IsDead())
                bOccupied |= Wall->Barrier->Bounds.GetBox().ExpandBy(60).IsInside(PC->GetPawn()->GetActorLocation());
            if (!bOccupied)
            {
                bSealed=true;
                for (auto Wall:Walls) if (IsValid(Wall)) Wall->SetSealed(true);
                UpdateLabel(TEXT("BREAK THE SEAL\nDestroy a wall to escape"));
            }
        }
        if (Age>=FMath::Max(1.f,WallLifetime)) Destroy();
    }
    else
    {
        const bool bInside=ContainsPlayer(PC->GetPawn()->GetActorLocation());
        AdvanceHold(Delta,bInside);
        if (!bComplete)
        {
            if (Progress>0) UpdateLabel(FString::Printf(TEXT("EMBER CIRCLE  %.1f / %.0fs\n%s"),Progress,HoldSeconds,bInside?TEXT("Hold your ground"):TEXT("Return to continue")));
            Marker->SetMarkerState(bInside?EMinimapMarkerState::Active:EMinimapMarkerState::Available);
            if (Age>105) Destroy();
        }
    }
}
void ASurvivalFieldObjective::EndPlay(const EEndPlayReason::Type Reason)
{
    for (auto Wall:Walls) if (IsValid(Wall)) Wall->Destroy();
    Super::EndPlay(Reason);
}
