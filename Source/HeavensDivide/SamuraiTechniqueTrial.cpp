#include "SamuraiTechniqueTrial.h"

#include "CharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemySpawner.h"
#include "HealthComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "SurvivorPlayerController.h"
#include "TechniqueTrialWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASamuraiTechniqueTrial::ASamuraiTechniqueTrial()
{
	bForceSamuraiOnEntry = false;
	bLockSwappingDuringTrial = true;
	bSuspendAutoAttacksDuringTrial = false;
	constexpr float ArenaYaw=45.0f;
	const FRotator ArenaRotation(0.0f,ArenaYaw,0.0f);
	TrialPlayerOffset=ArenaRotation.RotateVector(FVector(0.0f,-440.0f,150.0f));
	TrialWidgetClass = UTechniqueTrialWidget::StaticClass();

	// Compact dojo: one fifth of the original prototype footprint. The thicker
	// blocking floor has its top at the arena origin so pawns settle predictably.
	TrialFloor->SetupAttachment(SceneRoot);
	TrialFloor->SetRelativeLocation(TrialArenaOffset+ArenaRotation.RotateVector(FVector(0.0f,0.0f,-50.0f)));
	TrialFloor->SetRelativeRotation(ArenaRotation);
	TrialFloor->SetRelativeScale3D(FVector(6.0f, 6.0f, 1.0f));
	TrialFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TrialFloor->SetCollisionResponseToAllChannels(ECR_Block);
	const FVector CompactWallLocations[] = {
		FVector(0.0f, 600.0f, 60.0f), FVector(0.0f, -600.0f, 60.0f),
		FVector(600.0f, 0.0f, 60.0f), FVector(-600.0f, 0.0f, 60.0f)};
	const FVector CompactWallScales[] = {
		FVector(6.0f, 0.2f, 0.6f), FVector(6.0f, 0.2f, 0.6f),
		FVector(0.2f, 6.0f, 0.6f), FVector(0.2f, 6.0f, 0.6f)};
	for (int32 WallIndex = 0; WallIndex < TrialWalls.Num(); ++WallIndex)
	{
		TrialWalls[WallIndex]->SetupAttachment(SceneRoot);
		TrialWalls[WallIndex]->SetRelativeLocation(TrialArenaOffset+ArenaRotation.RotateVector(CompactWallLocations[WallIndex]));
		TrialWalls[WallIndex]->SetRelativeRotation(ArenaRotation);
		TrialWalls[WallIndex]->SetRelativeScale3D(CompactWallScales[WallIndex]);
		TrialWalls[WallIndex]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TrialWalls[WallIndex]->SetCollisionResponseToAllChannels(ECR_Block);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceMaterial(TEXT("/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator.M_SamuraiLaneIndicator"));
	if(SurfaceMaterial.Succeeded())LaneIndicatorMaterial=SurfaceMaterial.Object;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float LaneX = (Index - 1) * 400.0f;
		const FName LaneName=Index==0?TEXT("LeftLane"):Index==1?TEXT("CenterLane"):TEXT("RightLane");
		UStaticMeshComponent* Lane=CreateDefaultSubobject<UStaticMeshComponent>(LaneName);
		Lane->SetupAttachment(SceneRoot);
		Lane->SetRelativeLocation(TrialArenaOffset+ArenaRotation.RotateVector(FVector(LaneX,0.0f,2.0f)));
		Lane->SetRelativeRotation(ArenaRotation);
		Lane->SetRelativeScale3D(FVector(LaneWidth/100.0f,LaneDepth/100.0f,0.03f));
		Lane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lane->SetHiddenInGame(true);
		Lane->SetVisibility(false);
		if(Cube.Succeeded())Lane->SetStaticMesh(Cube.Object);
		if(LaneIndicatorMaterial)Lane->SetMaterial(0,LaneIndicatorMaterial);
		Lanes.Add(Lane);
		if(Index==0)LeftLane=Lane;
		else if(Index==1)CenterLane=Lane;
		else RightLane=Lane;

		const FName VFXName=Index==0?TEXT("LeftStrikeVFX"):Index==1?TEXT("CenterStrikeVFX"):TEXT("RightStrikeVFX");
		UNiagaraComponent* StrikeVFX=CreateDefaultSubobject<UNiagaraComponent>(VFXName);
		StrikeVFX->SetupAttachment(SceneRoot);
		StrikeVFX->SetRelativeLocation(TrialArenaOffset+ArenaRotation.RotateVector(FVector(LaneX,0.0f,7.0f)));
		StrikeVFX->SetRelativeRotation(ArenaRotation);
		StrikeVFX->SetAutoActivate(false);
		StrikeVFXComponents.Add(StrikeVFX);
		if(Index==0)LeftStrikeVFX=StrikeVFX;
		else if(Index==1)CenterStrikeVFX=StrikeVFX;
		else RightStrikeVFX=StrikeVFX;
	}
}

