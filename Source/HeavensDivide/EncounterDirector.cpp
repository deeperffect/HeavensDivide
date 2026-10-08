#include "EncounterDirector.h"
#include "EnemySpawner.h"
#include "TacticalEnemy.h"
#include "HealingUrn.h"
#include "HealingPickup.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "CombatAudio.h"
#include "EncounterHazard.h"

namespace
{
bool FindEncounterGround(UWorld* World,FVector Desired,FVector& Out,float Radius,float HalfHeight)
{
    FHitResult Ground;
    if(!World->LineTraceSingleByChannel(Ground,Desired+FVector(0,0,700),Desired-FVector(0,0,1800),ECC_GameTraceChannel2)
        || Ground.ImpactNormal.Z<.65f) return false;
    Out=Ground.ImpactPoint;
    return !World->OverlapBlockingTestByChannel(Out+FVector(0,0,HalfHeight+4),FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius,HalfHeight));
}
}

AEncounterDirector::AEncounterDirector() { PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.5f; }
void AEncounterDirector::BeginPlay()
{
    Super::BeginPlay(); NextMarch=FirstMarchSeconds+FMath::FRandRange(0.f,15.f);
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
    if (Now>=NextMarch)
    {
        const bool bSpawned=SpawnMarch();
        const float Pressure=Now>=480 ? .78f : 1.f;
        NextMarch=Now+(bSpawned ? FMath::FRandRange(MarchIntervalMin,MarchIntervalMax)*Pressure : 12.f);
    }
    if (Now>=NextUrn) { SpawnUrn(); NextUrn=Now+FMath::FRandRange(38.f,55.f); }
}
bool AEncounterDirector::SpawnMarch()
{
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if (!PC || !PC->GetPawn() || !MarchingEnemyClass) return false;
    Marchers.RemoveAll([](auto E){return !E.IsValid() || E->IsDead();});
    if (!Marchers.IsEmpty()) return false;
    const FRotator CameraRotation=PC->PlayerCameraManager->GetCameraRotation();
    const FVector Across=FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y).GetSafeNormal2D();
    const FVector Travel=FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::X).GetSafeNormal2D()*(FMath::RandBool()?1.f:-1.f);
    const FVector Center=PC->GetPawn()->GetActorLocation()-Travel*1500;
    const float Minutes=Spawner.IsValid()?Spawner->GetRunTimeMinutes():0;
    const int32 Count=FMath::Clamp(13+2*FMath::FloorToInt(Minutes/3),13,FMath::Clamp(MaxMarchEnemies,13,21));
    TArray<FVector> Positions;
    const auto* CDO=MarchingEnemyClass.GetDefaultObject();
    // Reject obstructed approaches instead of stacking a compressed line in a doorway.
    for (int32 I=0;I<Count;++I)
    {
        const FVector P=Center+Across*(I-(Count-1)*.5f)*115;
        FVector Found;
        if (!FindEncounterGround(GetWorld(),P,Found,CDO->GetCapsuleComponent()->GetScaledCapsuleRadius(),CDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())) return false;
        Positions.Add(Found);
    }
    for (FVector P:Positions)
    {
        P.Z+=CDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        if (auto* E=GetWorld()->SpawnActor<ATacticalEnemy>(MarchingEnemyClass,P,Travel.Rotation(),Params))
        {
            E->ApplySpawnDifficultyScaling(1+Minutes*.045f,1+Minutes*.015f);
            E->InitializeMarch(Travel,FMath::Min(330.f,235.f+Minutes*8),3600,1.4f); Marchers.Add(E);
        }
    }
    if (!Marchers.IsEmpty())
    {
        UCombatAudioLibrary::PlayEvent(this,TEXT("BossWarning"),Center,false,.65f);
        AEncounterHazard::Spawn(this,Center+Travel*420,EEncounterShape::Lane,Count*115,60,1.4f,0,.2f,Across.Rotation().Yaw);
    }
    return !Marchers.IsEmpty();
}
bool AEncounterDirector::SpawnUrn()
{
    Urns.RemoveAll([](auto E){return !E.IsValid() || E->IsDead();});
    if (!HealingUrnClass || Urns.Num()>=2) return false;
    int32 Pickups=0; for(TActorIterator<AHealingPickup> It(GetWorld());It;++It) ++Pickups;
    if (Pickups>=3) return false;
    auto* PC=UGameplayStatics::GetPlayerController(this,0); if(!PC || !PC->GetPawn()) return false;
    for(int32 Attempt=0;Attempt<10;++Attempt)
    {
        const float A=FMath::FRandRange(0,2*PI), D=FMath::FRandRange(650.f,1150.f);
        const FVector Desired=PC->GetPawn()->GetActorLocation()+FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,0);
        FVector Found;
        if(!FindEncounterGround(GetWorld(),Desired,Found,38,48)) continue;
        if(FVector::Dist2D(Found,PC->GetPawn()->GetActorLocation())<550) continue;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        if(auto* Urn=GetWorld()->SpawnActor<AHealingUrn>(HealingUrnClass,Found+FVector(0,0,52),FRotator(0,FMath::FRandRange(0.f,360.f),0),Params))
        { Urn->SetLifeSpan(100); Urns.Add(Urn); return true; }
    }
    return false;
}
