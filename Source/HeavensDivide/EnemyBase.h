// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class UHealthComponent;
class UAnimMontage;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class ACharacterBase;
class UCharacterManagerComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetTarget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UHealthComponent* GetHealthComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UEnemyHealthBarWidget> HealthBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	float HealthBarHeightOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FVector2D HealthBarDrawSize = FVector2D(120.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StopDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bIsDead = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DeathDestroyDelay = 3.0f;

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	virtual void HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);

	void InitializeTargetFromCharacterManager();
	void InitializeHealthBar();
	virtual void UpdateEnemyBehavior(float DeltaSeconds);
	virtual bool ShouldSkipMovement() const;
	virtual void StopEnemyBehavior();
	void MoveTowardCurrentTarget();
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void DestroyAfterDeath();
	void FaceTarget();
	bool IsPlayerTargetDead() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy")
	void OnEnemyDeath();

	UPROPERTY()
	TObjectPtr<UCharacterManagerComponent> ObservedCharacterManager;

};
