#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GoblinBombEnemy.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "EnemyDeathComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoblinBombAttackTest, "HeavensDivide.Enemies.GoblinBomb",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGoblinBombAttackTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ASurvivorPlayerController* Controller = World->SpawnActor<ASurvivorPlayerController>();
	ACharacterBase* Player = World->SpawnActor<ACharacterBase>();
	Player->SetOwner(Controller);
	UCharacterManagerComponent* Manager = Controller->GetCharacterManager();
	FObjectProperty* ActivePlayer = FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"));
	ActivePlayer->SetObjectPropertyValue_InContainer(Manager, Player);
	UHealthComponent* PlayerHealth = Controller->GetPlayerHealthComponent();
	PlayerHealth->RestoreCurrentHealth(100.0f);

	auto SpawnBomb = [&]()
	{
		AGoblinBombEnemy* Bomb = World->SpawnActor<AGoblinBombEnemy>();
		UStaticMeshComponent* BombMesh = NewObject<UStaticMeshComponent>(Bomb, TEXT("Bomb"));
		Bomb->AddInstanceComponent(BombMesh);
		BombMesh->SetupAttachment(Bomb->GetMesh());
		BombMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Assets/EnemyCharacters/Goblin/Bomb.Bomb")));
		BombMesh->RegisterComponent();
		BombMesh->SetMaterial(0, Bomb->BombFlashMaterial);
		Bomb->SetActorLocation(FVector(0, 0, 0));
		Player->SetActorLocation(FVector(100, 0, 0));
		Bomb->SetTarget(Player);
		Bomb->ObservedCharacterManager = Manager;
		Bomb->CachedSurvivorController = Controller;
		Bomb->bDropsXP = false;
		Bomb->TelegraphWindupDuration = 1.0f;
		Bomb->EnemyDeathComponent->DeathNiagaraSystem = nullptr;
		Bomb->EnemyDeathComponent->DeathSound = nullptr;
		Bomb->GetHealthComponent()->OnDeath.AddUniqueDynamic(Bomb, &AGoblinBombEnemy::HandleDeath);
		return Bomb;
	};
	AGoblinBombEnemy* Bomb = SpawnBomb();
	TestFalse(TEXT("Bomb has no contact damage"), Bomb->UsesContactDamage());
	Bomb->StartAttack();
	TestTrue(TEXT("Charge pauses the walking animation"), Bomb->GetMesh()->bPauseAnims);
	TestTrue(TEXT("Bomb flash material is applied to its slot"), Bomb->ChargeBombMesh && Bomb->ChargeBombMesh->GetMaterial(0) == Bomb->BombFlashMID);
	TestTrue(TEXT("Nearby player starts charge without a montage"), Bomb->bIsAttacking);
	TestTrue(TEXT("Shared AoE decal becomes visible"), Bomb->AttackTelegraphDecal->IsVisible());
	TestTrue(TEXT("Fuse timer is scheduled"), World->GetTimerManager().IsTimerActive(Bomb->DetonationTimer));
	TestNotNull(TEXT("Attached Bomb receives the red flash material"), Bomb->BombFlashMID.Get());
	TestTrue(TEXT("Charge visuals run only during the fuse"), World->GetTimerManager().IsTimerActive(Bomb->ChargePresentationTimer));
	if (Bomb->BombFlashMID)
	{
		TestTrue(TEXT("Bomb starts with visible flash"), Bomb->bBombFlashOn);
		TestEqual(TEXT("Bomb flash is red"), Bomb->BombFlashMID->K2_GetVectorParameterValue(TEXT("FlashColor")), Bomb->BombFlashColor);
	}
	Bomb->PerformAttackHit();
	TestEqual(TEXT("Animation notifies cannot detonate early"), PlayerHealth->GetCurrentHealth(), 100.0f);
	TestFalse(TEXT("Bomb survives until fuse expires"), Bomb->IsDead());
	// Tick the real timer, including its first-frame pending registration.
	++GFrameCounter;
	World->GetTimerManager().Tick(0.0f);
	++GFrameCounter;
	World->GetTimerManager().Tick(0.5f);
	TestFalse(TEXT("Charge does not finish halfway through"), Bomb->IsDead());
	++GFrameCounter;
	World->GetTimerManager().Tick(0.51f);
	TestEqual(TEXT("Explosion applies shared AoE damage once"), PlayerHealth->GetCurrentHealth(), 90.0f);
	TestTrue(TEXT("Detonation reduces enemy HP to zero"), Bomb->GetHealthComponent()->IsDead());
	TestTrue(TEXT("Detonation uses authoritative death and immediate cleanup"), Bomb->IsDead() && Bomb->IsActorBeingDestroyed());
	Bomb->Detonate();
	TestEqual(TEXT("Repeated detonation cannot double-hit"), PlayerHealth->GetCurrentHealth(), 90.0f);

	AGoblinBombEnemy* Escaped = SpawnBomb();
	Escaped->StartAttack();
	Player->SetActorLocation(FVector(2000, 0, 0));
	Escaped->Detonate();
	TestTrue(TEXT("Leaving the circle does not cancel a committed explosion"), Escaped->IsDead());
	TestEqual(TEXT("Player outside the AoE takes no damage"), PlayerHealth->GetCurrentHealth(), 90.0f);

	AGoblinBombEnemy* Killed = SpawnBomb();
	const FQuat OriginalRotation = Killed->GetMesh()->GetRelativeRotation().Quaternion();
	Killed->StartAttack();
	UStaticMeshComponent* KilledBombMesh = Killed->ChargeBombMesh;
	// Simulate the temporary visual rotation before cancellation.
	Killed->GetMesh()->SetRelativeRotation(FRotator(7, 0, 5));
	Killed->GetHealthComponent()->ApplyDamage(1000.0f);
	TestTrue(TEXT("Early death restores the mesh rotation"), Killed->GetMesh()->GetRelativeRotation().Quaternion().Equals(OriginalRotation));
	TestTrue(TEXT("Early death restores the bomb overlay"), KilledBombMesh && KilledBombMesh->GetOverlayMaterial() == nullptr);
	TestTrue(TEXT("Death presentation keeps the corpse animation paused"), Killed->GetMesh()->bPauseAnims);
	TestTrue(TEXT("Early death restores the bomb material"), KilledBombMesh->GetMaterial(0) != Killed->BombFlashMID);
	TestFalse(TEXT("Early death cancels charge visuals"), World->GetTimerManager().IsTimerActive(Killed->ChargePresentationTimer));
	TestFalse(TEXT("Killing the charging bomb cancels its fuse"), World->GetTimerManager().IsTimerActive(Killed->DetonationTimer));
	TestFalse(TEXT("Killed bomb hides its decal"), Killed->AttackTelegraphDecal->IsVisible());
	Killed->Detonate();
	TestFalse(TEXT("Early death never becomes a detonation"), Killed->bDetonated);
	TestTrue(TEXT("Early death keeps the standard dissolve"), Killed->EnemyDeathComponent->IsComponentTickEnabled());
	TestEqual(TEXT("Early death does not damage player"), PlayerHealth->GetCurrentHealth(), 90.0f);

	AGoblinBombEnemy* Suspended = SpawnBomb();
	Suspended->StartAttack();
	Suspended->StopChargePresentation();
	TestFalse(TEXT("Cancelling charge restores prior animation playback"), Suspended->GetMesh()->bPauseAnims);
	Suspended->StartChargePresentation();
	Suspended->SetGameplaySuspended(true);
	TestFalse(TEXT("Suspending gameplay cancels the fuse"), World->GetTimerManager().IsTimerActive(Suspended->DetonationTimer));
	Suspended->StartAttack();
	TestFalse(TEXT("Suspended enemy cannot restart charging"), Suspended->bIsAttacking);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif

