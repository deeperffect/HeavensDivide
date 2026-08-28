#pragma once

#include "CoreMinimal.h"
#include "TechniqueTrialBase.h"
#include "SamuraiTechniqueTrial.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UStaticMeshComponent;
class UTechniqueTrialWidget;

UENUM(BlueprintType)
enum class ESamuraiTrialLane : uint8 { Left, Center, Right };

UENUM(BlueprintType)
enum class ESamuraiMemoryTrialState : uint8
{
	Inactive,
	Preparing,
	Previewing,
	PreExecutionPause,
	Executing,
	BetweenRounds,
	AwaitingReward,
	Completed,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSamuraiTrialLaneEvent, ESamuraiTrialLane, Lane);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSamuraiTrialStrikeEvent, ESamuraiTrialLane, SafeLane, bool, bPlayerWasSafe);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ASamuraiTechniqueTrial : public ATechniqueTrialBase
{
	GENERATED_BODY()
public:
	ASamuraiTechniqueTrial();
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintPure, Category="Samurai Trial|Memory") ESamuraiMemoryTrialState GetMemoryTrialState() const { return MemoryState; }
	UFUNCTION(BlueprintPure, Category="Samurai Trial|Memory") const TArray<ESamuraiTrialLane>& GetCurrentSequence() const { return CurrentSequence; }

	UPROPERTY(BlueprintAssignable, Category="Samurai Trial|Events") FSamuraiTrialLaneEvent OnPreviewLaneShown;
	UPROPERTY(BlueprintAssignable, Category="Samurai Trial|Events") FSamuraiTrialLaneEvent OnPreviewLaneHidden;
	UPROPERTY(BlueprintAssignable, Category="Samurai Trial|Events") FSamuraiTrialLaneEvent OnStrikeTelegraph;
	UPROPERTY(BlueprintAssignable, Category="Samurai Trial|Events") FSamuraiTrialStrikeEvent OnStrikeExecuted;
protected:
	virtual bool BeginChallenge() override;
	virtual void StopChallenge() override;
	virtual bool ShouldSuspendAutoAttacksDuringTrial() const override { return true; }
	virtual void RelocateAdditionalArenaComponents(const FVector& WorldDelta) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="1")) int32 NumberOfRounds=3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="1")) int32 Round1SequenceLength=3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="1")) int32 Round2SequenceLength=4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="1")) int32 Round3SequenceLength=5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0", UIMin="0", ToolTip="Seconds to wait after arriving in the trial room before the first Round 1 preview indicator appears.")) float InitialPreviewDelay=1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0.05")) float PreviewDisplayDuration=0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0")) float PreviewGapDuration=0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0")) float PreExecutionPause=1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0.05")) float StrikeTelegraphDuration=0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0", UIMin="0", ToolTip="Seconds between activating the unsafe-lane Niagara effects and applying the strike damage.")) float StrikeDamageDelay=0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0")) float TimeBetweenStrikes=0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0")) float BetweenRoundDelay=1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="0")) float TrialStrikeDamage=30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(ClampMin="1")) int32 RewardChoiceCount=3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") TArray<TObjectPtr<UStaticMeshComponent>> Lanes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") TObjectPtr<UStaticMeshComponent> LeftLane;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") TObjectPtr<UStaticMeshComponent> CenterLane;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") TObjectPtr<UStaticMeshComponent> RightLane;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") float LaneWidth=380.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") float LaneDepth=1120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Lanes") TObjectPtr<UMaterialInterface> LaneIndicatorMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Strike VFX") TObjectPtr<UNiagaraComponent> LeftStrikeVFX;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Strike VFX") TObjectPtr<UNiagaraComponent> CenterStrikeVFX;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Samurai Trial|Strike VFX") TObjectPtr<UNiagaraComponent> RightStrikeVFX;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Debug", meta=(ToolTip="Debug-only: draws red boxes over the two damaging lane footprints when each strike resolves.")) bool bDrawStrikeDamageDebugBoxes=true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Debug", meta=(ClampMin="0.01")) float StrikeDebugBoxDuration=0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Debug", meta=(ClampMin="0.1")) float StrikeDebugBoxThickness=5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|Debug", meta=(ClampMin="1.0")) float StrikeDebugBoxHeight=100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Samurai Trial|UI") TSubclassOf<UTechniqueTrialWidget> TrialWidgetClass;
private:
	void BeginRound();
	void ShowPreviewStep();
	void HidePreviewStep();
	void BeginExecution();
	void TelegraphStrike();
	void ResolveStrike();
	void ApplyPendingStrikeDamage();
	void AdvanceExecution();
	void CompleteAllRounds();
	void SetLaneFillAmount(float FillAmount);
	void HideAllLanes();
	void ShowDangerLanesExcept(ESamuraiTrialLane SafeLane, float FillAmount);
	void SpawnStrikeImpactEffects(ESamuraiTrialLane SafeLane);
	void DrawStrikeDamageDebugBoxes(ESamuraiTrialLane SafeLane) const;
	bool IsPlayerInLane(ESamuraiTrialLane Lane) const;
	int32 GetSequenceLengthForRound(int32 RoundIndex) const;
	UFUNCTION() void HandleRewardCompleted();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(AllowPrivateAccess="true")) ESamuraiMemoryTrialState MemoryState=ESamuraiMemoryTrialState::Inactive;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Samurai Trial|Memory", meta=(AllowPrivateAccess="true")) TArray<ESamuraiTrialLane> CurrentSequence;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LaneMaterials;
	UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> StrikeVFXComponents;
	UPROPERTY() TObjectPtr<UTechniqueTrialWidget> TrialWidget;
	int32 CurrentRound=0;
	int32 CurrentStep=0;
	float DangerFillElapsed=0.0f;
	float DangerFillTarget=0.0f;
	bool bAnimateDangerFill=false;
	FTimerHandle PhaseTimer;
};
