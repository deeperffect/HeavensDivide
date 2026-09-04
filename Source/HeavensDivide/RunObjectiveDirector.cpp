#include "RunObjectiveDirector.h"

#include "BloodShrine.h"
#include "BossToriiGate.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "NinjaTechniqueTrial.h"
#include "ObjectiveDirectorRunStateSubsystem.h"
#include "ObjectiveSpawnPoint.h"
#include "SamuraiTechniqueTrial.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "TrialChoiceWidget.h"
#include "TwinSoulTrial.h"
#include "UObject/ConstructorHelpers.h"

ARunObjectiveDirector::ARunObjectiveDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FClassFinder<ABloodShrine> BloodBP(TEXT("/Game/HeavensDivide/Blueprints/Objectives/BP_BloodShrine"));
	static ConstructorHelpers::FClassFinder<ASamuraiTechniqueTrial> SamuraiBP(TEXT("/Game/HeavensDivide/Blueprints/Objectives/BP_SamuraiTechniqueTrial"));
	static ConstructorHelpers::FClassFinder<ANinjaTechniqueTrial> NinjaBP(TEXT("/Game/HeavensDivide/Blueprints/Objectives/BP_NinjaTechniqueTrial"));
	static ConstructorHelpers::FClassFinder<ATwinSoulTrial> TwinBP(TEXT("/Game/HeavensDivide/Blueprints/Objectives/BP_TwinSoulTrial"));
	BloodShrineClass = BloodBP.Succeeded() ? BloodBP.Class : TSubclassOf<ABloodShrine>(ABloodShrine::StaticClass());
	SamuraiTrialClass = SamuraiBP.Succeeded() ? SamuraiBP.Class : TSubclassOf<ASamuraiTechniqueTrial>(ASamuraiTechniqueTrial::StaticClass());
	NinjaTrialClass = NinjaBP.Succeeded() ? NinjaBP.Class : TSubclassOf<ANinjaTechniqueTrial>(ANinjaTechniqueTrial::StaticClass());
	TwinSoulTrialClass = TwinBP.Succeeded() ? TwinBP.Class : TSubclassOf<ATwinSoulTrial>(ATwinSoulTrial::StaticClass());
}

void ARunObjectiveDirector::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] BeginPlay Actor=%s World=%s PendingKill=%s"), *GetName(), *GetNameSafe(GetWorld()), IsActorBeingDestroyed() ? TEXT("true") : TEXT("false"));
	InitializeDirector();
}

void ARunObjectiveDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseFirstTrialChoice(false);
	CancelPendingMilestones();
	if (PlayerController && PlayerController->GetPlayerHealthComponent()) PlayerController->GetPlayerHealthComponent()->OnDeath.RemoveDynamic(this, &ARunObjectiveDirector::HandleRunEnded);
	if (BossGate) BossGate->OnBossGateTravelStarted.RemoveDynamic(this, &ARunObjectiveDirector::HandleBossTravelStarted);
	Super::EndPlay(EndPlayReason);
}

