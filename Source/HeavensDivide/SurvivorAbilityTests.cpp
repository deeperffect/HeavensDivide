#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "HealthComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivorAbilitiesTest,"HeavensDivide.Abilities.Expansion",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSurvivorAbilitiesTest::RunTest(const FString& Parameters)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Saved player controller loads"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* Ability=PC->FindComponentByClass<USurvivorAbilityComponent>();
 if(!TestNotNull(TEXT("Actual controller inherits ability component"),Ability)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* Upgrades=PC->GetPlayerUpgrades();Ability->Controller=PC;Ability->Upgrades=Upgrades;
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 FActorSpawnParameters SpawnParams;SpawnParams.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,SpawnParams);
 auto* Ninja=World->SpawnActor<ANinjaCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,SpawnParams);
 Samurai->SetOwner(PC);Ninja->SetOwner(PC);
 auto* Active=FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"));
 Active->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Samurai);
 for(int32 i=0;i<BuildFamilyCount;++i)
 {
  if(BuildFamilies[i].Available)continue;
  auto* Retired=NewObject<UUpgradeDefinition>();Retired->UpgradeId=BuildFamilies[i].Id;Retired->BuildFamilyId=BuildFamilies[i].Id;
  TestFalse(TEXT("Retired starter cannot be acquired even through a stale reference"),Upgrades->CanAcquireUpgrade(Retired));
  TestNull(TEXT("Retired starter absent from saved pool"),Upgrades->FindUpgradeDefinition(BuildFamilies[i].Id));
  if(i<4)TestFalse(TEXT("Retired legacy ability cannot activate"),Ability->ActivateAbility(i,Samurai));
  else TestFalse(TEXT("Retired expanded ability cannot activate"),Ability->ActivateBuildFamily(i,Samurai));
 }
 for(const auto* Id:{TEXT("BladeWave"),TEXT("BladeWavePower"),TEXT("WideArc"),TEXT("BladeWaveHaste"),TEXT("ReturningBlade"),TEXT("CrossingBlades"),TEXT("SplinterWave"),TEXT("GrandEntrance"),TEXT("TagTeam")})
  TestNotNull(TEXT("Attack upgrades remain in saved pool"),Upgrades->FindUpgradeDefinition(Id));
 PC->AbilityShowcase();
 TestTrue(TEXT("Legacy showcase now previews Blade Wave"),Upgrades->HasUpgradeId(TEXT("BladeWave")));
 const int32 Mastery=Upgrades->GetSamuraiMasteryPoints();PC->AbilityShowcase();
 TestEqual(TEXT("Preview remains idempotent"),Upgrades->GetSamuraiMasteryPoints(),Mastery);
 Ability->UpdateAbilities();
 TestTrue(TEXT("No automatic casts are scheduled"),Ability->Pending.IsEmpty()&&Ability->BuildCasts.IsEmpty());
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
