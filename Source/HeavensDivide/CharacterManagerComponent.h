// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterManagerComponent.generated.h"

class ACharacterBase;
class ANinjaCharacter;
class ASamuraiCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterSwapped, ACharacterBase*, OldCharacter, ACharacterBase*, NewCharacter);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UCharacterManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterManagerComponent();

	virtual void BeginPlay() override;

	void InitializeParty();
	void SwapCharacter();

	UPROPERTY(BlueprintAssignable, Category = "Characters", meta = (ToolTip = "Broadcast after a successful swap between Samurai and Ninja."))
	FOnCharacterSwapped OnCharacterSwapped;

	UFUNCTION(BlueprintPure, Category = "Characters")
	ACharacterBase* GetActiveCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Characters")
	ACharacterBase* GetInactiveCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Characters")
	ASamuraiCharacter* GetSamurai() const;

	UFUNCTION(BlueprintPure, Category = "Characters")
	ANinjaCharacter* GetNinja() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Characters", meta = (FormerlySerializedAs = "SamuraiCharacterClass", ToolTip = "Samurai Blueprint class spawned and managed as one of the two player characters."))
	TSubclassOf<ASamuraiCharacter> SamuraiClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Characters", meta = (FormerlySerializedAs = "NinjaCharacterClass", ToolTip = "Ninja Blueprint class spawned and managed as one of the two player characters."))
	TSubclassOf<ANinjaCharacter> NinjaClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Characters", meta = (ToolTip = "Runtime Samurai instance owned by the character manager."))
	TObjectPtr<ASamuraiCharacter> SamuraiCharacter;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Characters", meta = (ToolTip = "Runtime Ninja instance owned by the character manager."))
	TObjectPtr<ANinjaCharacter> NinjaCharacter;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Characters", meta = (ToolTip = "Currently controlled/active player character."))
	TObjectPtr<ACharacterBase> ActiveCharacter;

	FTransform GetInitialSpawnTransform() const;
	void ApplyInitialCharacterModes();

	bool bIsSwapInProgress = false;
};
