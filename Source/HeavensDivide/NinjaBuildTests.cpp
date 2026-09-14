#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "NinjaBuildComponent.h"
#include "ShadowClone.h"
#include "EnemyStatusEffectComponent.h"
#include "NinjaCharacter.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Components/StaticMeshComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjaBuildsTest,"HeavensDivide.Combat.NinjaBuilds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNinjaBuildsTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Saved controller"),PC)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);auto* Manager=PC->GetCharacterManager();
 auto* ClassProperty=FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaClass"));
 auto* NinjaClass=Cast<UClass>(ClassProperty->GetObjectPropertyValue_InContainer(Manager));
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* N=World->SpawnActor<ANinjaCharacter>(NinjaClass,FVector::ZeroVector,FRotator::ZeroRotator,Params);
 if(!TestNotNull(TEXT("Saved Ninja Blueprint"),N)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 N->SetOwner(PC);N->SetCharacterMode(ECharacterMode::Active);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaCharacter"))->SetObjectPropertyValue_InContainer(Manager,N);
 FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Manager,N);
 auto* B=N->FindComponentByClass<UNinjaBuildComponent>();auto* A=N->FindComponentByClass<UAutoAttackComponent>();auto* U=PC->GetPlayerUpgrades();
 for(const auto* Id:{TEXT("ShadowStep"),TEXT("MultipleStrikes"),TEXT("AfterimageFrenzy")})TestNotNull(TEXT("Clone card retained"),U->FindUpgradeDefinition(Id));
 for(const auto* Id:{TEXT("Crossfire"),TEXT("ExecutionersKunai"),TEXT("FanOfBlades"),TEXT("NinjaProjectileBonus"),TEXT("NinjaProjectilePierce"),TEXT("ChainExecution"),TEXT("BladeCascade"),TEXT("ProjectileBounce"),TEXT("ProjectileSplit"),TEXT("PotentVenom"),TEXT("VirulentStrain"),TEXT("HemotoxicReaction"),TEXT("AcceleratedVenom")})
 {
  TestNull(TEXT("Legacy Ninja card absent"),U->FindUpgradeDefinition(Id));
  auto* Stale=NewObject<UUpgradeDefinition>(U);Stale->UpgradeId=Id;Stale->MaxLevel=1;
  TestFalse(TEXT("Stale legacy card cannot be acquired"),U->CanAcquireUpgrade(Stale));
 }
 if(!TestNotNull(TEXT("Saved Ninja inherits build component"),B)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 const auto SavedProjectileClass=A->ProjectileClass;
 A->OwnerCharacter=N;A->ProjectileClass=AAttackProjectileBase::StaticClass();A->ActiveAttackDirection=FVector::ForwardVector;
 auto Grant=[&](FName Id){auto* Card=U->FindUpgradeDefinition(Id);TestNotNull(*Id.ToString(),Card);return U->AcquireUpgrade(Card);};
 auto Spawn=[&](FVector Pos,float HP){auto* E=World->SpawnActor<AEnemyBase>(Pos,FRotator::ZeroRotator,Params);E->ConfigureObjectiveEnemy(HP,EPlayerAttackSource::Other,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(HP);FScriptDelegate D;D.BindUFunction(E,TEXT("HandleDeath"));E->GetHealthComponent()->OnDeath.AddUnique(D);return E;};
 auto* E=Spawn(FVector(300,0,0),100000);
 TestTrue(TEXT("Acquire Fang"),Grant(TEXT("ReturningFang")));
 TestFalse(TEXT("Ninja stances are exclusive"),U->CanAcquireUpgrade(U->FindUpgradeDefinition(TEXT("BarrageStance"))));
 A->AttackInterval=99;A->NextAttackReadyTime=100000;
 B->TickComponent(.1f,LEVELTICK_All,nullptr);TestTrue(TEXT("Fang launches despite normal cooldown"),B->Fang.IsValid());
 if(B->Fang.IsValid())
 {
  const float Before=E->GetHealthComponent()->GetCurrentHealth();
  for(int32 i=0;i<30&&B->Fang.IsValid();++i)B->Fang->Tick(.05f);
  TestTrue(TEXT("Fang returns and repeatedly damages without attack timer"),Before-E->GetHealthComponent()->GetCurrentHealth()>A->GetEffectiveAttackDamage()*1.5f);
  const float OldSpeed=B->Fang->Speed;
  FCharacterStatModifier M;M.ModifierId=TEXT("TestSpeed");M.Stat=ECharacterStatType::AttackSpeedMultiplier;M.Operation=EStatModifierOperation::Multiply;M.Value=2;N->GetCharacterStats()->AddModifier(M);
  B->Fang->LaunchFang();TestTrue(TEXT("Attack speed converts to blade speed"),FMath::IsNearlyEqual(B->Fang->Speed,OldSpeed*2,.1f));N->GetCharacterStats()->RemoveModifier(TEXT("TestSpeed"));
  N->SetCharacterMode(ECharacterMode::Inactive);for(int32 i=0;i<30&&B->Fang.IsValid();++i)B->Fang->Tick(.05f);
  TestFalse(TEXT("Inactive Ninja retrieves Fang without relaunch"),B->Fang.IsValid());N->SetCharacterMode(ECharacterMode::Active);
 }
 PC->NinjaBuildPreview(TEXT("Clear"));TestFalse(TEXT("Preview clear removes stance"),B->Has(TEXT("ReturningFang")));
 Grant(TEXT("BarrageStance"));A->AttackInterval=1;A->NextAttackReadyTime=0;
 auto Kunais=[&](){int32 Count=0;for(TActorIterator<AAttackProjectileBase> It(World);It;++It)++Count;return Count;};
 const int32 Before=Kunais();A->bIsAttacking=true;A->bAttackNotifyConsumed=false;A->SpawnAutoAttackProjectile();TestEqual(TEXT("Barrage produces its actual extra projectiles"),Kunais()-Before,A->GetEffectiveProjectileCount());
 Grant(TEXT("NeedleRain"));Grant(TEXT("Crescendo"));B->VolleyCount=3;B->ConsecutiveVolleys=2;
 FVector Dir=FVector::ForwardVector;int32 Count=3;float Spacing=10;B->ModifyVolley(Dir,Count,Spacing);TestEqual(TEXT("Crescendo and fourth-volley burst combine"),Count,8);
 N->SetCharacterMode(ECharacterMode::Inactive);B->TickComponent(.1f,LEVELTICK_All,nullptr);TestEqual(TEXT("Swap preserves Crescendo"),B->ConsecutiveVolleys,3);N->SetCharacterMode(ECharacterMode::Active);

 B->VolleyCount=0;B->ConsecutiveVolleys=14;Count=3;B->ModifyVolley(Dir,Count,Spacing);
 TestEqual(TEXT("Peak volley includes the full rank-one cap"),Count,8);
 TestEqual(TEXT("Crescendo resets only after peak volley"),B->ConsecutiveVolleys,0);
 Grant(TEXT("Crescendo"));B->ConsecutiveVolleys=20;Count=3;B->ModifyVolley(Dir,Count,Spacing);
 TestEqual(TEXT("Rank two raises temporary cap to seven"),Count,10);
 Count=3;B->ModifyVolley(Dir,Count,Spacing);TestEqual(TEXT("Next volley starts a fresh buildup"),Count,3);
 TestTrue(TEXT("Acquire forking"),Grant(TEXT("ForkingProjectiles")));
 TestEqual(TEXT("Fork upgrade enables splitting"),A->GetEffectiveProjectileSplitBonus(),1);
 TSet<AAttackProjectileBase*> ExistingProjectiles;for(TActorIterator<AAttackProjectileBase> It(World);It;++It)ExistingProjectiles.Add(*It);
 A->SpawnProjectileInstance(FVector(0,0,45),FVector::ForwardVector,40,1000,0,false);
 AAttackProjectileBase* Parent=nullptr;for(TActorIterator<AAttackProjectileBase> It(World);It;++It)if(!ExistingProjectiles.Contains(*It))Parent=*It;
 if(TestNotNull(TEXT("Fork parent spawned"),Parent))
 {
  ExistingProjectiles.Add(Parent);Parent->HandleProjectileOverlap(nullptr,E,nullptr,0,false,FHitResult());
  int32 Children=0;for(TActorIterator<AAttackProjectileBase> It(World);It;++It)if(!ExistingProjectiles.Contains(*It))
  {
   ++Children;TestEqual(TEXT("Fork deals half parent damage"),It->ProjectileDamage,20.f);
   TestFalse(TEXT("Fork cannot recursively fork"),It->bCanTriggerSplit);
   TestTrue(TEXT("Fork ignores original target"),It->DamagedEnemies.Contains(E));
  }
  TestEqual(TEXT("Impact creates exactly two fork projectiles"),Children,2);
 }
 PC->NinjaBuildPreview(TEXT("Clear"));Grant(TEXT("GreatShuriken"));
 A->bIsAttacking=true;A->bAttackNotifyConsumed=false;A->SpawnAutoAttackProjectile();
 ANinjaBuildProjectile* Wheel=nullptr;for(auto P:B->Projectiles)if(P.IsValid()&&P->Kind==1)Wheel=P.Get();
 if(TestNotNull(TEXT("Normal notify spawns a real Great Shuriken"),Wheel))
 {
  Wheel->SetActorLocation(E->GetActorLocation()+FVector(0,0,40));Wheel->Speed=0;
  const float HP=E->GetHealthComponent()->GetCurrentHealth();Wheel->Tick(.1f);const float First=E->GetHealthComponent()->GetCurrentHealth();
  Wheel->Tick(.1f);TestEqual(TEXT("Per-enemy cooldown prevents every-frame damage"),E->GetHealthComponent()->GetCurrentHealth(),First);
  Wheel->Tick(.1f);Wheel->Tick(.1f);TestTrue(TEXT("Overlapping shuriken repeats hits"),E->GetHealthComponent()->GetCurrentHealth()<First&&First<HP);
  Grant(TEXT("WideOrbit"));const float OriginalRadius=Wheel->Radius;
  Wheel->Tick(.1f);TestEqual(TEXT("Stationary shuriken does not grow"),Wheel->Radius,OriginalRadius);
  Wheel->Speed=1000;Wheel->Tick(.1f);TestTrue(TEXT("Travelling shuriken grows"),Wheel->Radius>OriginalRadius);
  TestTrue(TEXT("Visual size follows hit radius"),FMath::IsNearlyEqual(Wheel->Visual->GetComponentScale().X,Wheel->Radius/50.f));
  Grant(TEXT("BreakingWheel"));const int32 Old=B->Projectiles.Num();Wheel->Age=3;Wheel->Tick(.1f);TestTrue(TEXT("Shuriken expires without orbit and scatters blades"),Wheel->IsActorBeingDestroyed()&&B->Projectiles.Num()>Old);
 }
 PC->NinjaBuildPreview(TEXT("Clear"));Grant(TEXT("EmbeddedBlades"));
 auto* Weak=Spawn(FVector(100,100,0),1);const int32 Old=B->Projectiles.Num();B->Hit(Weak,20);
 TestTrue(TEXT("A one-hit kill still scatters its embedded blade"),Weak->IsDead()&&B->Projectiles.Num()>Old);
 auto* Tank=Spawn(FVector(500,0,0),100000);for(int32 i=0;i<20;++i)B->Hit(Tank,1);
 TestEqual(TEXT("Embedded stacks have a finite cap"),B->Embedded.FindChecked(Tank).Count,12);
 auto* FragmentVictim=Spawn(FVector(800,0,0),1);const int32 Existing=B->Projectiles.Num();B->Hit(FragmentVictim,20,false);
 TestEqual(TEXT("Scattered blade kills cannot scatter recursively"),B->Projectiles.Num(),Existing);
 PC->NinjaBuildPreview(TEXT("Fang"));TestTrue(TEXT("Fang preview grants its branches"),B->Has(TEXT("CuttingReturn"))&&B->Has(TEXT("EmbeddedBlades")));
 const int32 Mastery=U->GetNinjaMasteryPoints();PC->NinjaBuildPreview(TEXT("Fang"));TestEqual(TEXT("Preview does not farm mastery"),U->GetNinjaMasteryPoints(),Mastery);
 PC->NinjaBuildPreview(TEXT("Shuriken"));TestTrue(TEXT("Preview switches exclusive stances cleanly"),B->Has(TEXT("GreatShuriken"))&&!B->Has(TEXT("ReturningFang")));

 PC->NinjaBuildPreview(TEXT("Clear"));
 A->ProjectileClass=SavedProjectileClass;
 TestNotNull(TEXT("Original kunai impact sound remains assigned"),SavedProjectileClass->GetDefaultObject<AAttackProjectileBase>()->ImpactFeedback.HitSound.Get());
 TestTrue(TEXT("Poison starter can be acquired"),Grant(TEXT("VenomousKunai")));
 B->Hit(Tank,1);TestTrue(TEXT("Ninja hits apply acquired poison"),Tank->HasStatus(EEnemyStatusEffect::Poison));
 auto MakeClone=[&](){auto* C=World->SpawnActor<AShadowClone>(FVector(0,100,0),FRotator::ZeroRotator,Params);C->InitializeShadowClone(N,PC,2);C->bAttackInProgress=true;return C;};
 PC->NinjaBuildPreview(TEXT("Fang"));N->SetCharacterMode(ECharacterMode::Inactive);
 auto* Clone=MakeClone();Clone->HandleAttackProjectileNotify();
 TestTrue(TEXT("Inactive Ninja's clone launches its own Fang"),Clone->ReturningBlade.IsValid());
 if(Clone->ReturningBlade.IsValid())
 {
  TestTrue(TEXT("Fang uses original projectile presentation"),Clone->ReturningBlade->KunaiPresentation.IsValid());
  auto* Presentation=Clone->ReturningBlade->KunaiPresentation.Get();
  TestTrue(TEXT("Fang uses saved kunai Blueprint"),Presentation&&Presentation->GetClass()==SavedProjectileClass);
  TestTrue(TEXT("Original kunai mesh remains visible"),Presentation&&Presentation->VisualMesh->IsVisible());
  TestFalse(TEXT("Cosmetic kunai cannot deal duplicate collision damage"),Presentation&&Presentation->GetActorEnableCollision());
  for(int32 i=0;i<200&&Clone->ReturningBlade.IsValid();++i)Clone->ReturningBlade->Tick(.05f);
  TestEqual(TEXT("Clone Fang completes exactly its attack quota"),Clone->RemainingAttacks,0);
  TestTrue(TEXT("Clone waits for Fang return before disappearing"),Clone->bAttackQuotaFinished);
 }
 Clone->Destroy();
 PC->NinjaBuildPreview(TEXT("Shuriken"));
 auto* ShurikenCard=U->FindUpgradeDefinition(TEXT("GreatShuriken"));
 TestTrue(TEXT("Saved shuriken card exposes travel speed"),ShurikenCard->BalanceParameters.Contains(TEXT("TravelSpeed")));
 const auto SavedBalance=ShurikenCard->BalanceParameters;ShurikenCard->BalanceParameters.Add(TEXT("TravelSpeed"),321.f);
 Clone=MakeClone();const int32 BladeCount=B->Projectiles.Num();Clone->HandleAttackProjectileNotify();
 TestTrue(TEXT("Clone emits shuriken"),B->Projectiles.Num()>BladeCount&&B->Projectiles.Last()->Kind==1);
 TestEqual(TEXT("Clone shuriken uses editable travel speed"),B->Projectiles.Last()->Speed,321.f);Clone->Destroy();
 auto* SpeedBlade=B->SpawnShuriken(FVector::ZeroVector,FVector::ForwardVector,false);
 TestEqual(TEXT("Player shuriken uses same editable travel speed"),SpeedBlade->Speed,321.f);
 ShurikenCard->BalanceParameters=SavedBalance;
 PC->NinjaBuildPreview(TEXT("Barrage"));Clone=MakeClone();const int32 KunaiCount=Kunais();
 const int32 PlayerVolley=B->VolleyCount;Clone->HandleAttackProjectileNotify();
 TestTrue(TEXT("Clone fires Barrage projectiles"),Kunais()>KunaiCount);
 TestEqual(TEXT("Clone does not advance player volley counter"),B->VolleyCount,PlayerVolley);
 TestEqual(TEXT("Clone tracks its own volleys"),Clone->VolleyCount,1);Clone->Destroy();
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
