#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AutoAttackComponent.h"
#include "SwapPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AttackProjectileBase.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrandEntranceTest,"HeavensDivide.Combat.GrandEntrance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGrandEntranceTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 const auto ControllerClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(ControllerClass);
 if(!TestNotNull(TEXT("Saved controller with authored upgrade pool"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 auto* Ninja=World->SpawnActor<ANinjaCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 Samurai->SetOwner(PC);Ninja->SetOwner(PC);Samurai->SetCharacterMode(ECharacterMode::Active);
 auto* Melee=Samurai->FindComponentByClass<UAutoAttackComponent>();
 auto* Ranged=Ninja->FindComponentByClass<UAutoAttackComponent>();
 Melee->OwnerCharacter=Samurai;Ranged->OwnerCharacter=Ninja;
 Melee->ImpactFeedback.bEnableCameraShake=false;
 Ranged->ProjectileClass=AAttackProjectileBase::StaticClass();Ranged->ActiveAttackDirection=FVector::ForwardVector;
 auto* Card=LoadObject<UUpgradeDefinition>(nullptr,TEXT("/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_GrandEntrance"));
 if(!TestNotNull(TEXT("Saved Grand Entrance card"),Card)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 TestEqual(TEXT("Shared synergy category"),Card->Category,EUpgradeCategory::Synergy);
 TestTrue(TEXT("Card has illustration"),Card->CardArtwork!=nullptr&&Card->Icon!=nullptr);
 Melee->ArmGrandEntranceAfterSwap();TestFalse(TEXT("Requires owning upgrade"),Melee->bGrandEntranceReady);
 TestTrue(TEXT("Acquire shared upgrade"),PC->GetPlayerUpgrades()->AcquireUpgrade(Card));
 TestFalse(TEXT("Single rank"),PC->GetPlayerUpgrades()->CanAcquireUpgrade(Card));
 TestFalse(TEXT("Acquisition alone grants no enhanced attack"),Melee->bGrandEntranceReady);
 PC->bPlayerInitiatedSwapPending=false;PC->HandleCharacterSwapped(Ninja,Samurai);
 TestFalse(TEXT("Automatic party changes do not grant enhancement"),Melee->bGrandEntranceReady);
 PC->bPlayerInitiatedSwapPending=true;PC->HandleCharacterSwapped(Ninja,Samurai);
 TestTrue(TEXT("Player swap arms incoming character"),Melee->bGrandEntranceReady);
 TestNotNull(TEXT("Pending enhancement resolves saved card tuning"),Melee->GetReadyGrandEntranceUpgrade());
 auto* Feedback=Samurai->SwapPresentation.Get();
 Feedback->bEnableSound=false;Feedback->bUseFallbackArrivalRing=false;Feedback->SwapFreezeDuration=0;
 Feedback->bEnableNinjaArrivalDrop=false;Feedback->bEnableSamuraiWalkIn=false; // Isolate the upgrade from cosmetic arrival timing.
 auto* OriginalOverlay=Samurai->GetMesh()->GetOverlayMaterial();
 Feedback->PlayArrival();Feedback->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("Grand Entrance never recolors arriving character"),Samurai->GetMesh()->GetOverlayMaterial(),OriginalOverlay);
 Melee->StopAutoAttack();TestTrue(TEXT("Canceled attack retains charge"),Melee->bGrandEntranceReady);
 Melee->bActiveAttackIsAssist=true;TestNull(TEXT("Assist cannot use enhancement"),Melee->GetReadyGrandEntranceUpgrade());Melee->bActiveAttackIsAssist=false;
 Melee->bDoubleCutFollowUpActive=true;TestNull(TEXT("Double Cut follow-up cannot use enhancement"),Melee->GetReadyGrandEntranceUpgrade());Melee->bDoubleCutFollowUpActive=false;
 FAutoAttackRunState State;Melee->CaptureRunState(State);Melee->bGrandEntranceReady=false;Melee->RestoreRunState(State);
 TestTrue(TEXT("Run snapshot preserves one pending enhancement"),Melee->bGrandEntranceReady);
 auto SpawnEnemy=[&](FVector Position){auto* E=World->SpawnActor<AEnemyBase>(Position,FRotator::ZeroRotator,Params);E->GetHealthComponent()->SetMaxHealthPreservePercent(10000);E->GetHealthComponent()->RestoreCurrentHealth(10000);return E;};
 auto* Front=SpawnEnemy(FVector(450,0,0));auto* Rear=SpawnEnemy(FVector(-450,0,0));auto* Far=SpawnEnemy(FVector(950,0,0));
 TestTrue(TEXT("Enhanced targeting reaches extended slash"),Melee->GetEffectiveTargetingRange()>=600);
 const float Damage=Melee->GetEffectiveAttackDamage();
 Melee->bIsAttacking=true;Melee->bAttackNotifyConsumed=false;Melee->PerformAttackTrace();
 TestTrue(TEXT("Enhanced swing hits distant front enemy"),FMath::IsNearlyEqual(10000-Front->GetHealthComponent()->GetCurrentHealth(),Damage,.02f));
 TestTrue(TEXT("Enhanced swing deals full damage behind Samurai"),FMath::IsNearlyEqual(10000-Rear->GetHealthComponent()->GetCurrentHealth(),Damage,.02f));
 TestEqual(TEXT("Enhanced swing respects radius"),Far->GetHealthComponent()->GetCurrentHealth(),10000.f);
 TestFalse(TEXT("Swing spends exactly one charge"),Melee->bGrandEntranceReady);
 Feedback->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("Spending enhancement restores original overlay"),Samurai->GetMesh()->GetOverlayMaterial(),OriginalOverlay);
 const float RearAfter=Rear->GetHealthComponent()->GetCurrentHealth();
 Melee->PerformAttackTrace();TestEqual(TEXT("Duplicate notify cannot deal damage"),Rear->GetHealthComponent()->GetCurrentHealth(),RearAfter);
 Melee->bAttackNotifyConsumed=false;Melee->PerformAttackTrace();
 TestEqual(TEXT("Next swing returns to ordinary area"),Rear->GetHealthComponent()->GetCurrentHealth(),RearAfter);
 Melee->ArmGrandEntranceAfterSwap();Melee->HandleOwnerCharacterModeChanged(ECharacterMode::Active,ECharacterMode::Inactive);
 TestFalse(TEXT("Leaving active character clears unused enhancement"),Melee->bGrandEntranceReady);
 Samurai->SetCharacterMode(ECharacterMode::Inactive);Ninja->SetCharacterMode(ECharacterMode::Active);
 PC->HandleCharacterSwapped(Samurai,Ninja);
 TestTrue(TEXT("Swap to Ninja arms projectile enhancement"),Ranged->bGrandEntranceReady);
 auto CountProjectiles=[&](){int32 Count=0;for(TActorIterator<AAttackProjectileBase> It(World);It;++It)if(It->GetOwner()==Ninja)++Count;return Count;};
 const int32 BaseCount=Ranged->GetEffectiveProjectileCount();
 Ranged->bIsAttacking=true;Ranged->bAttackNotifyConsumed=false;Ranged->SpawnAutoAttackProjectile();
 TestEqual(TEXT("Enhanced volley spawns eight extra actual projectiles"),CountProjectiles(),BaseCount+8);
 TestFalse(TEXT("Volley consumes enhancement"),Ranged->bGrandEntranceReady);
 Ranged->SpawnAutoAttackProjectile();TestEqual(TEXT("Duplicate projectile notify does not spawn again"),CountProjectiles(),BaseCount+8);
 Ranged->bAttackNotifyConsumed=false;Ranged->SpawnAutoAttackProjectile();
 TestEqual(TEXT("Next volley has normal projectile count"),CountProjectiles(),BaseCount*2+8);
 Ranged->ArmGrandEntranceAfterSwap();PC->bIsPlayerDead=true;
 TestNull(TEXT("Death disables pending enhancement"),Ranged->GetReadyGrandEntranceUpgrade());
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
