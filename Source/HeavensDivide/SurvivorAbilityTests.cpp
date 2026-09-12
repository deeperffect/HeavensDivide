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
 auto Card=[&](const TCHAR* Owner,const TCHAR* Id)
 {
  const FString Path=FString(TEXT("/Game/HeavensDivide/Upgrades/"))+Owner+TEXT("/DA_Upgrade_")+Owner+Id;
  auto* Result=LoadObject<UUpgradeDefinition>(nullptr,*Path);TestNotNull(*Path,Result);return Result;
 };
 TArray<AEnemyBase*> Enemies;
 for(int32 i=0;i<3;++i)
 {
  auto* E=World->SpawnActor<AEnemyBase>(FVector(100+i*110,0,0),FRotator::ZeroRotator,SpawnParams);
  E->GetHealthComponent()->SetMaxHealthPreservePercent(10000);E->GetHealthComponent()->RestoreCurrentHealth(10000);Enemies.Add(E);
 }
 Ability->UpdateAbilities();TestEqual(TEXT("Locked abilities never fire"),Enemies[0]->GetHealthComponent()->GetCurrentHealth(),10000.0f);
 auto* Power=Card(TEXT("Samurai"),TEXT("SteelTempestPower"));
 TestFalse(TEXT("Scaling is gated behind its ability"),Upgrades->CanAcquireUpgrade(Power));
 TestTrue(TEXT("Starter can be acquired naturally"),Upgrades->AcquireUpgrade(Card(TEXT("Samurai"),TEXT("SteelTempest"))));
 const float Expected=24*Ability->Power(Samurai);
 Ability->UpdateAbilities();
 TestTrue(TEXT("Pulse applies damage once per enemy through health"),FMath::IsNearlyEqual(10000-Enemies[0]->GetHealthComponent()->GetCurrentHealth(),Expected,0.01f));
 const float After=Enemies[0]->GetHealthComponent()->GetCurrentHealth();Ability->UpdateAbilities();
 TestEqual(TEXT("Cooldown prevents repeated pulse"),Enemies[0]->GetHealthComponent()->GetCurrentHealth(),After);
 TestFalse(TEXT("Evolution requires investment"),Upgrades->CanAcquireUpgrade(Card(TEXT("Samurai"),TEXT("RazorHalo"))));
 for(int32 i=0;i<2;++i){Upgrades->AcquireUpgrade(Power);Upgrades->AcquireUpgrade(Card(TEXT("Samurai"),TEXT("SteelTempestArea")));}
 TestTrue(TEXT("Damage scaling accumulates"),Ability->Magnitude(0,TEXT("Power"))>0.39f);
 Upgrades->AcquireUpgrade(Card(TEXT("Samurai"),TEXT("SteelTempestHaste")));
 TestTrue(TEXT("Own recharge stat reduces ability cooldown"),Ability->Cooldown(0)<4);
 TestTrue(TEXT("Evolution unlocks after force/reach investment"),Upgrades->AcquireUpgrade(Card(TEXT("Samurai"),TEXT("RazorHalo"))));
 Ability->ActivateAbility(0,Samurai);TestEqual(TEXT("Razor Halo schedules one echo"),Ability->Pending.Num(),1);Ability->Pending.Reset();
 Upgrades->AcquireUpgrade(Card(TEXT("Samurai"),TEXT("Heavenfall")));
 TestTrue(TEXT("Heavenfall finds a target"),Ability->ActivateAbility(1,Samurai));
 const float BeforeSky=Enemies[0]->GetHealthComponent()->GetCurrentHealth();
 for(auto* E:Enemies) E->SetActorLocation(FVector(3000,0,0));
 Ability->Cooldowns[1]=100;
 for(int32 i=0;i<6;++i) Ability->UpdateAbilities();
 TestEqual(TEXT("Escaping the marked position avoids Heavenfall"),Enemies[0]->GetHealthComponent()->GetCurrentHealth(),BeforeSky);
 TestTrue(TEXT("Delayed strike is consumed once"),Ability->Pending.IsEmpty());
 for(int32 i=0;i<3;++i) Enemies[i]->SetActorLocation(FVector(100+i*110,0,0));
 Active->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Ninja);
 Upgrades->AcquireUpgrade(Card(TEXT("Ninja"),TEXT("NightThread")));
 TArray<float> BeforeThread;for(auto* E:Enemies)BeforeThread.Add(E->GetHealthComponent()->GetCurrentHealth());
 Ability->ActivateAbility(2,Ninja);
 for(int32 i=0;i<3;++i) TestTrue(TEXT("Thread hits each distinct chain target"),FMath::IsNearlyEqual(BeforeThread[i]-Enemies[i]->GetHealthComponent()->GetCurrentHealth(),25*Ability->Power(Ninja),0.01f));
 Upgrades->AcquireUpgrade(Card(TEXT("Ninja"),TEXT("VenomGarden")));
 Ability->Cooldowns[2]=100;Ability->Cooldowns[3]=100;
 Ability->ActivateAbility(3,Ninja);
 const float BeforeGarden=Enemies[0]->GetHealthComponent()->GetCurrentHealth();const float GardenDamage=4*Ability->Power(Ninja);
 // Cast fields persist on swap, but the other character's cooldown stays frozen.
 Active->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Samurai);Ability->Cooldowns[0]=100;
 for(int32 i=0;i<80;++i)Ability->UpdateAbilities();
 TestTrue(TEXT("Garden deals exactly eight pulses across character swap"),FMath::IsNearlyEqual(BeforeGarden-Enemies[0]->GetHealthComponent()->GetCurrentHealth(),8*GardenDamage,0.02f));
 TestTrue(TEXT("Garden expires and leaves no pending work"),Ability->Pending.IsEmpty());
 TestEqual(TEXT("Garden seeds Poison without stacking it each pulse"),Enemies[0]->GetStatusStacks(EEnemyStatusEffect::Poison),1);
 TestEqual(TEXT("Inactive character cooldown is preserved"),Ability->Cooldowns[2],100.0f);
 // Garden is crowd-targeted rather than wasted on the nearest straggler.
 Enemies[0]->SetActorLocation(FVector(100,0,0));
 Enemies[1]->SetActorLocation(FVector(700,0,0));Enemies[2]->SetActorLocation(FVector(800,0,0));
 Ability->ActivateAbility(3,Ninja);
 TestTrue(TEXT("Garden selects the group behind the nearest isolated enemy"),Ability->Pending[0].Position.X>=700);
 TestFalse(TEXT("Cannot stack gardens"),Ability->ActivateAbility(3,Ninja));
 for(int32 i=0;i<12;++i) Ability->UpdateAbilities();
 const auto Garden=Ability->Pending[0];
 TestEqual(TEXT("Two elapsed garden pulses are excluded from payout"),Garden.Ticks,6);
 Ability->HandleSamuraiMeleeHit(FVector(-1000,0,0));
 TestEqual(TEXT("A hit outside garden cannot detonate it"),Ability->Pending.Num(),1);
 const float BeforeBurst=Enemies[1]->GetHealthComponent()->GetCurrentHealth();
 Ability->HandleSamuraiMeleeHit(Garden.Position);
 TestTrue(TEXT("Detonation consumes field"),Ability->Pending.IsEmpty());
 TestTrue(TEXT("Detonation pays remaining damage with combo bonus"),FMath::IsNearlyEqual(BeforeBurst-Enemies[1]->GetHealthComponent()->GetCurrentHealth(),Garden.Damage*Garden.Ticks*1.5f,0.02f));
 TestTrue(TEXT("Detonation prepares survivors with Bleed"),Enemies[1]->HasStatus(EEnemyStatusEffect::Bleed));
 const float AfterBurst=Enemies[1]->GetHealthComponent()->GetCurrentHealth();Ability->HandleSamuraiMeleeHit(Garden.Position);
 TestEqual(TEXT("Repeated melee hit cannot duplicate payout"),Enemies[1]->GetHealthComponent()->GetCurrentHealth(),AfterBurst);
 Ability->ActivateAbility(1,Samurai);
 TestTrue(TEXT("Heavenfall also favors the group"),Ability->Pending[0].Position.X>=700);Ability->Pending.Reset();
 for(int32 i=0;i<3;++i){Enemies[i]->SetActorLocation(FVector(100+i*110,0,0));Enemies[i]->GetStatusEffectComponent()->ClearAllStatuses();}
 Samurai->SetCharacterMode(ECharacterMode::Assisting);Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
 TestTrue(TEXT("Samurai setup resolves through assist mode"),Ability->ExecuteSetupAssist(Samurai));
 for(auto* E:Enemies) TestTrue(TEXT("Broad assist applies intrinsic Bleed without Bleeding Edge"),E->HasStatus(EEnemyStatusEffect::Bleed));
 Samurai->SetCharacterMode(ECharacterMode::Active);
 Ability->ActivateAbility(2,Ninja);
 for(auto* E:Enemies) TestTrue(TEXT("Night Thread poisons bleeding survivors without Venomous Kunai"),E->HasStatus(EEnemyStatusEffect::Poison));
 for(auto* E:Enemies) E->GetStatusEffectComponent()->ClearAllStatuses();
 Ninja->SetCharacterMode(ECharacterMode::Assisting);Ninja->SetVisualFacingRotation(FRotator::ZeroRotator);
 TestTrue(TEXT("Ninja setup resolves"),Ability->ExecuteSetupAssist(Ninja));
 for(auto* E:Enemies) TestTrue(TEXT("Ninja fan poisons multiple enemies"),E->HasStatus(EEnemyStatusEffect::Poison));
 Ninja->SetCharacterMode(ECharacterMode::Inactive);
 TestFalse(TEXT("Inactive character cannot execute setup attack"),Ability->ExecuteSetupAssist(Ninja));
 TestFalse(TEXT("Ordinary attacks still require their status starter"),Enemies[0]->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Samurai));
 TestFalse(TEXT("Intrinsic status cannot bypass character source rules"),Enemies[0]->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Ninja,true));
 auto Offers=Upgrades->RollUpgradeChoices(EUpgradeCategory::Samurai,3);
 TestEqual(TEXT("Character offer has three choices"),Offers.Num(),3);
 TestTrue(TEXT("Offer includes a new starter when available"),Offers.ContainsByPredicate([](const auto* U){return U->Role==EUpgradeRole::Starter;}));
 TestTrue(TEXT("Offer includes scaling when available"),Offers.ContainsByPredicate([](const auto* U){return U->Role==EUpgradeRole::Support;}));
 FPlayerUpgradeRunState Saved;Upgrades->CaptureRunState(Saved);Upgrades->RestoreRunState(Saved);
 TestTrue(TEXT("Ability scaling survives run-state restoration"),Ability->Magnitude(0,TEXT("Power"))>0.39f);
 PC->AbilityShowcase();
 TestTrue(TEXT("Showcase unlocks all evolutions"),Upgrades->HasUpgradeId(TEXT("Starfall"))&&Upgrades->HasUpgradeId(TEXT("BlackWeb"))&&Upgrades->HasUpgradeId(TEXT("WitheringGarden")));
 const int32 Mastery=Upgrades->GetSamuraiMasteryPoints();PC->AbilityShowcase();
 TestEqual(TEXT("Repeated showcase does not inflate mastery"),Upgrades->GetSamuraiMasteryPoints(),Mastery);
 Ability->Pending.Reset();Ability->ActivateAbility(1,Samurai);
 TestEqual(TEXT("Starfall avoids wasting strikes on the same tight group"),Ability->Pending.Num(),1);
 Ability->Pending.Reset();
 Enemies[0]->SetActorLocation(FVector(800,0,0));Enemies[1]->SetActorLocation(FVector(-800,0,0));Enemies[2]->SetActorLocation(FVector(0,800,0));
 Ability->ActivateAbility(1,Samurai);TestEqual(TEXT("Starfall covers three separated groups"),Ability->Pending.Num(),3);
 for(int32 i=0;i<3;++i) Enemies[i]->SetActorLocation(FVector(100+i*110,0,0));
 Ability->Pending.Reset();Ability->ActivateAbility(2,Ninja);
 TestEqual(TEXT("Black Web schedules a burst at each target"),Ability->Pending.Num(),3);
 Ability->Pending.Reset();Ability->ActivateAbility(3,Ninja);
 TestTrue(TEXT("Withering Garden extends pulses and enables final burst"),Ability->Pending.Num()==1&&Ability->Pending[0].Ticks==11&&Ability->Pending[0].bFinalBurst);
 auto* EndState=FindFProperty<FEnumProperty>(ASurvivorPlayerController::StaticClass(),TEXT("RunEndState"));
 EndState->GetUnderlyingProperty()->SetIntPropertyValue(EndState->ContainerPtrToValuePtr<void>(PC),static_cast<int64>(ERunEndState::Victory));
 Ninja->SetCharacterMode(ECharacterMode::Assisting);
 TestFalse(TEXT("Run end prevents assist damage"),Ability->ExecuteSetupAssist(Ninja));
 const float BeforeEnd=Enemies[0]->GetHealthComponent()->GetCurrentHealth();Ability->UpdateAbilities();
 TestTrue(TEXT("Run end cancels pending ability damage"),Ability->Pending.IsEmpty());
 TestEqual(TEXT("Run end deals no lingering damage"),Enemies[0]->GetHealthComponent()->GetCurrentHealth(),BeforeEnd);
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