void ASamuraiTechniqueTrial::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if(!bAnimateDangerFill)return;

	DangerFillElapsed+=DeltaSeconds;
	const float Duration=FMath::Max(0.01f,PreviewDisplayDuration);
	const float Alpha=FMath::Clamp(DangerFillElapsed/Duration,0.0f,1.0f);
	SetLaneFillAmount(FMath::Lerp(0.0f,DangerFillTarget,Alpha));
	if(Alpha>=1.0f)bAnimateDangerFill=false;
}

bool ASamuraiTechniqueTrial::BeginChallenge()
{
	if(!PlayerController||Lanes.Num()!=3)return false;
	// Samurai Memory Trial is character-specific. Reinforce the lock here as
	// well as in the class default so older Blueprint defaults cannot permit a
	// mid-trial swap. TechniqueTrialBase releases it during every cleanup path.
	bLockSwappingDuringTrial = true;
	PlayerController->SetSwapLocked(true);
	LaneMaterials.Reset();
	for(UStaticMeshComponent* Lane:Lanes)
	{
		if(!Lane)continue;
		if(LaneIndicatorMaterial)Lane->SetMaterial(0,LaneIndicatorMaterial);
		UMaterialInstanceDynamic* Material=Lane->CreateAndSetMaterialInstanceDynamic(0);
		if(Material)
		{
			Material->SetVectorParameterValue(TEXT("FillColor"),FLinearColor(1.0f,0.02f,0.01f,1.0f));
			Material->SetScalarParameterValue(TEXT("FillAmount"),0.0f);
		}
		LaneMaterials.Add(Material);
	}
	HideAllLanes();

	if (TrialWidgetClass)
	{
		TrialWidget = CreateWidget<UTechniqueTrialWidget>(PlayerController, TrialWidgetClass);
		if (TrialWidget)
		{
			TrialWidget->AddToViewport(20);
			TrialWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
			TrialWidget->SetPositionInViewport(FVector2D(40.0f, 95.0f), false);
		}
	}
	PlayerController->OnSamuraiTrialRewardCompleted.AddUniqueDynamic(this, &ASamuraiTechniqueTrial::HandleRewardCompleted);
	CurrentRound = 0;
	MemoryState = ESamuraiMemoryTrialState::Preparing;
	if (TrialWidget) TrialWidget->ShowMemoryStatus(1, NumberOfRounds, FText::FromString(TEXT("GET READY")), 0, 0);
	if (InitialPreviewDelay > 0.0f)
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::BeginRound, InitialPreviewDelay, false);
	else
		BeginRound();
	return true;
}