void ARunObjectiveDirector::InitializeDirector()
{
	if (bStopped || !GetWorld()) return;
	PlayerController = Cast<ASurvivorPlayerController>(GetWorld()->GetFirstPlayerController());
	for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It) { RunTimeSource = *It; break; }
	if (!PlayerController || !RunTimeSource)
	{
		GetWorldTimerManager().SetTimer(InitializationRetryTimer, this, &ARunObjectiveDirector::InitializeDirector, 0.25f, false);
		return;
	}
	GetWorldTimerManager().ClearTimer(InitializationRetryTimer);
	SpawnPoints.Reset();
	for (TActorIterator<AObjectiveSpawnPoint> It(GetWorld()); It; ++It) SpawnPoints.Add(*It);
	for (TActorIterator<ABossToriiGate> It(GetWorld()); It; ++It) { BossGate = *It; break; }
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] ToriiGuard FoundActor=%s Class=%s"), *GetNameSafe(BossGate), BossGate ? *GetNameSafe(BossGate->GetClass()) : TEXT("None"));
	if (!BossGate)
	{
		bStopped = true;
		UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] No BossToriiGate found; director disabled outside the survival arena."));
		return;
	}
	if (PlayerController->IsPlayerDead())
	{
		bStopped = true;
		return;
	}
	if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent()) Health->OnDeath.AddUniqueDynamic(this, &ARunObjectiveDirector::HandleRunEnded);
	if (BossGate) BossGate->OnBossGateTravelStarted.AddUniqueDynamic(this, &ARunObjectiveDirector::HandleBossTravelStarted);

	UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	State->InitializeRunDecisions();
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] State ExecutedMask=0x%02x UsedPoints=%d"), State->GetExecutedMilestoneMask(), State->GetUsedPointCount());
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] Classes BloodShrine=%s SamuraiTrial=%s NinjaTrial=%s TwinSoulTrial=%s"), *GetNameSafe(BloodShrineClass), *GetNameSafe(SamuraiTrialClass), *GetNameSafe(NinjaTrialClass), *GetNameSafe(TwinSoulTrialClass));
	for (AObjectiveSpawnPoint* Point : SpawnPoints) UE_LOG(LogTemp, Verbose, TEXT("[ObjectiveDirector] SpawnPoint Name=%s Location=%s"), *GetNameSafe(Point), *Point->GetActorLocation().ToCompactString());
	const bool bClassesValid = BloodShrineClass && SamuraiTrialClass && NinjaTrialClass && TwinSoulTrialClass;
	if (SpawnPoints.IsEmpty())
	{
		bStopped = true;
		UE_LOG(LogTemp, Error, TEXT("[ObjectiveDirector] DISABLED Reason=NoSpawnPoints"));
		return;
	}
	if (!bClassesValid)
	{
		bStopped = true;
		UE_LOG(LogTemp, Error, TEXT("[ObjectiveDirector] DISABLED Reason=InvalidObjectiveClass"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] READY RunTime=%.1f SpawnPoints=%d Torii=OK Spawner=%s Classes=OK"), GetAuthoritativeRunTime(), SpawnPoints.Num(), *GetNameSafe(RunTimeSource));
#if !UE_BUILD_SHIPPING
	GetWorldTimerManager().SetTimer(StatusLogTimer, this, &ARunObjectiveDirector::LogRunTimeStatus, 15.0f, true);
#endif
	ScheduleMilestones();
}

void ARunObjectiveDirector::ScheduleMilestones()
{
	Milestones = {
		{BloodShrineSpawnTime, ERunObjectiveMilestoneType::BloodShrine},
		{GuaranteedCharacterTrialSpawnTime, ERunObjectiveMilestoneType::FirstCharacterTrialChoice},
		{SecondCharacterTrialSpawnTime, ERunObjectiveMilestoneType::SecondCharacterTrial},
		{TwinSoulTrialSpawnTime, ERunObjectiveMilestoneType::TwinSoulTrial}};
	MilestoneTimers.SetNum(Milestones.Num());
	UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	const float RunTime = GetAuthoritativeRunTime();
	for (int32 Index = 0; Index < Milestones.Num(); ++Index)
	{
		if (State->HasExecuted(Index)) continue;
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ARunObjectiveDirector::ExecuteMilestone, Index);
		GetWorldTimerManager().SetTimer(MilestoneTimers[Index], Delegate, FMath::Max(Milestones[Index].SpawnTime - RunTime, KINDA_SMALL_NUMBER), false);
	}
}

void ARunObjectiveDirector::ExecuteMilestone(int32 Index)
{
	ExecuteMilestoneInternal(Index, false);
}

