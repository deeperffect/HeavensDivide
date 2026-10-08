#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "SharedPlayerStatsComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExperiencePickupMotionTest, "HeavensDivide.Pickups.ExperienceMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExperiencePickupMotionTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	auto* PC = World->SpawnActor<ASurvivorPlayerController>();
	auto* Party = PC->GetCharacterManager();
	auto* Stats = PC->GetSharedPlayerStats();
	auto* XP = PC->GetExperienceComponent();
	XP->RestoreRunState(100, 0);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Samurai = World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	auto* Ninja = World->SpawnActor<ANinjaCharacter>(FVector(0, 500, 0), FRotator::ZeroRotator, Params);
	const auto* ActiveProperty = FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"));
	ActiveProperty->SetObjectPropertyValue_InContainer(Party, Samurai);
	auto* PickupClass = LoadClass<AExperiencePickup>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_ExperiencePickup.BP_ExperiencePickup_C"));
	if (!TestNotNull(TEXT("Authored XP pickup loads"), PickupClass))
	{
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	auto SpawnPickup = [&](FVector Position)
	{
		auto* Orb = World->SpawnActor<AExperiencePickup>(PickupClass, Position, FRotator::ZeroRotator, Params);
		Orb->ExperienceComponent = XP;
		Orb->CharacterManager = Party;
		Orb->SharedPlayerStats = Stats;
		Orb->PickupSound = nullptr;
		Orb->DispatchBeginPlay();
		Orb->InitializePickup(1, XP, Party);
		return Orb;
	};
	const auto* Defaults = PickupClass->GetDefaultObject<AExperiencePickup>();
	auto* Idle = SpawnPickup(FVector(10000, 0, 0));
	TestFalse(TEXT("Distant XP costs no actor tick"), Idle->IsActorTickEnabled());
	TestEqual(TEXT("Starting range stays at the authored value"), Idle->AttractionRadius, Defaults->AttractionRadius);
	auto* Magnetism = LoadObject<UUpgradeDefinition>(nullptr, TEXT("/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalPickupRadius.DA_Upgrade_GlobalPickupRadius"));
	if (TestNotNull(TEXT("Saved Magnetism upgrade loads"), Magnetism))
	{
		for (int32 Rank = 1; Rank <= 3; ++Rank)
		{
			TestTrue(TEXT("Acquire pickup range rank"), PC->GetPlayerUpgrades()->AcquireUpgrade(Magnetism));
			TestTrue(TEXT("Each Common rank adds 30 percent of the base range"),
				FMath::IsNearlyEqual(Idle->AttractionRadius, Defaults->AttractionRadius * (1.f + .3f * Rank), .01f));
		}
		PC->GetPlayerUpgrades()->RebuildAllUpgradeModifiers();
		TestTrue(TEXT("Rebuilding stats does not compound pickup range"),
			FMath::IsNearlyEqual(Idle->AttractionRadius, Defaults->AttractionRadius * 1.9f, .01f));
		Stats->ClearModifiersFromSource(Magnetism->UpgradeId);
	}

	// At multiple frame rates, every orb must kick out once, reverse, and award once.
	for (float Step : {1.f / 120.f, 1.f / 60.f, 1.f / 15.f})
	{
		const FVector Start(Defaults->AttractionRadius * .8f, 0, 0);
		auto* Orb = SpawnPickup(Start);
		const int32 BeforeXP = XP->GetCurrentXP();
		Orb->Tick(Step);
		TestTrue(TEXT("First movement is away from the player"), Orb->GetActorLocation().X > Start.X);
		TestEqual(TEXT("Kick cannot push XP below the ground"), Orb->GetActorLocation().Z, Start.Z);
		TestEqual(TEXT("XP is not awarded during the kick"), XP->GetCurrentXP(), BeforeXP);
		TestEqual(TEXT("Attracting XP no longer generates overlap work"), Orb->PickupCollision->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		float Elapsed = Step;
		for (; !Orb->bCollected && Elapsed < 2.f; Elapsed += Step) Orb->Tick(Step);
		TestTrue(TEXT("XP returns promptly at every tested frame rate"), Orb->bCollected && Elapsed < .8f);
		TestTrue(TEXT("Return accelerates beyond the old constant speed"), Orb->CurrentAttractionSpeed > Orb->AttractionSpeed);
		Orb->Collect(); Orb->Tick(Step);
		TestEqual(TEXT("XP is awarded exactly once"), XP->GetCurrentXP(), BeforeXP + 1);
	}
	auto* Close = SpawnPickup(FVector::ZeroVector);
	TestFalse(TEXT("XP spawned on the player still shows its kick"), Close->bCollected);
	Close->Tick(.07f);
	TestTrue(TEXT("Coincident pickup gets a valid outward direction"), Close->GetActorLocation().Size() > 1.f);
	Close->Tick(1.f);
	TestTrue(TEXT("A long frame crosses both phases without losing XP"), Close->bCollected);

	auto* Swapping = SpawnPickup(FVector(Defaults->AttractionRadius * .8f, 0, 0));
	TestTrue(TEXT("Swap fixture starts within the actual pickup range"), Swapping->bAttracting);
	Swapping->Tick(.07f);
	ActiveProperty->SetObjectPropertyValue_InContainer(Party, Ninja);
	float PreviousDistance = FVector::Dist(Swapping->GetActorLocation(), Ninja->GetActorLocation());
	Swapping->Tick(.2f);
	TestTrue(TEXT("An in-flight pickup retargets to the newly active character"),
		FVector::Dist(Swapping->GetActorLocation(), Ninja->GetActorLocation()) < PreviousDistance);
	for (int32 Frame = 0; Frame < 120 && !Swapping->bCollected; ++Frame)
	{
		Ninja->AddActorWorldOffset(FVector(5, 0, 0));
		Swapping->Tick(1.f / 60.f);
	}
	TestTrue(TEXT("Homing catches a moving character after swapping"), Swapping->bCollected);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