void ASamuraiTechniqueTrial::StopChallenge()
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	bAnimateDangerFill=false;
	HideAllLanes();
	for(UNiagaraComponent* StrikeVFX:StrikeVFXComponents)if(StrikeVFX)StrikeVFX->DeactivateImmediate();
	if (PlayerController) PlayerController->OnSamuraiTrialRewardCompleted.RemoveDynamic(this, &ASamuraiTechniqueTrial::HandleRewardCompleted);
	if (TrialWidget)
	{
		TrialWidget->RemoveFromParent();
		TrialWidget = nullptr;
	}
	if (MemoryState != ESamuraiMemoryTrialState::Completed && MemoryState != ESamuraiMemoryTrialState::AwaitingReward)
		MemoryState = ESamuraiMemoryTrialState::Failed;
}

int32 ASamuraiTechniqueTrial::GetSequenceLengthForRound(int32 RoundIndex) const
{
	if (RoundIndex == 0) return FMath::Max(1, Round1SequenceLength);
	if (RoundIndex == 1) return FMath::Max(1, Round2SequenceLength);
	if (RoundIndex == 2) return FMath::Max(1, Round3SequenceLength);
	return FMath::Max(1, Round3SequenceLength);
}

void ASamuraiTechniqueTrial::BeginRound()
{
	CurrentSequence.Reset();
	const int32 Length = GetSequenceLengthForRound(CurrentRound);
	for (int32 Index = 0; Index < Length; ++Index)
		CurrentSequence.Add(static_cast<ESamuraiTrialLane>(FMath::RandRange(0, 2)));
#if !UE_BUILD_SHIPPING
	FString SequenceText;
	for (ESamuraiTrialLane Lane : CurrentSequence) SequenceText += FString::Printf(TEXT("%d "), static_cast<int32>(Lane));
	UE_LOG(LogTemp, Display, TEXT("[SamuraiMemoryTrial] Round=%d Length=%d Sequence=%s"), CurrentRound + 1, Length, *SequenceText);
#endif
	CurrentStep = 0;
	MemoryState = ESamuraiMemoryTrialState::Previewing;
	HideAllLanes();
	if (TrialWidget) TrialWidget->ShowMemoryStatus(CurrentRound + 1, NumberOfRounds, FText::FromString(TEXT("MEMORIZE")), 0, Length);
	ShowPreviewStep();
}

void ASamuraiTechniqueTrial::ShowPreviewStep()
{
	if (MemoryState != ESamuraiMemoryTrialState::Previewing || !CurrentSequence.IsValidIndex(CurrentStep)) return;
	HideAllLanes();
	const ESamuraiTrialLane Lane = CurrentSequence[CurrentStep];
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("[SamuraiMemoryTrial] Preview Round=%d Step=%d SafeLane=%d"), CurrentRound + 1, CurrentStep + 1, static_cast<int32>(Lane));
#endif
	ShowDangerLanesExcept(Lane,1.0f);
	OnPreviewLaneShown.Broadcast(Lane);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::HidePreviewStep, FMath::Max(0.01f, PreviewDisplayDuration), false);
}

void ASamuraiTechniqueTrial::HidePreviewStep()
{
	if (!CurrentSequence.IsValidIndex(CurrentStep)) return;
	const ESamuraiTrialLane Lane = CurrentSequence[CurrentStep];
	HideAllLanes();
	OnPreviewLaneHidden.Broadcast(Lane);
	++CurrentStep;
	if (CurrentStep < CurrentSequence.Num())
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::ShowPreviewStep, FMath::Max(0.01f, PreviewGapDuration), false);
		return;
	}
	MemoryState = ESamuraiMemoryTrialState::PreExecutionPause;
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::BeginExecution, FMath::Max(0.01f, PreExecutionPause), false);
}

void ASamuraiTechniqueTrial::BeginExecution()
{
	MemoryState = ESamuraiMemoryTrialState::Executing;
	CurrentStep = 0;
	if (TrialWidget) TrialWidget->ShowMemoryStatus(CurrentRound + 1, NumberOfRounds, FText::FromString(TEXT("MOVE")), 1, CurrentSequence.Num());
	TelegraphStrike();
}