void ARunObjectiveDirector::ExecuteMilestoneInternal(int32 Index, bool bIgnoreScheduleTime)
{
	if (bStopped || !Milestones.IsValidIndex(Index)) return;
	UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	if (State->HasExecuted(Index)) return;
	const float RemainingRunTime = Milestones[Index].SpawnTime - GetAuthoritativeRunTime();
	if (!bIgnoreScheduleTime && RemainingRunTime > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] MilestoneDeferred %s Target=%.1f Current=%.1f"), *UEnum::GetValueAsString(Milestones[Index].Type), Milestones[Index].SpawnTime, GetAuthoritativeRunTime());
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ARunObjectiveDirector::ExecuteMilestone, Index);
		GetWorldTimerManager().SetTimer(MilestoneTimers[Index], Delegate, FMath::Max(RemainingRunTime, 0.25f), false);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] MilestoneReached %s Target=%.1f Current=%.1f"), *UEnum::GetValueAsString(Milestones[Index].Type), Milestones[Index].SpawnTime, GetAuthoritativeRunTime());
	const FRunObjectiveMilestone& Milestone = Milestones[Index];
	switch (Milestone.Type)
	{
	case ERunObjectiveMilestoneType::BloodShrine:
		State->MarkExecuted(Index);
		SpawnObjective(BloodShrineClass, TEXT("BloodShrine"));
		break;
	case ERunObjectiveMilestoneType::FirstCharacterTrialChoice:
		if (ShowFirstTrialChoice())
		{
			State->MarkExecuted(Index);
		}
		else if (!bFirstTrialChoiceResolved && !bFirstTrialChoiceOpen)
		{
			FTimerDelegate Retry = FTimerDelegate::CreateUObject(this, &ARunObjectiveDirector::ExecuteMilestone, Index);
			GetWorldTimerManager().SetTimer(MilestoneTimers[Index], Retry, 0.25f, false);
		}
		break;
	case ERunObjectiveMilestoneType::SecondCharacterTrial:
		if (!bFirstTrialChoiceResolved)
		{
			FTimerDelegate Retry = FTimerDelegate::CreateUObject(this, &ARunObjectiveDirector::ExecuteMilestone, Index);
			GetWorldTimerManager().SetTimer(MilestoneTimers[Index], Retry, 0.25f, false);
			break;
		}
		State->MarkExecuted(Index);
		SpawnCharacterTrial(FirstSelectedTrial == ECharacterTrialType::Samurai
			? ECharacterTrialType::Ninja : ECharacterTrialType::Samurai, TEXT("SecondCharacterTrial"));
		break;
	case ERunObjectiveMilestoneType::TwinSoulTrial:
		State->MarkExecuted(Index);
		SpawnObjective(TwinSoulTrialClass, TEXT("TwinSoulTrial"));
		break;
	}
}

bool ARunObjectiveDirector::ShowFirstTrialChoice()
{
	if (bStopped || bFirstTrialChoiceResolved || bFirstTrialChoiceOpen || !PlayerController) return false;
	TrialChoiceWidget = CreateWidget<UTrialChoiceWidget>(PlayerController, UTrialChoiceWidget::StaticClass());
	if (!TrialChoiceWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[ObjectiveDirector] Failed to create first trial choice UI."));
		return false;
	}
	TrialChoiceWidget->InitializeTrialChoice(this);
	TrialChoiceWidget->AddToViewport(200);
	bFirstTrialChoiceOpen = true;
	PlayerController->BeginObjectiveChoiceInput(TrialChoiceWidget);
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] First trial choice opened."));
	return true;
}

bool ARunObjectiveDirector::ResolveFirstTrialChoice(ECharacterTrialType SelectedTrial)
{
	if (bStopped || bFirstTrialChoiceResolved || !bFirstTrialChoiceOpen
		|| (SelectedTrial != ECharacterTrialType::Samurai && SelectedTrial != ECharacterTrialType::Ninja))
	{
		return false;
	}

	FirstSelectedTrial = SelectedTrial;
	bFirstTrialChoiceResolved = true;
	CloseFirstTrialChoice(true);
	SpawnCharacterTrial(SelectedTrial, TEXT("FirstCharacterTrial"));
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] First trial choice resolved: %s"),
		SelectedTrial == ECharacterTrialType::Samurai ? TEXT("Samurai") : TEXT("Ninja"));
	return true;
}

void ARunObjectiveDirector::CloseFirstTrialChoice(bool bRestoreGameplay)
{
	if (TrialChoiceWidget)
	{
		TrialChoiceWidget->RemoveFromParent();
		TrialChoiceWidget = nullptr;
	}
	bFirstTrialChoiceOpen = false;
	if (bRestoreGameplay && PlayerController)
	{
		PlayerController->EndObjectiveChoiceInput();
	}
}

