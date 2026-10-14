#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterSmokeBurst.generated.h"
class UNiagaraComponent;
class UNiagaraSystem;

/** A detached burst survives hiding, moving or removing its assisting character. */
UCLASS(NotBlueprintable, Transient)
class ACharacterSmokeBurst : public AActor
{
    GENERATED_BODY()
public:
    ACharacterSmokeBurst();
    void Initialize(UNiagaraSystem* System, float Scale, float Seconds);
    UNiagaraComponent* GetEffect() const { return Effect; }
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<UNiagaraComponent> Effect;
    float Age = 0;
    float EmissionSeconds = .65f;
};
