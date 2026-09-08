#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyHitFlashTest, "HeavensDivide.ImpactFeedback.EnemyHitFlash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyHitFlashTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
	Enemy->InitializeHitFlash();
	UHealthComponent* Health = Enemy->GetHealthComponent();
	Health->RestoreCurrentHealth(100.0f);
	TestNotNull(TEXT("Default flash material is assigned"), Enemy->HitFlashMID.Get());
	UMaterialInstanceDynamic* ExistingOverlay = UMaterialInstanceDynamic::Create(Enemy->HitFlashMaterial, Enemy);
	Enemy->GetMesh()->SetOverlayMaterial(ExistingOverlay);
	TestTrue(TEXT("Bleed still deals damage"), Enemy->ApplyStatusDamage(1, EPlayerAttackSource::Samurai));
	TestTrue(TEXT("Poison still deals damage"), Enemy->ApplyStatusDamage(1, EPlayerAttackSource::Ninja));
	TestTrue(TEXT("Status ticks do not flash"), Enemy->GetMesh()->GetOverlayMaterial() == ExistingOverlay);
	TestFalse(TEXT("Status ticks do not start flash timer"), World->GetTimerManager().IsTimerActive(Enemy->HitFlashTimer));
	Enemy->ApplyPlayerDamage(10, EPlayerAttackSource::Samurai);
	TestTrue(TEXT("Accepted damage starts material flash"), Enemy->GetMesh()->GetOverlayMaterial() == Enemy->HitFlashMID);
	Enemy->ApplyStatusDamage(1, EPlayerAttackSource::Ninja);
	TestTrue(TEXT("Tick does not remove an existing direct-hit flash"), Enemy->GetMesh()->GetOverlayMaterial() == Enemy->HitFlashMID);
	Enemy->ApplyPlayerDamage(10, EPlayerAttackSource::Ninja);
	TestTrue(TEXT("Repeated hit preserves prior overlay"), Enemy->PreHitFlashOverlay == ExistingOverlay);
	TestTrue(TEXT("Flash timer is active"), World->GetTimerManager().IsTimerActive(Enemy->HitFlashTimer));
	Enemy->EndHitFlash();
	TestTrue(TEXT("Original overlay restored"), Enemy->GetMesh()->GetOverlayMaterial() == ExistingOverlay);
	Health->SetDamageEnabled(false);
	Enemy->ApplyPlayerDamage(10, EPlayerAttackSource::Samurai);
	TestTrue(TEXT("Rejected damage does not flash"), Enemy->GetMesh()->GetOverlayMaterial() == ExistingOverlay);
	Health->SetDamageEnabled(true);
	Enemy->ApplyPlayerDamage(10, EPlayerAttackSource::Samurai);
	Enemy->ApplyStatusDamage(1000, EPlayerAttackSource::Samurai);
	TestTrue(TEXT("Lethal damage clears flash"), Enemy->GetMesh()->GetOverlayMaterial() != Enemy->HitFlashMID);
	TestFalse(TEXT("Lethal damage cancels timer"), World->GetTimerManager().IsTimerActive(Enemy->HitFlashTimer));
	World->DestroyWorld(false);
	return true;
}
#endif
