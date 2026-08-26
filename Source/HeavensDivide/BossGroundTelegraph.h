#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossGroundTelegraph.generated.h"

class ASurvivorPlayerController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ABossGroundTelegraph : public AActor
{
	GENERATED_BODY()

public:
	ABossGroundTelegraph();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void InitializeTelegraph(ASurvivorPlayerController* InPlayerController, float InRadius, float InDuration, float InDamage, UMaterialInterface* InMaterial);
	void CancelTelegraph();

	UFUNCTION(BlueprintImplementableEvent, Category="Boss|Telegraph")
	void OnTelegraphResolved(bool bHitPlayer);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Telegraph")
	TObjectPtr<UStaticMeshComponent> TelegraphVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Telegraph")
	FLinearColor TelegraphColor = FLinearColor::Red;

private:
	TWeakObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
	float Radius = 180.0f;
	float Duration = 0.8f;
	float Damage = 18.0f;
	float Elapsed = 0.0f;
	bool bInitialized = false;
	bool bResolved = false;
};