AActor* ARunObjectiveDirector::SpawnCharacterTrial(ECharacterTrialType TrialType, const TCHAR* ObjectiveName)
{
	return SpawnObjective(TrialType == ECharacterTrialType::Samurai
		? TSubclassOf<AActor>(SamuraiTrialClass) : TSubclassOf<AActor>(NinjaTrialClass), ObjectiveName);
}

AActor* ARunObjectiveDirector::SpawnObjective(TSubclassOf<AActor> ObjectiveClass, const TCHAR* ObjectiveName)
{
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] SpawnRequest Type=%s Time=%.1f"), ObjectiveName, GetAuthoritativeRunTime());
	if (!ObjectiveClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ObjectiveDirector] %s class is not configured."), ObjectiveName);
		return nullptr;
	}
	const TArray<AObjectiveSpawnPoint*> UnusedCandidates = GetUnusedCandidates();
	TArray<AObjectiveSpawnPoint*> Candidates = BuildPreferredCandidates();
	if (Candidates.IsEmpty()) Candidates = GetUnusedCandidates();
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] Candidates Type=%s Total=%d Used=%d Preferred=%d"), ObjectiveName, SpawnPoints.Num(), SpawnPoints.Num() - UnusedCandidates.Num(), Candidates.Num());
	ShufflePoints(Candidates);
	UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	for (AObjectiveSpawnPoint* Point : Candidates)
	{
		if (!IsValid(Point)) continue;
		// ObjectiveSpawnPoint uses a scaled editor billboard as its root component.
		// Preserve the marker's placement, but never propagate that visualization
		// scale into runtime objective actors.
		const FTransform ObjectiveSpawnTransform(
			Point->GetActorQuat(),
			Point->GetActorLocation(),
			FVector::OneVector);
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AActor* Spawned = GetWorld()->SpawnActor<AActor>(ObjectiveClass, ObjectiveSpawnTransform, Params);
		if (!Spawned)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ObjectiveDirector] Spawn blocked for %s at Point=%s; trying another point."), ObjectiveName, *GetNameSafe(Point));
			continue;
		}
		const FVector InitialSpawnedLocation = Spawned->GetActorLocation();
		if (ANinjaTechniqueTrial* NinjaTrial = Cast<ANinjaTechniqueTrial>(Spawned))
		{
			NinjaTrial->AlignEntranceToSpawnTransform(ObjectiveSpawnTransform);
		}
		if (!bReuseObjectiveSpawnPoints) State->MarkPointUsed(Point);
		State->TrackSpawnedObjective(Spawned);
		UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] Spawned %s Point=%s Time=%.1f Marker=%s Initial=%s Final=%s Scale=%s"),
			ObjectiveName,
			*GetNameSafe(Point),
			GetAuthoritativeRunTime(),
			*Point->GetActorLocation().ToCompactString(),
			*InitialSpawnedLocation.ToCompactString(),
			*Spawned->GetActorLocation().ToCompactString(),
			*Spawned->GetActorScale3D().ToCompactString());
		return Spawned;
	}
	UE_LOG(LogTemp, Error, TEXT("[ObjectiveDirector] ERROR FailedToSpawn %s after %d candidates."), ObjectiveName, Candidates.Num());
	return nullptr;
}

TArray<AObjectiveSpawnPoint*> ARunObjectiveDirector::GetUnusedCandidates() const
{
	TArray<AObjectiveSpawnPoint*> Result;
	const UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	for (AObjectiveSpawnPoint* Point : SpawnPoints) if (IsValid(Point) && (bReuseObjectiveSpawnPoints || !State->IsPointUsed(Point))) Result.Add(Point);
	return Result;
}

