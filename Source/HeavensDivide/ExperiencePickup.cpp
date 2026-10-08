// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExperiencePickup.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "ExperienceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"

AExperiencePickup::AExperiencePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(Root);
	PickupCollision->InitSphereRadius(AttractionRadius);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetCollisionObjectType(ECC_WorldDynamic);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	PickupCollision->SetGenerateOverlapEvents(true);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Root);
}

void AExperiencePickup::BeginPlay()
{
	Super::BeginPlay();

	AttractionRadius = FMath::Max(1.0f, AttractionRadius);
	PickupRadius = FMath::Clamp(PickupRadius, 1.0f, AttractionRadius);
	BasePickupRadius = PickupRadius;
	BaseAttractionRadius = AttractionRadius;
	XPValue = FMath::Max(0, XPValue);

	if (PickupCollision)
	{
		PickupCollision->OnComponentBeginOverlap.AddDynamic(this, &AExperiencePickup::HandlePickupOverlap);
	}

	CacheSharedPlayerState();
	ApplySharedPickupRadiusStats();
	if (SharedPlayerStats)
	{
		SharedPlayerStats->OnStatsChanged.AddDynamic(this, &AExperiencePickup::HandleSharedPlayerStatsChanged);
	}
}

void AExperiencePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SharedPlayerStats)
	{
		SharedPlayerStats->OnStatsChanged.RemoveDynamic(this, &AExperiencePickup::HandleSharedPlayerStatsChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AExperiencePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bRewardFlight)
	{
		FlightElapsed += DeltaSeconds;
		const float T=FMath::Clamp(FlightElapsed/FlightDuration,0.f,1.f);
		SetActorLocation(FMath::Lerp(FlightStart,FlightLanding,T)+FVector(0,0,4*T*(1-T)*FlightHeight));
		if(T>=1){bRewardFlight=false;PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);CheckInitialActiveCharacterProximity();if(!bAttracting)SetActorTickEnabled(false);}
		return;
	}

	if (!bAttracting || bCollected || DeltaSeconds <= 0.0f)
	{
		return;
	}

	ACharacterBase* ActiveCharacter = GetActiveCharacter();
	if (!ActiveCharacter)
	{
		return;
	}

	// A short, planar kick away from the player gives the pull a readable anticipation.
	// Consume only this phase's time so hitches do not delay the homing phase.
	float RemainingTime = DeltaSeconds;
	if (BounceElapsed < BounceDuration)
	{
		const float BounceStep = FMath::Min(RemainingTime, BounceDuration - BounceElapsed);
		BounceElapsed += BounceStep;
		RemainingTime -= BounceStep;
		const float Alpha = FMath::Clamp(BounceElapsed / BounceDuration, 0.0f, 1.0f);
		const float EaseOut = 1.0f - FMath::Square(1.0f - Alpha);
		SetActorLocation(BounceStart + BounceDirection * FMath::Max(0.0f, BounceDistance) * EaseOut);
		if (RemainingTime <= 0.0f)
		{
			return;
		}
	}

	const FVector CurrentLocation = GetActorLocation();
	FVector ToPlayer = ActiveCharacter->GetActorLocation() - CurrentLocation;
	const float Distance = ToPlayer.Size();

	if (Distance <= PickupRadius)
	{
		Collect();
		return;
	}

	if (!ToPlayer.Normalize())
	{
		return;
	}

	const float MaxSpeed = FMath::Max(0.0f, AttractionSpeed) * FMath::Max(1.0f, MaxAttractionSpeedMultiplier);
	const float Acceleration = FMath::Max(0.0f, AttractionAcceleration);
	const float AcceleratingTime = Acceleration > 0.0f
		? FMath::Min(RemainingTime, FMath::Max(0.0f, MaxSpeed - CurrentAttractionSpeed) / Acceleration)
		: 0.0f;
	const float NextSpeed = FMath::Min(MaxSpeed, CurrentAttractionSpeed + Acceleration * AcceleratingTime);
	const float TravelDistance = (CurrentAttractionSpeed + NextSpeed) * 0.5f * AcceleratingTime
		+ NextSpeed * (RemainingTime - AcceleratingTime);
	CurrentAttractionSpeed = NextSpeed;
	const float StepDistance = FMath::Min(TravelDistance, Distance);
	SetActorLocation(CurrentLocation + ToPlayer * StepDistance, false, nullptr, ETeleportType::None);
	if (Distance - StepDistance <= PickupRadius)
	{
		Collect();
	}
}

