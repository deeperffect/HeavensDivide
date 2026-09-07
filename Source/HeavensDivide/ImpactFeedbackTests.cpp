#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ImpactFeedback.h"
#include "AutoAttackComponent.h"
#include "EnemyBase.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "AttackProjectileBase.h"
#include "HealthComponent.h"
#include "HeavensDivideGameUserSettings.h"
#include "SurvivorPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactFeedbackTest, "HeavensDivide.ImpactFeedback.CombatAndSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactFeedbackTest::RunTest(const FString& Parameters)
{
	UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
	if (!TestNotNull(TEXT("Existing settings class"), Settings)) return false;
	const float OriginalIntensity = Settings->GetCameraShakeIntensity();
	const FString OriginalIni = GGameUserSettingsIni;
	// Exercise real save/load against an isolated file, never the user's preferences.
	GGameUserSettingsIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ImpactFeedbackTest.ini"));
	Settings->SetCameraShakeIntensity(0.4f);
	Settings->SaveSettings();
	GConfig->Flush(false, GGameUserSettingsIni);
	UHeavensDivideGameUserSettings* Reloaded = NewObject<UHeavensDivideGameUserSettings>();
	Reloaded->LoadConfig(nullptr, *GGameUserSettingsIni);
	TestEqual(TEXT("40 percent survives reload"), Reloaded->GetCameraShakeIntensity(), 0.4f);
	Settings->SetCameraShakeIntensity(-1.0f);
	TestEqual(TEXT("Lower clamp"), Settings->GetCameraShakeIntensity(), 0.0f);
	Settings->SetCameraShakeIntensity(2.0f);
	TestEqual(TEXT("Upper clamp"), Settings->GetCameraShakeIntensity(), 1.0f);

	const UWorld::InitializationValues WorldValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldValues);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASurvivorPlayerController* PC = World->SpawnActor<ASurvivorPlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine);
	if (!PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		PC->PlayerCameraManager->InitializeFor(PC);
	}
	Settings->OnCameraShakeIntensityChanged.AddUObject(PC, &ASurvivorPlayerController::HandleCameraShakeIntensityChanged);
	Settings->SetCameraShakeIntensity(0.5f);
	PC->PlayGameplayCameraShake(USamuraiImpactCameraShake::StaticClass(), 2.0f);
	TestTrue(TEXT("Shake starts"), PC->ActiveGameplayShake.IsValid());
	if (PC->ActiveGameplayShake.IsValid())
	{
		TestEqual(TEXT("Authored scale times user setting"), PC->ActiveGameplayShake->ShakeScale, 1.0f);
		Settings->SetCameraShakeIntensity(0.0f);
		TestEqual(TEXT("Setting immediately mutes active shake"), PC->ActiveGameplayShake->ShakeScale, 0.0f);
		PC->PlayerCameraManager->StopCameraShake(PC->ActiveGameplayShake.Get(), true);
	}
	PC->ActiveGameplayShake.Reset();
	PC->PlayGameplayCameraShake(USamuraiImpactCameraShake::StaticClass());
	TestFalse(TEXT("Zero prevents new shakes"), PC->ActiveGameplayShake.IsValid());
	Settings->SetCameraShakeIntensity(1.0f);
	PC->LastGameplayShakeTime = -1.0;

	ASamuraiCharacter* Samurai = World->SpawnActor<ASamuraiCharacter>(ASamuraiCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Samurai->SetOwner(PC);
	UAutoAttackComponent* Attack = Samurai->FindComponentByClass<UAutoAttackComponent>();
	Attack->OwnerCharacter = Samurai;
	Attack->ImpactFeedback.HitNiagaraSystem = nullptr;
	Attack->ImpactFeedback.HitSound = nullptr;
	Attack->ImpactSound = nullptr;
	const FVector Center = Samurai->GetVisualForwardVector() * Attack->AttackForwardOffset;
	TArray<AEnemyBase*> Enemies;
	for (int32 Index = 0; Index < 15; ++Index)
	{
		AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>(AEnemyBase::StaticClass(), Center + FVector(0, Index - 7, 0), FRotator::ZeroRotator, Spawn);
		Enemy->GetHealthComponent()->RestoreCurrentHealth(100.0f);
		Enemies.Add(Enemy);
	}
	FVector Location, Normal;
	Enemies[0]->GetImpactContact(FVector(-500, 0, 0), Location, Normal);
	TestTrue(TEXT("Melee contact is on surface, not origin"), !Location.Equals(Enemies[0]->GetActorLocation()));
	TestTrue(TEXT("Contact normal is normalized"), Normal.IsNormalized());
	Attack->bIsAttacking = true;
	Attack->bAttackNotifyConsumed = false;
	Attack->PerformAttackTrace();
	TestTrue(TEXT("Successful swing requests camera shake"), PC->ActiveGameplayShake.IsValid());
	int32 DamagedCount = 0;
	for (AEnemyBase* Enemy : Enemies) if (Enemy->GetHealthComponent()->GetCurrentHealth() < 100.0f) ++DamagedCount;
	TestEqual(TEXT("All fifteen enemies receive damage"), DamagedCount, 15);
	const float HealthAfterSwing = Enemies[0]->GetHealthComponent()->GetCurrentHealth();
	Attack->PerformAttackTrace();
	TestEqual(TEXT("Repeated notify cannot reapply the swing"), Enemies[0]->GetHealthComponent()->GetCurrentHealth(), HealthAfterSwing);
	for (AEnemyBase* Enemy : Enemies) Enemy->GetHealthComponent()->SetDamageEnabled(false);
	if (PC->ActiveGameplayShake.IsValid()) PC->PlayerCameraManager->StopCameraShake(PC->ActiveGameplayShake.Get(), true);
	PC->ActiveGameplayShake.Reset();
	Attack->bAttackNotifyConsumed = false;
	Attack->PerformAttackTrace();
	TestFalse(TEXT("Rejected damage causes no shake"), PC->ActiveGameplayShake.IsValid());
	for (AEnemyBase* Enemy : Enemies) Enemy->SetActorLocation(FVector(10000, 0, 0));
	Attack->bAttackNotifyConsumed = false;
	Attack->PerformAttackTrace();
	TestFalse(TEXT("Miss causes no shake"), PC->ActiveGameplayShake.IsValid());
	TestFalse(TEXT("Generic projectile feedback defaults to no shake"), FImpactFeedbackData().bEnableCameraShake);
	UImpactFeedbackLibrary::PlayImpactFeedback(Samurai, FImpactFeedbackData(), Location, Normal);

	ANinjaCharacter* Ninja = World->SpawnActor<ANinjaCharacter>(ANinjaCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Ninja->SetOwner(PC);
	AAttackProjectileBase* Projectile = World->SpawnActor<AAttackProjectileBase>();
	Projectile->InitializeProjectile(Ninja, FVector::ForwardVector, 10.0f, 1000.0f,
		EProjectileTargetType::Enemies, 0.0f, false, nullptr, true, 1);
	FHitResult Hit;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		AEnemyBase* Enemy = Enemies[Index];
		Enemy->GetHealthComponent()->SetDamageEnabled(true);
		Enemy->GetHealthComponent()->RestoreCurrentHealth(100.0f);
		Projectile->HandleProjectileOverlap(nullptr, Enemy, Enemy->GetCapsuleComponent(), 0, false, Hit);
		TestEqual(TEXT("Piercing kunai damages each enemy"), Enemy->GetHealthComponent()->GetCurrentHealth(), 90.0f);
		Projectile->HandleProjectileOverlap(nullptr, Enemy, Enemy->GetCapsuleComponent(), 0, false, Hit);
		TestEqual(TEXT("Repeated kunai overlap does not damage twice"), Enemy->GetHealthComponent()->GetCurrentHealth(), 90.0f);
	}
	TestFalse(TEXT("Kunai impacts never shake by default"), PC->ActiveGameplayShake.IsValid());

	// Exercise the saved preset through the actual camera rig, not just shake allocation.
	UClass* RigClass = LoadClass<AActor>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_PlayerCameraRig.BP_PlayerCameraRig_C"));
	UClass* ShakeClass = LoadClass<UCameraShakeBase>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/Feedback/BP_CS_SamuraiImpact.BP_CS_SamuraiImpact_C"));
	if (TestNotNull(TEXT("Saved camera rig"), RigClass) && TestNotNull(TEXT("Saved shake preset"), ShakeClass))
	{
		AActor* Rig = World->SpawnActor<AActor>(RigClass);
		if (USpringArmComponent* Boom = Rig->FindComponentByClass<USpringArmComponent>())
			Boom->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		PC->SetViewTarget(Rig);
		PC->PlayerCameraManager->UpdateCamera(1.0f / 60.0f);
		const FMinimalViewInfo Baseline = PC->PlayerCameraManager->GetCameraCacheView();
		PC->LastGameplayShakeTime = -1.0;
		PC->PlayGameplayCameraShake(ShakeClass, 1.0f);
		float PeakDisplacement = 0.0f;
		for (int32 Frame = 0; Frame < 12; ++Frame)
		{
			PC->PlayerCameraManager->UpdateCamera(1.0f / 120.0f);
			PeakDisplacement = FMath::Max(PeakDisplacement, static_cast<float>(FVector::Distance(Baseline.Location, PC->PlayerCameraManager->GetCameraLocation())));
		}
		AddInfo(FString::Printf(TEXT("Saved shake through BP camera: peak displacement %.6f cm, camera distance %.1f cm, FOV %.1f"), PeakDisplacement, Baseline.Location.Size(), Baseline.FOV));
		TestTrue(TEXT("Saved shake changes final camera POV"), PeakDisplacement > 0.001f);
	}
	Settings->OnCameraShakeIntensityChanged.RemoveAll(PC);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Settings->SetCameraShakeIntensity(OriginalIntensity);
	GConfig->UnloadFile(GGameUserSettingsIni);
	IFileManager::Get().Delete(*GGameUserSettingsIni);
	GGameUserSettingsIni = OriginalIni;
	// UE's config cache can share the settings branch with the temporary filename.
	// Persist the restored runtime preferences back to their real destination.
	Settings->SaveSettings();
	return true;
}
#endif