TArray<AObjectiveSpawnPoint*> ARunObjectiveDirector::BuildPreferredCandidates() const
{
	TArray<AObjectiveSpawnPoint*> Base = GetUnusedCandidates();
	TArray<AObjectiveSpawnPoint*> Both;
	for (AObjectiveSpawnPoint* Point : Base) if (MeetsPlayerDistance(Point) && MeetsObjectiveSeparation(Point)) Both.Add(Point);
	if (!Both.IsEmpty()) return Both;
	TArray<AObjectiveSpawnPoint*> PlayerOnly;
	for (AObjectiveSpawnPoint* Point : Base) if (MeetsPlayerDistance(Point)) PlayerOnly.Add(Point);
	if (!PlayerOnly.IsEmpty()) return PlayerOnly;
	TArray<AObjectiveSpawnPoint*> SeparationOnly;
	for (AObjectiveSpawnPoint* Point : Base) if (MeetsObjectiveSeparation(Point)) SeparationOnly.Add(Point);
	return SeparationOnly.IsEmpty() ? Base : SeparationOnly;
}

bool ARunObjectiveDirector::MeetsPlayerDistance(const AObjectiveSpawnPoint* Point) const
{
	const UCharacterManagerComponent* Manager = PlayerController ? PlayerController->GetCharacterManager() : nullptr;
	const ACharacterBase* Player = Manager ? Manager->GetActiveCharacter() : nullptr;
	if (!Player) return true;
	const float Distance = FVector::Dist2D(Point->GetActorLocation(), Player->GetActorLocation());
	return Distance >= ObjectiveSpawnMinDistanceFromPlayer && (ObjectiveSpawnMaxDistanceFromPlayer <= 0.0f || Distance <= ObjectiveSpawnMaxDistanceFromPlayer);
}

bool ARunObjectiveDirector::MeetsObjectiveSeparation(const AObjectiveSpawnPoint* Point) const
{
	if (ObjectiveMinSeparation <= 0.0f) return true;
	const UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	for (const TWeakObjectPtr<AActor>& Entry : State->SpawnedObjectives)
	{
		const AActor* Existing = Entry.Get();
		if (IsValid(Existing) && FVector::DistSquared2D(Point->GetActorLocation(), Existing->GetActorLocation()) < FMath::Square(ObjectiveMinSeparation)) return false;
	}
	return true;
}

void ARunObjectiveDirector::ShufflePoints(TArray<AObjectiveSpawnPoint*>& Points) const
{
	UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	for (int32 Index = Points.Num() - 1; Index > 0; --Index) Points.Swap(Index, State->RandRange(0, Index));
}

void ARunObjectiveDirector::HandleRunEnded() { bStopped = true; CloseFirstTrialChoice(false); CancelPendingMilestones(); }
void ARunObjectiveDirector::HandleBossTravelStarted() { bStopped = true; CloseFirstTrialChoice(false); CancelPendingMilestones(); }
void ARunObjectiveDirector::CancelPendingMilestones()
{
	GetWorldTimerManager().ClearTimer(InitializationRetryTimer);
	GetWorldTimerManager().ClearTimer(StatusLogTimer);
	for (FTimerHandle& Handle : MilestoneTimers) GetWorldTimerManager().ClearTimer(Handle);
}
void ARunObjectiveDirector::LogRunTimeStatus()
{
	const UObjectiveDirectorRunStateSubsystem* State = GetWorld()->GetSubsystem<UObjectiveDirectorRunStateSubsystem>();
	UE_LOG(LogTemp, Log, TEXT("[ObjectiveDirector] RunTime=%.1f Spawner=%s ExecutedMask=0x%02x UsedPoints=%d/%d"), GetAuthoritativeRunTime(), *GetNameSafe(RunTimeSource), State->GetExecutedMilestoneMask(), State->GetUsedPointCount(), SpawnPoints.Num());
}
float ARunObjectiveDirector::GetAuthoritativeRunTime() const { return RunTimeSource ? RunTimeSource->GetRunTimeSeconds() : 0.0f; }
void ARunObjectiveDirector::DebugSpawnBloodShrine() { ExecuteMilestoneInternal(0, true); }
void ARunObjectiveDirector::DebugSpawnGuaranteedCharacterTrial() { ExecuteMilestoneInternal(1, true); }
void ARunObjectiveDirector::DebugSpawnTwinSoulTrial() { ExecuteMilestoneInternal(3, true); }
void ARunObjectiveDirector::DebugTriggerAllObjectiveMilestones() { for (int32 Index = 0; Index < Milestones.Num(); ++Index) ExecuteMilestoneInternal(Index, true); }