void ASamuraiTechniqueTrial::TelegraphStrike()
{
	if (MemoryState != ESamuraiMemoryTrialState::Executing || !CurrentSequence.IsValidIndex(CurrentStep)) return;
	const ESamuraiTrialLane SafeLane = CurrentSequence[CurrentStep];
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("[SamuraiMemoryTrial] Telegraph Round=%d Strike=%d SafeLane=%d"), CurrentRound + 1, CurrentStep + 1, static_cast<int32>(SafeLane));
#endif
	HideAllLanes();
	OnStrikeTelegraph.Broadcast(SafeLane);
	if (TrialWidget) TrialWidget->ShowMemoryStatus(CurrentRound + 1, NumberOfRounds, FText::FromString(TEXT("MOVE")), CurrentStep + 1, CurrentSequence.Num());
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::ResolveStrike, FMath::Max(0.01f, StrikeTelegraphDuration), false);
}

void ASamuraiTechniqueTrial::ResolveStrike()
{
	if (MemoryState != ESamuraiMemoryTrialState::Executing || !CurrentSequence.IsValidIndex(CurrentStep)) return;
	const ESamuraiTrialLane SafeLane = CurrentSequence[CurrentStep];
	HideAllLanes();
	SpawnStrikeImpactEffects(SafeLane);
	if(StrikeDamageDelay>0.0f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer,this,&ASamuraiTechniqueTrial::ApplyPendingStrikeDamage,StrikeDamageDelay,false);
		return;
	}
	ApplyPendingStrikeDamage();
}

void ASamuraiTechniqueTrial::ApplyPendingStrikeDamage()
{
	if(MemoryState!=ESamuraiMemoryTrialState::Executing||!CurrentSequence.IsValidIndex(CurrentStep))return;
	const ESamuraiTrialLane SafeLane=CurrentSequence[CurrentStep];
	DrawStrikeDamageDebugBoxes(SafeLane);
	const bool bSafe = IsPlayerInLane(SafeLane);
	if (!bSafe && PlayerController && !PlayerController->IsPlayerDead())
	{
		if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent()) Health->ApplyDamage(TrialStrikeDamage);
	}
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("[SamuraiMemoryTrial] Strike Round=%d Step=%d SafeLane=%d PlayerSafe=%d Damage=%.1f"), CurrentRound + 1, CurrentStep + 1, static_cast<int32>(SafeLane), bSafe, bSafe ? 0.0f : TrialStrikeDamage);
#endif
	OnStrikeExecuted.Broadcast(SafeLane, bSafe);
	++CurrentStep;
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::AdvanceExecution, FMath::Max(0.01f, TimeBetweenStrikes), false);
}

void ASamuraiTechniqueTrial::AdvanceExecution()
{
	if (PlayerController && PlayerController->IsPlayerDead()) return;
	if (CurrentStep < CurrentSequence.Num())
	{
		TelegraphStrike();
		return;
	}
	++CurrentRound;
	if (CurrentRound >= FMath::Max(1, NumberOfRounds))
	{
		CompleteAllRounds();
		return;
	}
	MemoryState = ESamuraiMemoryTrialState::BetweenRounds;
	if (TrialWidget) TrialWidget->ShowMemoryStatus(CurrentRound, NumberOfRounds, FText::FromString(TEXT("ROUND COMPLETE")), 0, 0);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiTechniqueTrial::BeginRound, FMath::Max(0.01f, BetweenRoundDelay), false);
}

void ASamuraiTechniqueTrial::CompleteAllRounds()
{
	MemoryState = ESamuraiMemoryTrialState::AwaitingReward;
	if (TrialWidget) TrialWidget->ShowComplete();
	if (PlayerController) PlayerController->RequestSamuraiTrialUpgradeReward(RewardChoiceCount);
}

void ASamuraiTechniqueTrial::HandleRewardCompleted()
{
	if (MemoryState != ESamuraiMemoryTrialState::AwaitingReward) return;
	MemoryState = ESamuraiMemoryTrialState::Completed;
	FinishChallenge();
}

