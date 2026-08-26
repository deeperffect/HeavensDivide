#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AutoAttackComponent.h"
#include "PlayerUpgradeComponent.h"
#include "RunTravelSubsystem.generated.h"

class ASurvivorPlayerController;

USTRUCT()
struct FRunTravelSnapshot
{
	GENERATED_BODY()
	UPROPERTY() bool bValid = false;
	UPROPERTY() float RunTimeSeconds = 0.0f;
	UPROPERTY() float CurrentHealth = 0.0f;
	UPROPERTY() float ExpectedMaxHealth = 0.0f;
	UPROPERTY() int32 CurrentXP = 0;
	UPROPERTY() int32 CurrentLevel = 1;
	UPROPERTY() int32 CurrentDashCharges = 1;
	UPROPERTY() int32 ExpectedMaxDashCharges = 1;
	UPROPERTY() bool bNinjaActive = false;
	UPROPERTY() FPlayerUpgradeRunState Upgrades;
	UPROPERTY() FAutoAttackRunState SamuraiAttack;
	UPROPERTY() FAutoAttackRunState NinjaAttack;
};

UCLASS()
class HEAVENSDIVIDE_API URunTravelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	bool CaptureFromController(ASurvivorPlayerController* Controller);
	bool RestoreToController(ASurvivorPlayerController* Controller);
	bool HasPendingRestore() const { return Snapshot.bValid; }
	float GetCapturedRunTimeSeconds() const { return Snapshot.RunTimeSeconds; }
	bool ConsumeBossEntryArrival();
	void ClearSnapshot();
private:
	UPROPERTY(Transient) FRunTravelSnapshot Snapshot;
	bool bBossEntryArrival = false;
};
