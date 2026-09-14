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
 auto* Root=A->TuningDefinition(4);auto* Echo=A->TuningDefinition(4,0);
 if(!TestNotNull(TEXT("Editable starter loaded from saved pool"),Root)||!Echo){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 const auto SavedRoot=Root->BalanceParameters,SavedEcho=Echo->BalanceParameters;
 const auto SavedVFX=Root->Presentation;const auto SavedBranchVFX=Echo->Presentation;
 TestTrue(TEXT("Migration exposes wave balance"),Root->bHasRuntimeBalance&&Root->BalanceParameters.Contains(TEXT("WaveWidth")));
 U->AcquireUpgrade(Root);U->AcquireUpgrade(Echo);
 Root->BalanceParameters.Add(TEXT("WaveWidth"),600);Root->BalanceParameters.Add(TEXT("WaveDamageMultiplier"),1.7f);
 TestEqual(TEXT("Blade Wave reads editable width"),A->Tuning(4,TEXT("WaveWidth"),300),600.f);
 TestEqual(TEXT("Blade Wave reads editable damage multiplier"),A->Tuning(4,TEXT("WaveDamageMultiplier"),0.65f),1.7f);
 A->Pending.Reset();A->BuildMarks.Reset();
 A->PreparationDamageMultiplier=2;A->PreparationDuration=9;
 A->RegisterFamilyHit(4,Enemy,20);const float BeforeReaction=Enemy->GetHealthComponent()->GetCurrentHealth();
 TestEqual(TEXT("Component setting controls universal preparation duration"),A->BuildMarks[0].Remaining,9.f);
 A->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemy,true);
 TestTrue(TEXT("Component setting controls universal preparation payout"),FMath::IsNearlyEqual(BeforeReaction-Enemy->GetHealthComponent()->GetCurrentHealth(),40,0.01f));
 // A real Niagara component receives the selected system, world placement and live radius parameters.
 auto* System=NewObject<UNiagaraSystem>(GetTransientPackage());
 Root->Presentation.PulseSystem=System;Root->Presentation.LocationOffset=FVector(10,20,30);Root->Presentation.AuthoredRadius=100;
 Root->Presentation.Scale=FVector(0.5f);Root->Presentation.LifetimeOverride=0.75f;
 auto* FX=A->FamilyAccent(4,FVector::ZeroVector,FVector(200,0,0),400,FLinearColor::White);
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
 TestTrue(TEXT("Owned branch can override the base presentation"),A->FamilyPresentation(4)==&Echo->Presentation);
 Root->BalanceParameters=SavedRoot;Echo->BalanceParameters=SavedEcho;Root->Presentation=SavedVFX;Echo->Presentation=SavedBranchVFX;
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