void ASamuraiTechniqueTrial::SetLaneFillAmount(float FillAmount)
{
	const float Alpha=FMath::Clamp(FillAmount,0.0f,1.0f);
	for(UMaterialInstanceDynamic* Material:LaneMaterials)
		if(Material)Material->SetScalarParameterValue(TEXT("FillAmount"),Alpha);
}

void ASamuraiTechniqueTrial::HideAllLanes()
{
	bAnimateDangerFill=false;
	SetLaneFillAmount(0.0f);
	for(int32 LaneIndex=0;LaneIndex<Lanes.Num();++LaneIndex)
	{
		UStaticMeshComponent* Lane=Lanes[LaneIndex].Get();
		if(!Lane)continue;
		Lane->SetHiddenInGame(true);
		Lane->SetVisibility(false);
	}
}

void ASamuraiTechniqueTrial::ShowDangerLanesExcept(ESamuraiTrialLane SafeLane,float FillAmount)
{
	DangerFillElapsed=0.0f;
	DangerFillTarget=FMath::Clamp(FillAmount,0.0f,1.0f);
	bAnimateDangerFill=DangerFillTarget>0.0f;
	SetLaneFillAmount(0.0f);
	for(int32 LaneIndex=0;LaneIndex<Lanes.Num();++LaneIndex)
	{
		UStaticMeshComponent* Lane=Lanes[LaneIndex].Get();
		if(!Lane||LaneIndex==static_cast<int32>(SafeLane))continue;
		Lane->SetHiddenInGame(false);
		Lane->SetVisibility(true);
	}
}

void ASamuraiTechniqueTrial::SpawnStrikeImpactEffects(ESamuraiTrialLane SafeLane)
{
	for(int32 LaneIndex=0;LaneIndex<StrikeVFXComponents.Num();++LaneIndex)
	{
		if(LaneIndex==static_cast<int32>(SafeLane))continue;
		UNiagaraComponent* StrikeVFX=StrikeVFXComponents[LaneIndex].Get();
		if(StrikeVFX&&StrikeVFX->GetAsset())StrikeVFX->Activate(true);
	}
}

void ASamuraiTechniqueTrial::DrawStrikeDamageDebugBoxes(ESamuraiTrialLane SafeLane) const
{
#if !UE_BUILD_SHIPPING
	if(!bDrawStrikeDamageDebugBoxes||!GetWorld())return;
	for(int32 LaneIndex=0;LaneIndex<Lanes.Num();++LaneIndex)
	{
		if(LaneIndex==static_cast<int32>(SafeLane))continue;
		const UStaticMeshComponent* Lane=Lanes[LaneIndex].Get();
		if(!Lane)continue;
		const FVector Scale=Lane->GetComponentScale();
		const FVector Extent(
			50.0f*FMath::Abs(Scale.X),
			50.0f*FMath::Abs(Scale.Y),
			FMath::Max(1.0f,StrikeDebugBoxHeight*0.5f));
		DrawDebugBox(
			GetWorld(),Lane->GetComponentLocation(),Extent,Lane->GetComponentQuat(),
			FColor::Red,false,StrikeDebugBoxDuration,0,StrikeDebugBoxThickness);
	}
#endif
}

bool ASamuraiTechniqueTrial::IsPlayerInLane(ESamuraiTrialLane Lane) const
{
	const int32 Index = static_cast<int32>(Lane);
	ACharacterBase* Character = GetActiveCharacter();
	if(!Character||!Lanes.IsValidIndex(Index)||!Lanes[Index])return false;

	const UStaticMeshComponent* LaneComponent=Lanes[Index].Get();
	const FVector LocalPlayerLocation=LaneComponent->GetComponentTransform().InverseTransformPosition(Character->GetActorLocation());
	// Engine's cube mesh is 100 units per side. The component transform is the
	// single authored source for both the visible indicator and safe footprint.
	return FMath::Abs(LocalPlayerLocation.X)<=50.0f
		&& FMath::Abs(LocalPlayerLocation.Y)<=50.0f;
}
