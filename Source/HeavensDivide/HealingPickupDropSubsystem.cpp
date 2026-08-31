#include "HealingPickupDropSubsystem.h"

#include "EnemyBase.h"
#include "Engine/World.h"
#include "HealingPickup.h"
#include "HealingPickupDropSettings.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"

void UHealingPickupDropSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PityStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UHealingPickupDropSubsystem::NotifyEnemyDied(AEnemyBase* Enemy)
{
	UWorld* World = GetWorld();
	const UHealingPickupDropSettings* Settings = GetDefault<UHealingPickupDropSettings>();
	if (!World || !Settings || !IsValid(Enemy) || Enemy->IsStressTestEnemy()) return;

	ASurvivorPlayerController* PlayerController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	UHealthComponent* PlayerHealth = PlayerController ? PlayerController->GetPlayerHealthComponent() : nullptr;
	if (!PlayerHealth || PlayerHealth->IsDead()) return;

	const float HealthPercent = PlayerHealth->GetHealthPercent();
	if (HealthPercent >= 1.0f - KINDA_SMALL_NUMBER) return;

	PruneActivePickups();
	if (Settings->MaximumActivePickups <= 0 || ActivePickups.Num() >= Settings->MaximumActivePickups) return;

	const float Now = World->GetTimeSeconds();
	if (LastSuccessfulSpawnTime >= 0.0f && Now - LastSuccessfulSpawnTime < Settings->HealDropCooldown) return;

	float DropChance = GetBaseDropChance(Enemy);
	const bool bGuaranteed = DropChance >= 1.0f;
	if (!bGuaranteed && HealthPercent < Settings->LowHealthThreshold)
	{
		DropChance *= Settings->LowHealthDropMultiplier;
	}
	if (!bGuaranteed && HealthPercent < Settings->HealPityHealthThreshold
		&& Now - PityStartTime >= Settings->HealPityDelay)
	{
		DropChance *= Settings->HealPityMultiplier;
	}
	DropChance = FMath::Clamp(DropChance, 0.0f, 1.0f);
	if (FMath::FRand() > DropChance) return;

	UClass* PickupClass = Settings->HealingPickupClass.IsNull()
		? AHealingPickup::StaticClass()
		: Settings->HealingPickupClass.LoadSynchronous();
	if (!PickupClass) return;

	const FVector SpawnLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, Settings->SpawnVerticalOffset);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AHealingPickup* Pickup = World->SpawnActor<AHealingPickup>(PickupClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (!Pickup) return;

	ActivePickups.Add(Pickup);
	Pickup->OnDestroyed.AddUniqueDynamic(this, &UHealingPickupDropSubsystem::HandlePickupDestroyed);
	LastSuccessfulSpawnTime = Now;
	PityStartTime = Now;
}

void UHealingPickupDropSubsystem::HandlePickupDestroyed(AActor* DestroyedActor)
{
	ActivePickups.RemoveAll([DestroyedActor](const TWeakObjectPtr<AHealingPickup>& Pickup)
	{
		return !Pickup.IsValid() || Pickup.Get() == DestroyedActor;
	});
}

void UHealingPickupDropSubsystem::PruneActivePickups()
{
	ActivePickups.RemoveAll([](const TWeakObjectPtr<AHealingPickup>& Pickup) { return !Pickup.IsValid(); });
}

float UHealingPickupDropSubsystem::GetBaseDropChance(const AEnemyBase* Enemy) const
{
	const UHealingPickupDropSettings* Settings = GetDefault<UHealingPickupDropSettings>();
	if (!Enemy || !Settings) return 0.0f;

	switch (Enemy->GetDropCategory())
	{
	case EEnemyDropCategory::Elite: return Settings->EliteDropChance;
	case EEnemyDropCategory::Boss: return Settings->BossDropChance;
	default: return Settings->NormalDropChance;
	}
}
