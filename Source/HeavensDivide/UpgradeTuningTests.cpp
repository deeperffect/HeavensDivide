#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "HealthComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpgradeTuningTest,"HeavensDivide.Abilities.EditorTuning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUpgradeTuningTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Controller"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* A=PC->FindComponentByClass<USurvivorAbilityComponent>();auto* U=PC->GetPlayerUpgrades();A->Controller=PC;A->Upgrades=U;
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);Samurai->SetOwner(PC);
 auto* Enemy=World->SpawnActor<AEnemyBase>(FVector(500,0,0),FRotator::ZeroRotator,Params);
 Enemy->GetHealthComponent()->SetMaxHealthPreservePercent(100000);Enemy->GetHealthComponent()->RestoreCurrentHealth(100000);
 auto* Root=A->TuningDefinition(0);auto* Echo=A->TuningDefinition(0,0);auto* Synergy=A->TuningDefinition(0,3);
 if(!TestNotNull(TEXT("Editable starter loaded from saved pool"),Root)||!Echo||!Synergy){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 const auto SavedRoot=Root->BalanceParameters,SavedEcho=Echo->BalanceParameters,SavedSynergy=Synergy->BalanceParameters;
 const auto SavedVFX=Root->Presentation;const auto SavedBranchVFX=Echo->Presentation;
 TestTrue(TEXT("Migration exposes starter balance"),Root->bHasRuntimeBalance&&Root->BalanceParameters.Contains(TEXT("Radius")));
 TestTrue(TEXT("Migration exposes branch balance"),Echo->BalanceParameters.Contains(TEXT("DamageMultiplier")));
 TestTrue(TEXT("Migration exposes synergy balance"),Synergy->BalanceParameters.Contains(TEXT("TriggerCooldown")));
 U->AcquireUpgrade(Root);
 Root->BalanceParameters.Add(TEXT("Radius"),260);
 TestFalse(TEXT("Enemy outside configured base radius cannot trigger Tempest"),A->ActivateAbility(0,Samurai));
 Root->BalanceParameters.Add(TEXT("Radius"),600);Root->BalanceParameters.Add(TEXT("Damage"),17);Root->BalanceParameters.Add(TEXT("Cooldown"),0.8f);
 const float Before=Enemy->GetHealthComponent()->GetCurrentHealth();
 TestTrue(TEXT("Editing radius expands acquisition and actual damage area"),A->ActivateAbility(0,Samurai));
 TestTrue(TEXT("Editing damage changes authoritative health loss"),FMath::IsNearlyEqual(Before-Enemy->GetHealthComponent()->GetCurrentHealth(),17*A->Power(Samurai),0.02f));
 TestEqual(TEXT("Editing cooldown changes recharge"),A->Cooldown(0),0.8f);
 A->GrantBuildPreview(TEXT("SteelTempest"),1);A->Pending.Reset();
 Echo->BalanceParameters.Add(TEXT("DamageMultiplier"),0.2f);Echo->BalanceParameters.Add(TEXT("Delay"),0.7f);
 const float BeforeEcho=Enemy->GetHealthComponent()->GetCurrentHealth();A->ActivateAbility(0,Samurai);
 TestEqual(TEXT("Razor Halo is scheduled once"),A->Pending.Num(),1);
 if(A->Pending.Num()==1)
 {
  TestEqual(TEXT("Branch asset controls echo delay"),A->Pending[0].Remaining,0.7f);
  TestTrue(TEXT("Branch asset controls echo fraction"),FMath::IsNearlyEqual(A->Pending[0].Damage,(BeforeEcho-Enemy->GetHealthComponent()->GetCurrentHealth())*0.2f,0.02f));
 }
 A->Pending.Reset();A->BuildMarks.Reset();A->ReactionGates[0]=0;
 Synergy->BalanceParameters.Add(TEXT("DamageMultiplier"),2);Synergy->BalanceParameters.Add(TEXT("Count"),1);Synergy->BalanceParameters.Add(TEXT("TriggerCooldown"),2.5f);
 A->RegisterFamilyHit(0,Enemy,20);const float BeforeReaction=Enemy->GetHealthComponent()->GetCurrentHealth();A->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemy);
 TestTrue(TEXT("Synergy asset controls payout"),FMath::IsNearlyEqual(BeforeReaction-Enemy->GetHealthComponent()->GetCurrentHealth(),40,0.01f));
 TestEqual(TEXT("Synergy asset controls trigger throttle"),A->ReactionGates[0],2.5f);
 // A real Niagara component receives the selected system, world placement and live radius parameters.
 auto* System=NewObject<UNiagaraSystem>(GetTransientPackage());
 Root->Presentation.PulseSystem=System;Root->Presentation.LocationOffset=FVector(10,20,30);Root->Presentation.AuthoredRadius=100;
 Root->Presentation.Scale=FVector(0.5f);Root->Presentation.LifetimeOverride=0.75f;
 auto* FX=A->FamilyAccent(0,FVector::ZeroVector,FVector(200,0,0),400,FLinearColor::White);
 TestNotNull(TEXT("Configured VFX actor"),FX);
 if(FX)
 {
  auto* Niagara=FX->FindComponentByClass<UNiagaraComponent>();
  TestTrue(TEXT("Selected Niagara is actually assigned"),Niagara&&Niagara->GetAsset()==System);
  if(Niagara)
  {
   TestTrue(TEXT("Niagara size follows gameplay radius"),Niagara->GetComponentScale().Equals(FVector(2),0.01f));
   bool bValid=false;const float Radius=Niagara->GetVariableFloat(TEXT("User.Radius"),bValid);
   TestTrue(TEXT("Niagara receives radius user parameter"),bValid&&FMath::IsNearlyEqual(Radius,400));
  }
  TestTrue(TEXT("Configured spawn offset is applied"),FX->GetActorLocation().Equals(FVector(10,20,30)));
  FX->MoveAnchor(FVector(100,0,0));TestTrue(TEXT("Following fields preserve their VFX offset"),FX->GetActorLocation().Equals(FVector(110,20,30)));
  TestFalse(TEXT("Assigned Niagara replaces fallback by default"),FX->FindComponentByClass<UStaticMeshComponent>()->IsVisible());FX->Destroy();
 }
 Echo->Presentation.bOverrideFamilyVisuals=true;Echo->Presentation.Scale=FVector(3);
 TestTrue(TEXT("Owned branch can override the base presentation"),A->FamilyPresentation(0)==&Echo->Presentation);
 Root->BalanceParameters=SavedRoot;Echo->BalanceParameters=SavedEcho;Synergy->BalanceParameters=SavedSynergy;Root->Presentation=SavedVFX;Echo->Presentation=SavedBranchVFX;
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
