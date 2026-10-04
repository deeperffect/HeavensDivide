#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SamuraiBladeWave.h"
#include "SamuraiWaveField.h"
#include "SamuraiCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSetAccessor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundSlashMotionTest,"HeavensDivide.Combat.GroundSlashMotion",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FGroundSlashMotionTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(!TestNotNull(TEXT("Controller"),PC)) return false;
 auto* Abilities=PC->FindComponentByClass<USurvivorAbilityComponent>();
 Abilities->Controller=PC;Abilities->Upgrades=PC->GetPlayerUpgrades();
 auto* Definition=Abilities->TuningDefinition(4);
 TestTrue(TEXT("Saved upgrade enables vendor motion"),Definition && Definition->Presentation.bGroundSlashMotion);
 auto* Ground=World->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Ground);Ground->SetRootComponent(Box);
 Box->SetBoxExtent(FVector(5000,5000,10));Box->SetCollisionObjectType(ECC_WorldStatic);
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);
 Box->RegisterComponent();Ground->SetActorLocation(FVector(0,0,-10));
 auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector(0,0,100),FRotator::ZeroRotator);Samurai->SetOwner(PC);
 auto* Wave=World->SpawnActor<ASamuraiBladeWave>(FVector(100,0,100),FRotator::ZeroRotator);
 Wave->InitializeBladeWave(Samurai,PC->GetPlayerUpgrades(),FVector::ForwardVector,10,300,650,1400,true);
 TestTrue(TEXT("Ground mode selected from upgrade"),Wave->bGroundSlash);
 TestTrue(TEXT("Ground trace positions visual 10cm above floor"),FMath::IsNearlyEqual(Wave->GroundAnchor.Z,10.f,.1f));
 Wave->Tick(.2f);
 TestTrue(TEXT("No slowdown before delay"),FMath::IsNearlyEqual(Wave->Movement->Velocity.Size(),1400.f));
 for(int32 Step=0;Step<6;++Step) Wave->Tick(.05f);
 TestTrue(TEXT("Velocity eases down after vendor delay"),Wave->Movement->Velocity.Size()<1400.f && Wave->Movement->Velocity.Size()>0.f);
 auto Outbound=Wave->AssignedVisual;
 TestTrue(TEXT("Upgrade visual exists"),Outbound.IsValid());
 Wave->SetActorLocation(Wave->PhaseOrigin+Wave->PhaseDirection*650);
 Wave->Tick(.01f);
 TestTrue(TEXT("Range cap starts return"),Wave->bReturning);
 TestTrue(TEXT("Returning wave restores speed"),FMath::IsNearlyEqual(Wave->Movement->Velocity.Size(),1400.f));
 TestTrue(TEXT("Outbound particle tails survive return"),Outbound.IsValid()&&!Outbound->IsActorBeingDestroyed());
 TestTrue(TEXT("Return spawns a fresh oriented effect"),Wave->AssignedVisual.IsValid()&&Wave->AssignedVisual!=Outbound);
 auto Return=Wave->AssignedVisual;
 Wave->FinishWave();
 TestTrue(TEXT("Return particle tails survive projectile completion"),Return.IsValid()&&!Return->IsActorBeingDestroyed());
 if(Return.IsValid())
 {
  Return->Tick(.5f);
  TestFalse(TEXT("Particles are not cut off at old short wave lifetime"),Return->IsActorBeingDestroyed());
  Return->Tick(6.f);
  TestTrue(TEXT("Particles have bounded cleanup"),!Return.IsValid() || Return->IsActorBeingDestroyed());
 }
 auto* U=PC->GetPlayerUpgrades();
 TestTrue(TEXT("Acquire Crescent for the real wave presentation"),U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("BladeWave"))));
 auto SpawnWave=[&]
 {
  auto* W=World->SpawnActor<ASamuraiBladeWave>(FVector(100,0,100),FRotator::ZeroRotator);
  W->InitializeBladeWave(Samurai,U,FVector::ForwardVector,10,300,650,1400,true);
  W->Movement->Deactivate();
  return W;
 };
 auto CheckEmission=[&](ASamuraiBladeWave* W,bool bExpectDebris)
 {
  auto* Accent=W->AssignedVisual.Get();
  if(!TestNotNull(TEXT("Wave owns its ground visual"),Accent))return;
  auto* Effect=Accent->FindComponentByClass<UNiagaraComponent>();
  if(!TestNotNull(TEXT("Ground visual owns Niagara"),Effect))return;
  TSet<FName> Emitting;
  for(int32 Step=0;Step<40;++Step)
  {
   Accent->MoveAnchor(W->GroundAnchor+FVector(Step*15,0,0));
   Effect->AdvanceSimulation(1,.02f);
   if(auto Controller=Effect->GetSystemInstanceController())
    if(auto* Instance=Controller->GetSystemInstance_Unsafe())
     for(const auto& Emitter:Instance->GetEmitters())
      if(Emitter->GetNumParticles()>0)Emitting.Add(Emitter->GetEmitterHandle().GetName());
  }
  int32 DebrisLayers=0;
  for(const auto& Emitter:Effect->GetAsset()->GetEmitterHandles())
   if(Emitter.GetName().ToString().StartsWith(TEXT("Debris")))
   {
    ++DebrisLayers;
    TestTrue(TEXT("Shared Niagara asset retains enabled debris"),Emitter.GetIsEnabled());
    const bool bGroundLayer=Emitter.GetName().ToString().StartsWith(TEXT("DebrisGround"));
    TestEqual(FString::Printf(TEXT("%s follows the field proc; returns reuse deposited ground rocks"),*Emitter.GetName().ToString()),
     Emitting.Contains(Emitter.GetName()),bExpectDebris && (!W->bReturning || !bGroundLayer));
   }
  TestEqual(TEXT("All three debris layers are covered"),DebrisLayers,3);
  for(const TCHAR* Layer:{TEXT("SlashMeshTUT"),TEXT("DecalBrightTUT"),TEXT("Trail01"),TEXT("Trail02"),TEXT("Trail03"),TEXT("Trail04"),TEXT("StarParticles"),TEXT("StarParticles002")})
   TestTrue(FString::Printf(TEXT("%s still emits independently of the field proc"),Layer),Emitting.Contains(FName(Layer)));
 };
 auto* Regular=SpawnWave();CheckEmission(Regular,false);
 auto* Field=U->FindUpgradeDefinition(TEXT("CrescentField"));
 if(TestNotNull(TEXT("Lingering Wake asset"),Field))
 {
  const auto SavedBalance=Field->BalanceParameters;
  Field->BalanceParameters.Add(TEXT("Chance"),0.f);
  TestTrue(TEXT("Acquire Lingering Wake"),U->AcquireUpgrade(Field));
  Regular->SetActorLocation(Regular->LaunchOrigin+FVector(650,0,0));Regular->BeginReturn();
  CheckEmission(Regular,false); // An already committed regular wave stays unchanged.
  auto* FailedWave=SpawnWave();
  TestFalse(TEXT("Owned Lingering Wake can fail its field proc"),FailedWave->bFieldPending);
  CheckEmission(FailedWave,false);
  Field->BalanceParameters.Add(TEXT("Chance"),1.f);
  auto* WakeWave=SpawnWave();
  TestTrue(TEXT("Successful field proc enables debris"),WakeWave->bFieldPending);
  CheckEmission(WakeWave,true);
  // Returns retain the original result even if the field chance changes later.
  FailedWave->SetActorLocation(FailedWave->LaunchOrigin+FVector(650,0,0));FailedWave->BeginReturn();
  CheckEmission(FailedWave,false);
  TestTrue(TEXT("Acquire split waves"),U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("CrescentSplit"))));
  Field->BalanceParameters.Add(TEXT("Chance"),0.f);
  WakeWave->SpawnSplitWaves(nullptr);
  int32 Children=0;
  for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)if(!It->bCanSplit)
  {
   ++Children;It->Movement->Deactivate();CheckEmission(*It,true);
   TestTrue(TEXT("Split waves inherit success even with current chance zero"),It->bFieldPending);
   It->Destroy();
  }
  TestEqual(TEXT("Both split waves inherit the successful parent"),Children,2);
  Field->BalanceParameters.Add(TEXT("Chance"),1.f);
  FailedWave->SpawnSplitWaves(nullptr);
  Children=0;
  for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)if(!It->bCanSplit)
  {
   ++Children;It->Movement->Deactivate();CheckEmission(*It,false);
   TestFalse(TEXT("Split waves inherit failure even with current chance one"),It->bFieldPending);
   It->Destroy();
  }
  TestEqual(TEXT("Both split waves inherit the failed parent"),Children,2);
  Field->BalanceParameters.Add(TEXT("Chance"),0.f);
  FPlayerUpgradeRunState Empty;U->RestoreRunState(Empty);
  U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("BladeWave")));
  WakeWave->SetActorLocation(WakeWave->LaunchOrigin+FVector(650,0,0));WakeWave->BeginReturn();
  TestFalse(TEXT("Committing the completed strip consumes its pending flag"),WakeWave->bFieldPending);
  CheckEmission(WakeWave,true); // Consuming the field does not erase its successful proc.
  WakeWave->SpawnSplitWaves(nullptr);
  Children=0;
  for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)if(!It->bCanSplit)
  {
   ++Children;It->Movement->Deactivate();CheckEmission(*It,true);
   TestTrue(TEXT("Return splits inherit success after the parent field spawned"),It->bFieldPending);
  }
  TestEqual(TEXT("Both return splits retain the original field result"),Children,2);
  CheckEmission(SpawnWave(),false);
  // Real particle survival and field-owned cleanup: base duration, all duration
  // ranks, and the shorter eruption lifetime. No replacement indicator spawns.
  for(int32 Variant=0;Variant<3;++Variant)
  {
   for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)It->Destroy();
   for(TActorIterator<ASamuraiWaveField> It(World);It;++It)It->Destroy();
   for(TActorIterator<AAbilityAccent> It(World);It;++It)It->Destroy();
   U->RestoreRunState(Empty);U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("BladeWave")));U->AcquireUpgrade(Field);
   if(Variant==1)for(int32 Rank=0;Rank<5;++Rank)U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("CrescentFieldPower")));
   if(Variant==2)U->AcquireUpgrade(U->FindUpgradeDefinition(TEXT("CrescentEruptionPact")));
   Field->BalanceParameters.Add(TEXT("Chance"),1.f);
   auto* TrailWave=SpawnWave();TrailWave->SetActorTickEnabled(false);
   TWeakObjectPtr<AAbilityAccent> Rocks=TrailWave->AssignedVisual;
   auto* Effect=Rocks->FindComponentByClass<UNiagaraComponent>();
   for(int32 Step=0;Step<40;++Step)
   {
    TrailWave->SetActorLocation(TrailWave->FieldTrailStart+FVector(Step*15,0,0));
    TrailWave->FollowGround();Rocks->MoveAnchor(TrailWave->GroundAnchor);
    Effect->AdvanceSimulation(1,.02f);
   }
   auto ReadGround=[&]
   {
    TMap<FName,int32> Counts;
    if(auto Controller=Effect->GetSystemInstanceController())
     if(auto* Instance=Controller->GetSystemInstance_Unsafe())
      for(const auto& Emitter:Instance->GetEmitters())
       if(Emitter->GetEmitterHandle().GetName().ToString().StartsWith(TEXT("DebrisGround")))
       {
        Counts.Add(Emitter->GetEmitterHandle().GetName(),Emitter->GetNumParticles());
        auto Sizes=FNiagaraDataSetAccessor<FVector3f>::CreateReader(Emitter->GetParticleData(),TEXT("Scale"));
        if(TestTrue(TEXT("Ground rocks have a visible mesh scale"),Sizes.IsValid()))
         for(int32 Index=0;Index<Emitter->GetNumParticles();++Index)
          TestTrue(TEXT("Retained rocks remain visible, not merely alive"),Sizes.GetSafe(Index,FVector3f::ZeroVector).SizeSquared()>.001f);
       }
    return Counts;
   };
   const auto Deposited=ReadGround();
   TestEqual(TEXT("Both stationary debris layers are deposited"),Deposited.Num(),2);
   for(const auto& Entry:Deposited)TestTrue(TEXT("Ground layer contains rocks"),Entry.Value>0);
   TrailWave->FinishWave();
   ASamuraiWaveField* DamageField=nullptr;
   for(TActorIterator<ASamuraiWaveField> It(World);It;++It)DamageField=*It;
   if(TestNotNull(TEXT("Successful wave creates a damage field"),DamageField))
   {
    // This isolated test world does not begin the controller's gameplay. Start
    // the field lifecycle explicitly so Destroy dispatches its real EndPlay.
    DamageField->DispatchBeginPlay();
    TestTrue(TEXT("Field adopts the original rocks without restarting Niagara"),DamageField->Debris==Rocks);
    TestFalse(TEXT("Wave destruction leaves its field rocks alive"),Rocks->IsActorBeingDestroyed());
    auto* Mesh=Rocks->FindComponentByClass<UStaticMeshComponent>();
    TestTrue(TEXT("No blue fallback mesh"),!Mesh || !Mesh->IsVisible());
    const float Lifetime=Variant==2?.3f:DamageField->Remaining;
    auto AdvanceField=[&](float Seconds)
    {
     while(Seconds>KINDA_SMALL_NUMBER)
     {
      const float Delta=FMath::Min(.02f,Seconds);
      if(Rocks.IsValid()&&!Rocks->IsActorBeingDestroyed())Effect->AdvanceSimulation(1,Delta);
      ++GFrameCounter;World->Tick(LEVELTICK_All,Delta);Seconds-=Delta;
     }
    };
    AdvanceField(Lifetime-.05f);
    TestTrue(TEXT("Rocks survive until the field's final damage interval"),Rocks.IsValid()&&!Rocks->IsActorBeingDestroyed());
    if(Rocks.IsValid()&&!Rocks->IsActorBeingDestroyed())
    {
     const auto Retained=ReadGround();
     for(const auto& Entry:Deposited)
      TestEqual(TEXT("Even the first deposited rocks persist through the field"),Retained.FindRef(Entry.Key),Entry.Value);
    }
    AdvanceField(.15f);
    TestTrue(TEXT("Field expiry or eruption cleans up its deposited rocks"),!Rocks.IsValid()||Rocks->IsActorBeingDestroyed());
    int32 Visuals=0;for(TActorIterator<AAbilityAccent> It(World);It;++It)++Visuals;
    TestEqual(TEXT("No separate field indicator or eruption flash remains"),Visuals,0);
   }
  }
  Field->BalanceParameters=SavedBalance;
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return true;
}
#endif
