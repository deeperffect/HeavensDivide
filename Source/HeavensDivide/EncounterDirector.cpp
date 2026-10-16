#include "EncounterDirector.h"
#include "EnemySpawner.h"
#include "TacticalEnemy.h"
#include "HealingUrn.h"
#include "HealingPickup.h"
#include "MagnetPickup.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "CombatAudio.h"
#include "EncounterHazard.h"
#include "SurvivalFieldObjective.h"
#include "EncounterWall.h"
#include "CombatGround.h"

AEncounterDirector::AEncounterDirector() { PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.5f; }
void AEncounterDirector::BeginPlay()
{
    Super::BeginPlay(); NextMarch=FirstMarchSeconds+FMath::FRandRange(0.f,15.f);
    NextMagnet=FirstMagnetSeconds;
    NextUrn=FirstUrnSeconds;
    WallTimes[0]=FMath::FRandRange(120.f,165.f);
    WallTimes[1]=FMath::FRandRange(300.f,345.f);
    for (TActorIterator<AEnemySpawner> It(GetWorld());It;++It) { Spawner=*It; break; }
}
void AEncounterDirector::Tick(float Delta)
{
    Super::Tick(Delta);
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || !PC->GetPawn() || PC->IsPlayerDead()) return;
    const bool Suspended=!Spawner.IsValid() || !Spawner->IsEncounterDirectorEnabled() || PC->IsAnyObjectiveActive();
    for (auto E:Marchers) if (E.IsValid()) E->SetGameplaySuspended(Suspended);
    if (Suspended) return;
    const float Now=Spawner->GetRunTimeSeconds();
    if (WallWindow<2 && Now>=WallTimes[WallWindow] && Now>=NextWallAttempt)
    {
        // Defer blocked encounters instead of silently discarding their time window.
        if (SpawnWallTrap()) ++WallWindow;
        else NextWallAttempt=Now+3.f;
    }
    if (Now>=NextCircle)
    {
        if (SpawnEmberCircle()) NextCircle=Now+180.f;
        else NextCircle=Now+10.f;
    }
    if (Now>=NextMarch)
    {
        const bool bSpawned=SpawnMarch();
        const float Pressure=Now>=480 ? .78f : 1.f;
        NextMarch=Now+(bSpawned ? FMath::FRandRange(MarchIntervalMin,MarchIntervalMax)*Pressure : 12.f);
    }
    if (Now>=NextUrn) { SpawnUrn(); NextUrn=Now+FMath::FRandRange(FMath::Max(10.f,UrnIntervalMin),FMath::Max(10.f,FMath::Max(UrnIntervalMin,UrnIntervalMax))); }
    if (Now>=NextMagnet)
    {
        const bool Spawned=SpawnUrn(true);
        NextMagnet=Now+(Spawned ? FMath::FRandRange(FMath::Max(10.f,MagnetIntervalMin),FMath::Max(MagnetIntervalMin,MagnetIntervalMax)) : 15.f);
    }
}
bool AEncounterDirector::SpawnMarch()
{
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if (!PC || !PC->GetPawn() || !MarchingEnemyClass) return false;
    Marchers.RemoveAll([](auto E){return !E.IsValid() || E->IsDead();});
    if (!Marchers.IsEmpty()) return false;
    if (!Spawner.IsValid())
        for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It) { Spawner=*It; break; }
    if (!Spawner.IsValid()) return false;
    const float Minutes=Spawner->GetRunTimeMinutes();
    const int32 Count=FMath::Clamp(13+2*FMath::FloorToInt(Minutes/3),13,FMath::Clamp(MaxMarchEnemies,13,21));
    FVector Travel, Across, Center;
    TArray<FVector> Positions;
    // Validate the complete line, never clamp individuals onto the arena edge.
    const float StartAngle=FMath::FRandRange(0.f,360.f);
    for (int32 Attempt=0; Attempt<24; ++Attempt)
    {
        const float Angle=FMath::DegreesToRadians(StartAngle+Attempt*137.50776f);
        Travel=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0);
        Across=FVector(-Travel.Y,Travel.X,0);
        Center=PC->GetPawn()->GetActorLocation()-Travel*1500;
        Positions.Reset();
        for (int32 I=0; I<Count; ++I)
        {
            const FVector Desired=Center+Across*(I-(Count-1)*.5f)*115;
            FVector Found;
            if (!Spawner->ValidateEncounterLocation(Desired,MarchingEnemyClass,Found)) break;
            Positions.Add(Found);
        }
        if (Positions.Num()==Count) break;
    }
    if (Positions.Num()!=Count) return false;
    for (FVector P:Positions)
    {

        if (auto* E=Cast<ATacticalEnemy>(Spawner->SpawnEnemyAtValidatedLocation(MarchingEnemyClass,P+FVector(0,0,2),Travel.Rotation())))
        {
            E->ApplySpawnDifficultyScaling(1.f,1+Minutes*.015f);
            E->InitializeMarch(Travel,2.5f*FMath::Min(330.f,235.f+Minutes*8),3600,1.4f); Marchers.Add(E);
        }
    }
    if (!Marchers.IsEmpty())
    {
        UCombatAudioLibrary::PlayEvent(this,TEXT("BossWarning"),Center,false,.65f);
    }
    return !Marchers.IsEmpty();
}
bool AEncounterDirector::SpawnUrn(bool bMagnet)
{
    auto& ActiveUrns=bMagnet ? MagnetUrns : Urns;
    const auto SpawnClass=bMagnet ? MagnetUrnClass : HealingUrnClass;
    ActiveUrns.RemoveAll([](auto E){return !E.IsValid() || E->IsDead();});
    if (!SpawnClass || ActiveUrns.Num()>=(bMagnet?1:2)) return false;
    int32 Pickups=0;
    if (bMagnet) { for(TActorIterator<AMagnetPickup> It(GetWorld());It;++It) if(!It->IsActorBeingDestroyed()) ++Pickups; }
    else { for(TActorIterator<AHealingPickup> It(GetWorld());It;++It) if(!It->IsActorBeingDestroyed()) ++Pickups; }
    if (Pickups>=(bMagnet?1:3)) return false;
    auto* PC=UGameplayStatics::GetPlayerController(this,0); if(!PC || !PC->GetPawn()) return false;
    if (!Spawner.IsValid()) for (TActorIterator<AEnemySpawner> It(GetWorld());It;++It) { Spawner=*It;break; }
    if (!Spawner.IsValid()) return false;
    for(int32 Attempt=0;Attempt<10;++Attempt)
    {
        const float A=FMath::FRandRange(0,2*PI), D=FMath::FRandRange(650.f,1150.f);
        const FVector Desired=PC->GetPawn()->GetActorLocation()+FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,0);
        FVector Found;
        if(!Spawner->ValidateEncounterLocation(Desired,SpawnClass,Found) || !CombatGround::IsInsideArena(GetWorld(),Found,60)) continue;
        if(FVector::Dist2D(Found,PC->GetPawn()->GetActorLocation())<550) continue;
        if(auto* Urn=Cast<AHealingUrn>(Spawner->SpawnEnemyAtValidatedLocation(SpawnClass,Found+FVector(0,0,2),FRotator(0,FMath::FRandRange(0.f,360.f),0))))
        {
            if (!CombatGround::IsInsideArena(GetWorld(),Urn->GetActorLocation(),Urn->GetCapsuleComponent()->GetScaledCapsuleRadius())) { Urn->Destroy(); continue; }
            Urn->SetLifeSpan(100); ActiveUrns.Add(Urn); return true;
        }
    }
    return false;
}
