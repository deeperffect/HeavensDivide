#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AutoAttackComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "CharacterStatsComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "AnimNotify_SpawnSamuraiSlashNiagara.h"
#include "AnimNotify_PerformAutoAttackTrace.h"
#include "NiagaraComponent.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDoubleCut360Test,"HeavensDivide.Combat.DoubleCut360",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDoubleCut360Test::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto* PC=World->SpawnActor<ASurvivorPlayerController>();
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>();
 Samurai->SetOwner(PC);
 auto* Attack=Samurai->FindComponentByClass<UAutoAttackComponent>();
 Attack->OwnerCharacter=Samurai;
 Attack->AttackRadius=200;
 Attack->AttackForwardOffset=150;
 Attack->ImpactFeedback.bEnableCameraShake=false;
 const FVector Forward=Samurai->GetVisualForwardVector().GetSafeNormal2D();
 const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
 TArray<AEnemyBase*> Targets;
 for(int32 Index=0;Index<5;++Index)
 {
  auto* Enemy=World->SpawnActor<AEnemyBase>();
  Enemy->GetCapsuleComponent()->SetCapsuleSize(5,10);
  Targets.Add(Enemy);
 }
 for(float Area:{1.f,1.5f})
 {
  FCharacterStatModifier Modifier;
  Modifier.ModifierId=TEXT("DoubleCutAreaTest");
  Modifier.Stat=ECharacterStatType::AttackAreaMultiplier;
  Modifier.Operation=EStatModifierOperation::Multiply;
  Modifier.Value=Area;
  Samurai->GetCharacterStats()->AddModifier(Modifier);
  const float Radius=Attack->GetEffectiveAttackRadius();
  TestEqual(TEXT("Double Cut retains normal area radius"),Radius,200.f*Area);
  const FVector Directions[]={Forward,Right,-Forward,-Right,Forward};
  for(int32 Index=0;Index<5;++Index)
  {
   Targets[Index]->SetActorLocation(Samurai->GetActorLocation()+Directions[Index]*Radius*(Index==4?1.5f:.75f));
   Targets[Index]->GetHealthComponent()->RestoreCurrentHealth(100);
  }
  Attack->bDoubleCutFollowUpActive=false;
  Attack->ExecuteMeleeAttackTrace();
  TestEqual(TEXT("Ordinary swing retains its forward hitbox"),Targets[2]->GetHealthComponent()->GetCurrentHealth(),100.f);
  for(auto* Enemy:Targets) Enemy->GetHealthComponent()->RestoreCurrentHealth(100);
  Attack->bDoubleCutFollowUpActive=true;
  Attack->ExecuteMeleeAttackTrace();
  for(int32 Index=0;Index<4;++Index)
   TestTrue(TEXT("Spin hits front, rear, and both sides"),Targets[Index]->GetHealthComponent()->GetCurrentHealth()<100);
  TestEqual(TEXT("Spin does not increase radius"),Targets[4]->GetHealthComponent()->GetCurrentHealth(),100.f);
 }
 auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_DoubleCutSamurai.AM_DoubleCutSamurai"));
 UAnimNotify_SpawnSamuraiSlashNiagara* Notify=nullptr;
 float DamageTime=-1,EffectTime=-2;
 if(TestNotNull(TEXT("Saved Double Cut montage"),Montage))
  for(const auto& Event:Montage->Notifies)
  {
   if(Cast<UAnimNotify_PerformAutoAttackTrace>(Event.Notify)) DamageTime=Event.GetTriggerTime();
   if(auto* Slash=Cast<UAnimNotify_SpawnSamuraiSlashNiagara>(Event.Notify)) {Notify=Slash;EffectTime=Event.GetTriggerTime();}
  }
 TestTrue(TEXT("Spin visual starts on damage frame"),FMath::IsNearlyEqual(DamageTime,EffectTime));
 if(TestNotNull(TEXT("Saved radial slash notify"),Notify))
 {
  Notify->Notify(Samurai->GetMesh(),Montage,FAnimNotifyEventReference());
  TArray<UNiagaraComponent*> Effects;
  for(USceneComponent* Child:Samurai->GetMesh()->GetAttachChildren())
   if(auto* Effect=Cast<UNiagaraComponent>(Child)) Effects.Add(Effect);
  TestEqual(TEXT("Four slash arcs cover the circle"),Effects.Num(),4);
  for(int32 Index=0;Index<Effects.Num();++Index)
  {
   TestTrue(TEXT("Every arc shares the character center"),Effects[Index]->GetRelativeLocation().Equals(Effects[0]->GetRelativeLocation()));
   TestTrue(TEXT("Arcs preserve the same area scale"),Effects[Index]->GetRelativeScale3D().Equals(Effects[0]->GetRelativeScale3D()));
   const FQuat Expected=FRotator(0,90.f*Index,0).Quaternion()*Effects[0]->GetRelativeRotation().Quaternion();
   TestTrue(TEXT("Arc directions evenly cover 360 degrees"),Effects[Index]->GetRelativeRotation().Quaternion().Equals(Expected,.001));
  }
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return true;
}
#endif
