#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EliteRewardChest.generated.h"

class AExperiencePickup;
class ASurvivorPlayerController;
class UExperienceComponent;
class UCharacterManagerComponent;
class UPoseableMeshComponent;
class UCameraComponent;
class UNiagaraComponent;
class UPointLightComponent;
class USoundBase;

/** Shared reward for enemies whose DropCategory is Elite. XP is conserved across the orb shower. */
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AEliteRewardChest : public AActor
{
 GENERATED_BODY()
public:
 AEliteRewardChest();
 void InitializeReward(int32 Value, TSubclassOf<AExperiencePickup> Pickup, UExperienceComponent* Experience, UCharacterManagerComponent* Manager);
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="1",ClampMax="64")) int32 OrbCount=40;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="1.5",ClampMax="6")) float OpeningDuration=2.8f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="55")) float CollectRadius=140.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward") FName LidBone=TEXT("Bone");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward") FRotator LidOpenRotation=FRotator(0,0,105);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward") TObjectPtr<USoundBase> OpeningSound;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Reward") TObjectPtr<UPoseableMeshComponent> ChestMesh;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Reward") TObjectPtr<UNiagaraComponent> RewardGlow;
private:
 friend class FEliteRewardTest;
 void FinishOpening();
 void RestorePlayer();
 UPROPERTY() TObjectPtr<UCameraComponent> Camera;
 UPROPERTY() TObjectPtr<UPointLightComponent> Light;
 UPROPERTY() TObjectPtr<UExperienceComponent> Experience;
 UPROPERTY() TObjectPtr<UCharacterManagerComponent> Manager;
 UPROPERTY() TSubclassOf<AExperiencePickup> PickupClass;
 TWeakObjectPtr<ASurvivorPlayerController> Player;
 TWeakObjectPtr<AActor> PreviousView;
 int32 TotalXP=0;
 bool bOpening=false,bOwnsPause=false,bRewardReleased=false,bPlayedOpeningSound=false;
 double OpeningStarted=0,LastRealTime=0;
 FTransform ClosedLid;
};