void AExperiencePickup::LaunchFromChest(FVector Landing,float Duration,float Height)
{
	bRewardFlight=true;bAttracting=false;FlightStart=GetActorLocation();FlightLanding=Landing;
	FlightDuration=FMath::Max(.1f,Duration);FlightHeight=Height;FlightElapsed=0;
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);SetActorTickEnabled(true);
}

void AExperiencePickup::InitializePickup(int32 InXPValue, UExperienceComponent* InExperienceComponent, UCharacterManagerComponent* InCharacterManager)
{
	XPValue = FMath::Max(0, InXPValue);
	ExperienceComponent = InExperienceComponent;
	CharacterManager = InCharacterManager;
	CacheSharedPlayerState();
	ApplySharedPickupRadiusStats();
	CheckInitialActiveCharacterProximity();
}

void AExperiencePickup::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bRewardFlight || bCollected || bAttracting || !IsActiveCharacter(OtherActor))
	{
		return;
	}

	BeginAttraction();
}

void AExperiencePickup::HandleSharedPlayerStatsChanged()
{
	ApplySharedPickupRadiusStats();
	CheckInitialActiveCharacterProximity();
}

bool AExperiencePickup::IsActiveCharacter(AActor* Actor) const
{
	return Actor && Actor == GetActiveCharacter();
}

ACharacterBase* AExperiencePickup::GetActiveCharacter() const
{
	return CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
}

void AExperiencePickup::BeginAttraction()
{
	const ACharacterBase* ActiveCharacter = GetActiveCharacter();
	if (bAttracting || bCollected || bRewardFlight || !ActiveCharacter)
	{
		return;
	}

	bAttracting = true;
	BounceStart = GetActorLocation();
	BounceDirection = (BounceStart - ActiveCharacter->GetActorLocation()).GetSafeNormal2D();
	if (BounceDirection.IsNearlyZero())
	{
		BounceDirection = -ActiveCharacter->GetActorForwardVector().GetSafeNormal2D();
	}
	BounceElapsed = 0.0f;
	CurrentAttractionSpeed = FMath::Max(0.0f, AttractionSpeed);
	// Once claimed, keep following the active character even outside the initial radius.
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(true);
}

void AExperiencePickup::Collect()
{
	if (bCollected)
	{
		return;
	}

	bCollected = true;
	SetActorTickEnabled(false);

	if (PickupCollision)
	{
		PickupCollision->SetGenerateOverlapEvents(false);
		PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (ExperienceComponent && XPValue > 0)
	{
		ExperienceComponent->AddXP(XPValue);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ExperiencePickup %s could not award XP. ExperienceComponent=%s XPValue=%d"),
			*GetNameSafe(this),
			*GetNameSafe(ExperienceComponent),
			XPValue);
	}
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	Destroy();
}

void AExperiencePickup::CacheSharedPlayerState()
{
	if (ExperienceComponent && CharacterManager && SharedPlayerStats)
	{
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!SurvivorController)
	{
		return;
	}

	if (!ExperienceComponent)
	{
		ExperienceComponent = SurvivorController->GetExperienceComponent();
	}

	if (!CharacterManager)
	{
		CharacterManager = SurvivorController->GetCharacterManager();
	}

	if (!SharedPlayerStats)
	{
		SharedPlayerStats = SurvivorController->GetSharedPlayerStats();
	}
}

void AExperiencePickup::ApplySharedPickupRadiusStats()
{
	if (BasePickupRadius <= 0.0f)
	{
		BasePickupRadius = PickupRadius;
	}

	if (BaseAttractionRadius <= 0.0f)
	{
		BaseAttractionRadius = AttractionRadius;
	}

	const float PickupRadiusMultiplier = SharedPlayerStats ? SharedPlayerStats->GetFinalPickupRadiusMultiplier() : 1.0f;
	PickupRadius = FMath::Max(1.0f, BasePickupRadius * PickupRadiusMultiplier);
	AttractionRadius = FMath::Max(PickupRadius, BaseAttractionRadius * PickupRadiusMultiplier);

	if (PickupCollision)
	{
		PickupCollision->SetSphereRadius(AttractionRadius);
	}
}

void AExperiencePickup::CheckInitialActiveCharacterProximity()
{
	if (bRewardFlight || bCollected || bAttracting)
	{
		return;
	}

	CacheSharedPlayerState();

	const ACharacterBase* ActiveCharacter = GetActiveCharacter();
	if (!ActiveCharacter)
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(GetActorLocation(), ActiveCharacter->GetActorLocation());
	if (DistanceSquared <= FMath::Square(AttractionRadius))
	{
		BeginAttraction();
	}
}
