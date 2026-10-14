#include "CharacterSmokeBurst.h"
#include "NiagaraComponent.h"

ACharacterSmokeBurst::ACharacterSmokeBurst()
{
    PrimaryActorTick.bCanEverTick=true;
    Effect=CreateDefaultSubobject<UNiagaraComponent>(TEXT("Smoke"));
    SetRootComponent(Effect);
    Effect->SetAutoActivate(false);
    Effect->SetAutoDestroy(false);
    Effect->SetForceSolo(true);
    Effect->SetAllowScalability(false);
    Effect->SetTranslucentSortPriority(40);
    SetActorEnableCollision(false);
}
void ACharacterSmokeBurst::Initialize(UNiagaraSystem* System,float Scale,float Seconds)
{
    Effect->SetAsset(System);
    SetActorScale3D(FVector(FMath::Max(.01f,Scale)));
    EmissionSeconds=FMath::Max(.05f,Seconds);
    Effect->SetRandomSeedOffset(static_cast<int32>(FPlatformTime::Cycles()));
    Effect->Activate(true);
}
void ACharacterSmokeBurst::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Swap freeze compensates the Niagara clock; emission uses that same clock.
    // Pausing stops actor and particle ticks together.
    Age+=DeltaSeconds*Effect->GetCustomTimeDilation();
    if (Age>=EmissionSeconds) Effect->Deactivate();
    if (Age>=EmissionSeconds+2.f) Destroy();
}
