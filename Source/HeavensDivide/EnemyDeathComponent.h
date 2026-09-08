#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundConcurrency.h"
#include "EnemyDeathComponent.generated.h"

class UNiagaraSystem;
class USoundBase;
class UMaterialInstanceDynamic;

/** Shared across enemy types, including when they use different death sounds. */
UCLASS()
class HEAVENSDIVIDE_API UEnemyDeathSoundConcurrency : public USoundConcurrency
{
	GENERATED_BODY()
public:
	UEnemyDeathSoundConcurrency();
};

/** Presentation only: the enemy's authoritative death handler owns gameplay and cleanup. */
UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UEnemyDeathComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UEnemyDeathComponent();
	void StartDeathPresentation(FSimpleDelegate OnComplete);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death")
	TObjectPtr<UNiagaraSystem> DeathNiagaraSystem;
	/** World-space offset from the actor location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death")
	FVector NiagaraSpawnOffset = FVector::ZeroVector;

	/** Use a non-looping sound. Playback does not allocate an attached audio component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death")
	TObjectPtr<USoundBase> DeathSound;

	/** Empty uses a shared 16-voice limit across all standard enemy deaths. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death")
	TObjectPtr<USoundConcurrency> DeathSoundConcurrency;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death|Dissolve", meta=(ClampMin="0.0", Units="s"))
	float DissolveDuration = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death|Dissolve")
	FName DissolveParameterName = TEXT("DissolveAmount");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death|Dissolve")
	float DissolveStartValue = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Death|Dissolve")
	float DissolveEndValue = 1.0f;

private:
	void ApplyDissolve(float Alpha);
	void Finish();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DissolveMaterials;
	FSimpleDelegate Completion;
	float Elapsed = 0.0f;
	bool bStarted = false;
	bool bFinished = false;
};
