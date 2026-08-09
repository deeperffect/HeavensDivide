// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyHealthBarWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	HealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidgetComponent"));
	HealthBarWidgetComponent->SetupAttachment(RootComponent);
	HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidgetComponent->SetDrawSize(HealthBarDrawSize);
	HealthBarWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AEnemyBase::HandleDeath);
	}

	InitializeHealthBar();
	InitializeTargetFromCharacterManager();
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObservedCharacterManager)
	{
		ObservedCharacterManager->OnCharacterSwapped.RemoveDynamic(this, &AEnemyBase::HandlePlayerCharacterSwapped);
	}

	StopEnemyBehavior();

	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateEnemyBehavior(DeltaSeconds);
}

void AEnemyBase::SetTarget(AActor* NewTarget)
{
	CurrentTarget = NewTarget;
}

AActor* AEnemyBase::GetTarget() const
{
	return CurrentTarget;
}

bool AEnemyBase::IsDead() const
{
	return bIsDead;
}

UHealthComponent* AEnemyBase::GetHealthComponent() const
{
	return HealthComponent;
}

void AEnemyBase::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentTarget = nullptr;
	StopEnemyBehavior();

	if (HealthBarWidgetComponent)
	{
		HealthBarWidgetComponent->SetHiddenInGame(true);
		HealthBarWidgetComponent->SetVisibility(false);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	SetActorEnableCollision(false);
	OnEnemyDeath();

	if (!DeathMontage)
	{
		DestroyAfterDeath();
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy death montage skipped: AnimInstance invalid for %s"), *GetNameSafe(this));
		DestroyAfterDeath();
		return;
	}

	const float PlayResult = AnimInstance->Montage_Play(DeathMontage);
	UE_LOG(LogTemp, Log, TEXT("Enemy death montage plays: %s Result=%.3f"), *GetNameSafe(DeathMontage), PlayResult);

	if (PlayResult <= 0.0f)
	{
		DestroyAfterDeath();
		return;
	}

	FOnMontageEnded DeathMontageEndedDelegate;
	DeathMontageEndedDelegate.BindUObject(this, &AEnemyBase::HandleDeathMontageEnded);
	AnimInstance->Montage_SetEndDelegate(DeathMontageEndedDelegate, DeathMontage);
}

void AEnemyBase::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != DeathMontage)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Enemy death montage finished: %s"), *GetNameSafe(this));
	Destroy();
}

void AEnemyBase::DestroyAfterDeath()
{
	if (DeathDestroyDelay > 0.0f)
	{
		SetLifeSpan(DeathDestroyDelay);
	}
	else
	{
		Destroy();
	}
}

void AEnemyBase::HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	SetTarget(NewCharacter);
}

void AEnemyBase::InitializeTargetFromCharacterManager()
{
	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!SurvivorController)
	{
		return;
	}

	ObservedCharacterManager = SurvivorController->GetCharacterManager();
	if (!ObservedCharacterManager)
	{
		return;
	}

	SetTarget(ObservedCharacterManager->GetActiveCharacter());
	ObservedCharacterManager->OnCharacterSwapped.AddDynamic(this, &AEnemyBase::HandlePlayerCharacterSwapped);
}

void AEnemyBase::InitializeHealthBar()
{
	if (!HealthBarWidgetComponent)
	{
		return;
	}

	HealthBarWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, HealthBarHeightOffset));
	HealthBarWidgetComponent->SetDrawSize(HealthBarDrawSize);

	if (HealthBarWidgetClass)
	{
		HealthBarWidgetComponent->SetWidgetClass(HealthBarWidgetClass);
	}

	HealthBarWidgetComponent->InitWidget();

	UEnemyHealthBarWidget* HealthBarWidget = Cast<UEnemyHealthBarWidget>(HealthBarWidgetComponent->GetUserWidgetObject());
	if (!HealthBarWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyBase %s has no EnemyHealthBarWidget assigned."), *GetNameSafe(this));
		return;
	}

	HealthBarWidget->InitializeFromHealthComponent(HealthComponent);
	HealthBarWidgetComponent->SetHiddenInGame(false);
	HealthBarWidgetComponent->SetVisibility(true);
}

void AEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (bIsDead || !CurrentTarget || IsPlayerTargetDead() || ShouldSkipMovement())
	{
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		return;
	}

	MoveTowardCurrentTarget();
}

bool AEnemyBase::ShouldSkipMovement() const
{
	return false;
}

void AEnemyBase::StopEnemyBehavior()
{
}

void AEnemyBase::MoveTowardCurrentTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.SizeSquared2D() <= FMath::Square(StopDistance))
	{
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		return;
	}

	if (ToTarget.Normalize())
	{
		AddMovementInput(ToTarget);
	}
}

void AEnemyBase::FaceTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.Normalize())
	{
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}

bool AEnemyBase::IsPlayerTargetDead() const
{
	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	return SurvivorController && SurvivorController->IsPlayerDead();
}
