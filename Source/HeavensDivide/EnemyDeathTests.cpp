#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnemyBase.h"
#include "EnemyDeathComponent.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyDeathTest, "HeavensDivide.EnemyDeath.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyDeathTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
	UEnemyDeathComponent* Death = Enemy->FindComponentByClass<UEnemyDeathComponent>();
	TestNotNull(TEXT("Base enemies inherit the death component"), Death);
	TestFalse(TEXT("No presentation tick while alive"), Death->IsComponentTickEnabled());
	Death->DeathNiagaraSystem = nullptr;
	Death->DeathSound = nullptr;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Assets/EnemyCharacters/Grunt/M_EnemyGrunt.M_EnemyGrunt"));
	TestNotNull(TEXT("Migrated enemy material exists"), Material);
	USkeletalMesh* SourceMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Assets/EnemyCharacters/Grunt/Enemy_BasicAxe.Enemy_BasicAxe"));
	TestNotNull(TEXT("Enemy mesh fixture exists"), SourceMesh);
	if (!SourceMesh)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	USkeletalMesh* Mesh = DuplicateObject<USkeletalMesh>(SourceMesh, GetTransientPackage());
	Mesh->GetMaterials().Add(FSkeletalMaterial(Material));
	Enemy->GetMesh()->SetSkeletalMeshAsset(Mesh);
	Enemy->bDropsXP = false;
	Enemy->GetMesh()->SetMaterial(0, Material);
	Enemy->GetMesh()->SetMaterial(1, Material);
	const FVector OriginalScale = Enemy->GetMesh()->GetRelativeScale3D();
	Enemy->GetHealthComponent()->OnDeath.AddUniqueDynamic(Enemy, &AEnemyBase::HandleDeath);
	Enemy->GetHealthComponent()->RestoreCurrentHealth(100.0f);
	Enemy->GetHealthComponent()->ApplyDamage(1000.0f);
	TestTrue(TEXT("Lethal HP triggers authoritative death"), Enemy->IsDead());
	TestFalse(TEXT("Dead enemy cannot collide"), Enemy->GetActorEnableCollision());
	TestFalse(TEXT("Dead enemy cannot tick gameplay"), Enemy->IsActorTickEnabled());
	TestFalse(TEXT("Cleanup waits for dissolve"), Enemy->IsActorBeingDestroyed());
	Death->TickComponent(0.125f, LEVELTICK_All, nullptr);
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Enemy->GetMesh()->GetMaterial(Slot));
		TestNotNull(TEXT("Every material slot gets a MID"), MID);
		if (MID) TestEqual(TEXT("Every slot reaches midpoint"), MID->K2_GetScalarParameterValue(TEXT("DissolveAmount")), 0.5f);
	}
	TestEqual(TEXT("Death does not shrink the mesh"), Enemy->GetMesh()->GetRelativeScale3D(), OriginalScale);
	Enemy->HandleDeath();
	TestFalse(TEXT("Repeated death does not destroy early"), Enemy->IsActorBeingDestroyed());
	Death->TickComponent(0.125f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Existing actor destruction runs at 0.25 seconds even with missing assets"), Enemy->IsActorBeingDestroyed());

	AEnemyBase* Boss = World->SpawnActor<AEnemyBase>();
	Boss->DropCategory = EEnemyDropCategory::Boss;
	Boss->bDropsXP = false;
	Boss->HandleDeath();
	TestFalse(TEXT("Boss category bypasses standard dissolve"), Boss->EnemyDeathComponent->IsComponentTickEnabled());
	TestTrue(TEXT("Boss fallback retains delayed cleanup"), Boss->GetLifeSpan() > 0.0f);

	AActor* Owner = World->SpawnActor<AActor>();
	UEnemyDeathComponent* Instant = NewObject<UEnemyDeathComponent>(Owner);
	Instant->RegisterComponent();
	Instant->DissolveDuration = 0.0f;
	Instant->DeathNiagaraSystem = nullptr;
	Instant->DeathSound = nullptr;
	int32 Completions = 0;
	const FSimpleDelegate Callback = FSimpleDelegate::CreateLambda([&Completions]() { ++Completions; });
	Instant->StartDeathPresentation(Callback);
	Instant->StartDeathPresentation(Callback);
	TestEqual(TEXT("Zero-duration presentation completes exactly once"), Completions, 1);
	TestFalse(TEXT("Instant completion leaves no tick"), Instant->IsComponentTickEnabled());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
