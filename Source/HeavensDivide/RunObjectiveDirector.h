#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RunObjectiveDirector.generated.h"

class ABloodShrine;
class ABossToriiGate;
class AEnemySpawner;
class ANinjaTechniqueTrial;
class AObjectiveSpawnPoint;
class ASamuraiTechniqueTrial;
class ASurvivorPlayerController;
class ATwinSoulTrial;
class UObjectiveDirectorRunStateSubsystem;

UENUM()
enum class ERunObjectiveMilestoneType : uint8
{
	BloodShrine,
	GuaranteedCharacterTrial,
	TwinSoulTrial,
	OptionalCharacterTrial
};

USTRUCT()
struct FRunObjectiveMilestone
{
	GENERATED_BODY()
	float SpawnTime = 0.0f;
	ERunObjectiveMilestoneType Type = ERunObjectiveMilestoneType::BloodShrine;
};

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ARunObjectiveDirector : public AActor
{
	GENERATED_BODY()
public:
	ARunObjectiveDirector();
	UFUNCTION(BlueprintCallable, Category="Objective Director|Debug", meta=(DevelopmentOnly)) void DebugSpawnBloodShrine();
	UFUNCTION(BlueprintCallable, Category="Objective Director|Debug", meta=(DevelopmentOnly)) void DebugSpawnGuaranteedCharacterTrial();
	UFUNCTION(BlueprintCallable, Category="Objective Director|Debug", meta=(DevelopmentOnly)) void DebugSpawnTwinSoulTrial();
	UFUNCTION(BlueprintCallable, Category="Objective Director|Debug", meta=(DevelopmentOnly)) void DebugRollAndSpawnOptionalTrial();
	UFUNCTION(BlueprintCallable, Category="Objective Director|Debug", meta=(DevelopmentOnly)) void DebugTriggerAllObjectiveMilestones();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Schedule", meta=(ClampMin="0.0")) float BloodShrineSpawnTime = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Schedule", meta=(ClampMin="0.0")) float GuaranteedCharacterTrialSpawnTime = 210.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Schedule", meta=(ClampMin="0.0")) float TwinSoulTrialSpawnTime = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Schedule", meta=(ClampMin="0.0")) float OptionalCharacterTrialSpawnTime = 390.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Schedule", meta=(ClampMin="0.0", ClampMax="1.0")) float OptionalCharacterTrialChance = 0.50f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Spawn Rules") bool bReuseObjectiveSpawnPoints = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Spawn Rules", meta=(ClampMin="0.0")) float ObjectiveSpawnMinDistanceFromPlayer = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Spawn Rules", meta=(ClampMin="0.0")) float ObjectiveSpawnMaxDistanceFromPlayer = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Spawn Rules", meta=(ClampMin="0.0")) float ObjectiveMinSeparation = 800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Classes") TSubclassOf<ABloodShrine> BloodShrineClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Classes") TSubclassOf<ASamuraiTechniqueTrial> SamuraiTrialClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Classes") TSubclassOf<ANinjaTechniqueTrial> NinjaTrialClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Director|Classes") TSubclassOf<ATwinSoulTrial> TwinSoulTrialClass;
private:
	void InitializeDirector();
	void ScheduleMilestones();
	void ExecuteMilestone(int32 MilestoneIndex);
	void ExecuteMilestoneInternal(int32 MilestoneIndex, bool bIgnoreScheduleTime);
	AActor* SpawnObjective(TSubclassOf<AActor> ObjectiveClass, const TCHAR* ObjectiveName);
	TArray<AObjectiveSpawnPoint*> BuildPreferredCandidates() const;
	TArray<AObjectiveSpawnPoint*> GetUnusedCandidates() const;
	bool MeetsPlayerDistance(const AObjectiveSpawnPoint* Point) const;
	bool MeetsObjectiveSeparation(const AObjectiveSpawnPoint* Point) const;
	void ShufflePoints(TArray<AObjectiveSpawnPoint*>& Points) const;
	UFUNCTION() void HandleRunEnded();
	UFUNCTION() void HandleBossTravelStarted();
	UFUNCTION() void LogRunTimeStatus();
	void CancelPendingMilestones();
	float GetAuthoritativeRunTime() const;
	TSubclassOf<AActor> GetGuaranteedTrialClass() const;
	TSubclassOf<AActor> GetOptionalTrialClass() const;

	UPROPERTY() TObjectPtr<AEnemySpawner> RunTimeSource;
	UPROPERTY() TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY() TObjectPtr<ABossToriiGate> BossGate;
	UPROPERTY() TArray<TObjectPtr<AObjectiveSpawnPoint>> SpawnPoints;
	TArray<FRunObjectiveMilestone> Milestones;
	TArray<FTimerHandle> MilestoneTimers;
	FTimerHandle InitializationRetryTimer;
	FTimerHandle StatusLogTimer;
	bool bStopped = false;
};
