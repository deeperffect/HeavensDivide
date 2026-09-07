#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnemyBase.h"
#include "EnemyLightweightMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "AutoAttackComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"
#include "HealthComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyPushbackTest, "HeavensDivide.Combat.SamuraiPushback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyPushbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
	UEnemyLightweightMovementComponent* Movement = Enemy->FindComponentByClass<UEnemyLightweightMovementComponent>();
	Movement->SetMovementEnabled(true);
	Enemy->SetActorLocation(FVector(0, 0, 100));
	Movement->RefreshSpawnZ();
	const FVector Origin(-100, 0, 100);
	Enemy->ApplyAttackPushback(Origin, EPlayerAttackSource::Ninja, 25, 0.1f);
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Ninja cannot cause pushback"), Enemy->GetActorLocation().Equals(FVector(0, 0, 100)));
	Enemy->ApplyAttackPushback(Origin, EPlayerAttackSource::Samurai, 25, 0.1f);
	Movement->StopMovement();
	TestTrue(TEXT("AI stopping pursuit does not disable pushback"), Movement->IsComponentTickEnabled());
	Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	const float FirstStep = Enemy->GetActorLocation().X;
	Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Push covers authored distance"), FMath::IsNearlyEqual(Enemy->GetActorLocation().X, 25.0, 0.01));
	TestTrue(TEXT("Push eases out"), FirstStep > 12.5f && FirstStep < 25.0f);
	TestEqual(TEXT("Push is horizontal"), Enemy->GetActorLocation().Z, 100.0);
	Enemy->SetActorLocation(FVector(0, 0, 100));
	Enemy->ApplyAttackPushback(Origin, EPlayerAttackSource::Samurai, 25, 0.1f);
	Enemy->ApplyAttackPushback(Origin, EPlayerAttackSource::Samurai, 25, 0.1f);
	Movement->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Repeated hits do not add impulses"), FMath::IsNearlyEqual(Enemy->GetActorLocation().X, 25.0, 0.01));
	Enemy->SetActorLocation(FVector(0, 0, 100));
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(5, 200, 200));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Wall->SetActorLocation(FVector(Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + 15, 0, 100));
	Enemy->ApplyAttackPushback(Origin, EPlayerAttackSource::Samurai, 25, 0.1f);
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Push stops before wall"), Enemy->GetActorLocation().X >= 0 && Enemy->GetActorLocation().X <= 10.01);
	Movement->SetMovementEnabled(false);
	const FVector StoppedLocation = Enemy->GetActorLocation();
	Enemy->ApplyAttackPushback(Origin, EPlayerAttackSource::Samurai, 25, 0.1f);
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Disabled movement cannot push"), Enemy->GetActorLocation().Equals(StoppedLocation));
	Wall->Destroy();
	ASurvivorPlayerController* PC = World->SpawnActor<ASurvivorPlayerController>();
	ASamuraiCharacter* Samurai = World->SpawnActor<ASamuraiCharacter>();
	Samurai->SetOwner(PC);
	UAutoAttackComponent* Attack = Samurai->FindComponentByClass<UAutoAttackComponent>();
	Attack->OwnerCharacter = Samurai;
	Attack->ImpactFeedback.bEnableCameraShake = false;
	UUpgradeDefinition* DoubleCut = NewObject<UUpgradeDefinition>();
	DoubleCut->UpgradeId = TEXT("TestDoubleCut");
	DoubleCut->SpecialEffects.Add(EUpgradeSpecialEffect::DoubleCut);
	PC->GetPlayerUpgrades()->DebugForceAcquireUpgrade(DoubleCut);
	Movement->SetMovementEnabled(true);
	const FVector TargetPosition = Samurai->GetActorLocation() + Samurai->GetVisualForwardVector() * Attack->AttackForwardOffset;
	const auto TestStrike = [&](const TCHAR* Label, bool bExpectPush)
	{
		Movement->CancelPushback();
		Movement->StopMovement();
		Enemy->SetActorLocation(TargetPosition);
		Enemy->GetHealthComponent()->RestoreCurrentHealth(100);
		Attack->ExecuteMeleeAttackTrace();
		TestTrue(TEXT("Strike still damages target"), Enemy->GetHealthComponent()->GetCurrentHealth() < 100);
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
		TestEqual(Label, !Enemy->GetActorLocation().Equals(TargetPosition, 0.01), bExpectPush);
	};
	TestStrike(TEXT("Ordinary strike keeps pushback"), true);
	Attack->DoubleCutPrimaryAttackCounter = Attack->DoubleCutPrimaryAttackCount - 1;
	TestStrike(TEXT("Strike earning Double Cut does not push"), false);
	Attack->DoubleCutPrimaryAttackCounter = 0;
	Attack->bDoubleCutReady = true;
	TestStrike(TEXT("Stored-ready first strike does not push"), false);
	Attack->bDoubleCutFollowUpActive = true;
	TestStrike(TEXT("Double Cut second strike pushes"), true);
	Movement->CancelPushback();
	Movement->StopMovement();
	Enemy->SetActorLocation(TargetPosition);
	Enemy->GetHealthComponent()->RestoreCurrentHealth(100);
	Attack->ExecuteDeathblow(nullptr, TargetPosition - FVector(50, 0, 0), 10, EPlayerAttackSource::Samurai, false);
	TestTrue(TEXT("Deathblow still damages target"), Enemy->GetHealthComponent()->GetCurrentHealth() < 100);
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Deathblow does not push"), Enemy->GetActorLocation().Equals(TargetPosition, 0.01));
	World->DestroyWorld(false);
	return true;
}
#endif
