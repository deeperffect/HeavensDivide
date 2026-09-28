#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CharacterSimulationSetupCommandlet.generated.h"

UCLASS()
class UCharacterSimulationSetupCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UCharacterSimulationSetupCommandlet();
 virtual int32 Main(const FString& Params) override;
 UFUNCTION(BlueprintCallable, Category="Editor|CharacterSimulation")
 static bool VerifySimulation(const FString& OnlyCharacter);
 UFUNCTION(BlueprintCallable, Category="Editor|CharacterSimulation")
 static bool RepairClothBindings();
 UFUNCTION(BlueprintCallable, Category="Editor|CharacterSimulation")
 static bool RemoveGeneratedCloth();
};
