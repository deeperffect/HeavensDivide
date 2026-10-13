#include "EncounterDirector.h"
#include "SurvivalFieldObjective.h"
#include "EncounterWall.h"
#include "EnemySpawner.h"
#include "SurvivorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

bool AEncounterDirector::SpawnWallTrap()
{
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || !PC->GetPawn() || PC->IsPlayerDead() || PC->IsAnyObjectiveActive() || WallTrap.IsValid()) return false;
    // An abandoned circle must not suppress the wall encounter.
    if (!Spawner.IsValid()) for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It) { Spawner=*It; break; }
    if (!Spawner.IsValid() || !Spawner->IsEncounterDirectorEnabled()) return false;
    FVector Ground;
    if (!Spawner->ProjectEncounterGround(PC->GetPawn()->GetActorLocation(),Ground)) return false;
    Ground.Z+=5;
    auto* Event=GetWorld()->SpawnActor<ASurvivalFieldObjective>(Ground,FRotator::ZeroRotator);
    if (!Event) return false;
    if (!Event->InitializeWallTrap(Spawner.Get())) { Event->Destroy(); return false; }
    WallTrap=Event;
    UE_LOG(LogTemp,Display,TEXT("Wall encounter spawned at %.1fs: %s"),Spawner->GetRunTimeSeconds(),*Ground.ToString());
    NextMarch=FMath::Max(NextMarch,Spawner->GetRunTimeSeconds()+38);
    return true;
}
bool AEncounterDirector::SpawnEmberCircle()
{
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || !PC->GetPawn() || PC->IsPlayerDead() || PC->IsAnyObjectiveActive() || EmberCircle.IsValid() || WallTrap.IsValid()) return false;
    if (!Spawner.IsValid()) for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It) { Spawner=*It; break; }
    if (!Spawner.IsValid() || !Spawner->IsEncounterDirectorEnabled()) return false;
    for (int32 Attempt=0; Attempt<16; ++Attempt)
    {
        const float A=FMath::FRandRange(0.f,2*PI);
        const FVector Center=PC->GetPawn()->GetActorLocation()+FVector(FMath::Cos(A),FMath::Sin(A),0)*1100;
        FVector Ground;
        if (!Spawner->ValidateEncounterLocation(Center,AEncounterWall::StaticClass(),Ground)) continue;
        bool bValid=true;
        for (int32 I=0; I<12 && bValid; ++I)
        {
            const float T=2*PI*I/12; FVector Edge;
            bValid=Spawner->ValidateEncounterLocation(Center+FVector(FMath::Cos(T),FMath::Sin(T),0)*ASurvivalFieldObjective::HoldZoneRadius,AEncounterWall::StaticClass(),Edge)
                && FMath::Abs(Edge.Z-Ground.Z)<60;
        }
        if (!bValid) continue;
        Ground.Z-=GetDefault<AEncounterWall>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-5;
        auto* Event=GetWorld()->SpawnActor<ASurvivalFieldObjective>(Ground,FRotator::ZeroRotator);
        if (!Event) return false;
        Event->InitializeHoldZone(); EmberCircle=Event; return true;
    }
    return false;
}
