#include "HealingPickup.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

AHealingPickup::AHealingPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(SceneRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(SceneRoot);
	PickupCollision->InitSphereRadius(PickupCollisionRadius);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetCollisionObjectType(ECC_WorldDynamic);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollision->SetGenerateOverlapEvents(true);
	PickupCollision->OnComponentBeginOverlap.AddDynamic(this, &AHealingPickup::HandlePickupOverlap);
}

void AHealingPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (PickupCollision) PickupCollision->SetSphereRadius(FMath::Max(1.0f, PickupCollisionRadius));
}

void AHealingPickup::BeginPlay()
{
	Super::BeginPlay();
	InitialMeshRelativeLocation = PickupMesh ? PickupMesh->GetRelativeLocation() : FVector::ZeroVector;
}

void AHealingPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bConsumed || !PickupMesh) return;

	IdleElapsed += FMath::Max(0.0f, DeltaSeconds);
	if (bRotate && !FMath::IsNearlyZero(RotationRate))
	{
		PickupMesh->AddLocalRotation(FRotator(0.0f, RotationRate * DeltaSeconds, 0.0f));
	}
	if (bEnableBobbing && BobAmplitude > 0.0f && BobFrequency > 0.0f)
	{
		FVector BobbedLocation = InitialMeshRelativeLocation;
		BobbedLocation.Z += FMath::Sin(IdleElapsed * UE_TWO_PI * BobFrequency) * BobAmplitude;
		PickupMesh->SetRelativeLocation(BobbedLocation);
	}
}

void AHealingPickup::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bConsumed || HealAmount <= 0.0f || !IsValid(OtherActor)) return;

	ACharacterBase* PlayerCharacter = Cast<ACharacterBase>(OtherActor);
	ASurvivorPlayerController* PlayerController = PlayerCharacter
		? Cast<ASurvivorPlayerController>(PlayerCharacter->GetController()) : nullptr;
	UCharacterManagerComponent* CharacterManager = PlayerController ? PlayerController->GetCharacterManager() : nullptr;
	if (!PlayerCharacter || !CharacterManager || CharacterManager->GetActiveCharacter() != PlayerCharacter) return;

	UHealthComponent* PlayerHealth = PlayerController->GetPlayerHealthComponent();
	if (!PlayerHealth || PlayerHealth->IsDead()
		|| PlayerHealth->GetCurrentHealth() >= PlayerHealth->GetMaxHealth() - KINDA_SMALL_NUMBER) return;

	// Claim the pickup before spawning effects or healing so another overlap cannot consume it.
	bConsumed = true;
	PickupCollision->SetGenerateOverlapEvents(false);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetVisibility(false, true);

	const FVector PickupLocation = GetActorLocation();
	if (PickupBurstFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupBurstFX, PickupLocation, GetActorRotation());
	}
	if (HealOverlayMaterial && PlayerCharacter->GetMesh())
	{
		USkeletalMeshComponent* CharacterMesh = PlayerCharacter->GetMesh();
		UMaterialInterface* PreviousOverlayMaterial = CharacterMesh->GetOverlayMaterial();
		CharacterMesh->SetOverlayMaterial(HealOverlayMaterial);

		if (HealOverlayDuration <= 0.0f)
		{
			CharacterMesh->SetOverlayMaterial(PreviousOverlayMaterial);
		}
		else if (UWorld* World = GetWorld())
		{
			const TWeakObjectPtr<USkeletalMeshComponent> WeakCharacterMesh(CharacterMesh);
			const TWeakObjectPtr<UMaterialInterface> WeakHealingOverlay(HealOverlayMaterial);
			const TWeakObjectPtr<UMaterialInterface> WeakPreviousOverlay(PreviousOverlayMaterial);
			FTimerHandle OverlayTimer;
			World->GetTimerManager().SetTimer(OverlayTimer, FTimerDelegate::CreateLambda(
				[WeakCharacterMesh, WeakHealingOverlay, WeakPreviousOverlay]()
				{
					USkeletalMeshComponent* Mesh = WeakCharacterMesh.Get();
					if (Mesh && Mesh->GetOverlayMaterial() == WeakHealingOverlay.Get())
					{
						Mesh->SetOverlayMaterial(WeakPreviousOverlay.Get());
					}
				}), HealOverlayDuration, false);
		}
	}
	if (HealingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HealingSound, PickupLocation);
	}

	PlayerHealth->Heal(HealAmount);
	Destroy();
}
