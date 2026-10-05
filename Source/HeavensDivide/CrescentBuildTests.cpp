#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CrescentBuild.h"
#include "AutoAttackComponent.h"
#include "SamuraiBladeWave.h"
#include "SamuraiWaveField.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "EnemyBase.h"
#include "EnemyLightweightMovementComponent.h"
#include "HealthComponent.h"
#include "HeavensDivideGameUserSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "SurvivorAbilityComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrescentBuildsTest,"HeavensDivide.Combat.CrescentBuilds",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCrescentBuildsTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const auto Cleanup=[&]{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* PCClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC=World->SpawnActor<ASurvivorPlayerController>(PCClass);
    if(!TestNotNull(TEXT("Saved controller"),PC)){Cleanup();return false;}
    PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
    auto* U=PC->GetPlayerUpgrades();
    auto Card=[&](FName Id){auto* C=U->FindUpgradeDefinition(Id);TestNotNull(*Id.ToString(),C);return C;};
    for(auto Id:{TEXT("BladeWave"),TEXT("CrescentDoubleCut"),TEXT("CrescentSplit"),TEXT("CrescentField"),TEXT("CrescentArc"),TEXT("CrescentDamage"),TEXT("CrescentAssist")})
        if(!Card(Id)){Cleanup();return false;}
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Manager=PC->GetCharacterManager();
    auto SpawnParty=[&](const TCHAR* ClassProperty)
    {
        auto* Class=Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),ClassProperty)->GetObjectPropertyValue_InContainer(Manager));
        return World->SpawnActor<ACharacterBase>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);
    };
    auto* Samurai=Cast<ASamuraiCharacter>(SpawnParty(TEXT("SamuraiClass")));
    auto* Ninja=Cast<ANinjaCharacter>(SpawnParty(TEXT("NinjaClass")));
    if(!Samurai||!Ninja){AddError(TEXT("Saved party missing"));Cleanup();return false;}
    auto SetParty=[&](const TCHAR* Property,ACharacterBase* Character){FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),Property)->SetObjectPropertyValue_InContainer(Manager,Character);};
    SetParty(TEXT("SamuraiCharacter"),Samurai);SetParty(TEXT("NinjaCharacter"),Ninja);SetParty(TEXT("ActiveCharacter"),Samurai);
    Samurai->SetOwner(PC);Ninja->SetOwner(PC);Samurai->SetCharacterMode(ECharacterMode::Active);Ninja->SetCharacterMode(ECharacterMode::Inactive);
    auto* Attack=Samurai->FindComponentByClass<UAutoAttackComponent>();Attack->OwnerCharacter=Samurai;Attack->ImpactFeedback.bEnableCameraShake=false;
    auto* NinjaAttack=Ninja->FindComponentByClass<UAutoAttackComponent>();NinjaAttack->OwnerCharacter=Ninja;Ninja->GetMesh()->InitAnim(true);
    auto* Assist=PC->FindComponentByClass<UInactiveCharacterAssistComponent>();Assist->SurvivorController=PC;Assist->PlayerUpgrades=U;
    auto Acquire=[&](FName Id){return U->AcquireUpgrade(Card(Id));};
    FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
    TestFalse(TEXT("Crescent upgrades require stance"),U->CanAcquireUpgrade(Card(TEXT("CrescentDamage"))));
    Acquire(TEXT("BladeWave"));FPlayerUpgradeRunState Base;U->CaptureRunState(Base);
    auto Advance=[&](float Seconds)
    {
        // UWorld clamps long frame deltas. Advance actual gameplay time in small frames.
        while(Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step=FMath::Min(.1f,Seconds);
            ++GFrameCounter;World->Tick(LEVELTICK_All,Step);Seconds-=Step;
        }
    };
    auto Enemy=[&](FVector Position,EPlayerAttackSource Restriction=EPlayerAttackSource::Other)
    {
        auto* E=World->SpawnActor<AEnemyBase>(Position,FRotator::ZeroRotator,Params);
        E->ConfigureObjectiveEnemy(10000,Restriction,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(10000);
        FScriptDelegate Death;Death.BindUFunction(E,TEXT("HandleDeath"));E->GetHealthComponent()->OnDeath.AddUnique(Death);return E;
    };
    auto Waves=[&]{TArray<ASamuraiBladeWave*> Result;for(TActorIterator<ASamuraiBladeWave> It(World);It;++It)Result.Add(*It);return Result;};
    auto Fields=[&]{TArray<ASamuraiWaveField*> Result;for(TActorIterator<ASamuraiWaveField> It(World);It;++It)Result.Add(*It);return Result;};
    auto Clear=[&]{for(auto* W:Waves())W->Destroy();for(auto* F:Fields())F->Destroy();};
    auto Wave=[&](FVector Position)
    {
        auto* W=World->SpawnActor<ASamuraiBladeWave>(Position,FRotator::ZeroRotator,Params);
        W->InitializeBladeWave(Samurai,U,FVector::ForwardVector,100,300,600,1000,false);
        W->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);W->Movement->Deactivate();return W;
    };
    auto Hit=[&](ASamuraiBladeWave* W,AEnemyBase* E){W->HandleOverlap(nullptr,E,nullptr,0,false,FHitResult());};
    auto CheckFieldBox=[&](ASamuraiWaveField* F,ASamuraiBladeWave* W)
    {
        const FVector Path=W->Collision->GetComponentLocation()-W->FieldTrailStart;
        const FVector Center=(W->FieldTrailStart+W->Collision->GetComponentLocation())*.5f;
        const FQuat Facing=Path.SizeSquared2D()>KINDA_SMALL_NUMBER?Path.GetSafeNormal2D().Rotation().Quaternion():W->Collision->GetComponentQuat();
        const FVector Extent=W->Collision->GetScaledBoxExtent()+FVector(Path.Size2D()*.5f,0,FMath::Abs(Path.Z)*.5f);
        TestTrue(TEXT("Field is centered on the completed path"),F->GetActorLocation().Equals(Center,.01f));
        TestTrue(TEXT("Field is aligned along the completed path"),F->GetActorQuat().Equals(Facing,.0001f));
        TestTrue(TEXT("Field sweeps the scaled wave box from start to finish"),F->HitboxHalfExtent.Equals(Extent,.01f));
        TestTrue(TEXT("Field does not apply wave scale twice"),F->GetActorScale3D().Equals(FVector::OneVector));
        TestNull(TEXT("Damage field has no separate mesh indicator"),F->FindComponentByClass<UStaticMeshComponent>());
        if(F->Debris.IsValid())
        {
            auto* Mesh=F->Debris->FindComponentByClass<UStaticMeshComponent>();
            TestTrue(TEXT("Deposited debris has no fallback rectangle"),!Mesh || !Mesh->IsVisible());
        }
    };
    // Exercise timer-driven waves with the saved alternating presentation montages.
    Samurai->GetMesh()->InitAnim(true);
    auto* SamuraiAnim=Samurai->GetMesh()->GetAnimInstance();
    if(!TestNotNull(TEXT("Samurai animation instance"),SamuraiAnim)){Cleanup();return false;}
    if(!TestNotNull(TEXT("Saved primary Crescent montage"),Attack->CrescentMontage.Get())
        || !TestNotNull(TEXT("Saved alternate Crescent montage"),Attack->CrescentAlternateMontage.Get())){Cleanup();return false;}
    TestEqual(TEXT("Primary uses supplied Blade Wave montage"),Attack->CrescentMontage->GetFName(),FName(TEXT("AM_SamuraiBladeWave")));
    TestEqual(TEXT("Alternate uses supplied second Blade Wave montage"),Attack->CrescentAlternateMontage->GetFName(),FName(TEXT("AM_SamuraiBladeWave2")));
    auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    const bool OriginalAutoTargeting=Settings ? Settings->IsAutoTargetingEnabled() : true;
    auto SetAutoTargeting=[&](bool Enabled)
    {
        if(Settings)FindFProperty<FBoolProperty>(UHeavensDivideGameUserSettings::StaticClass(),TEXT("bAutoTargetingEnabled"))->SetPropertyValue_InContainer(Settings,Enabled);
    };
    SetAutoTargeting(true);
    Samurai->SetActorLocation(FVector(2000,0,0));Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
    Attack->StopAutoAttack();Attack->NextAttackReadyTime=0;Attack->bAutoAttackEnabled=true;
    const auto OriginalMontage=Attack->AttackMontage;Attack->AttackMontage=nullptr;
    const float WaveReach=Attack->GetEffectiveTargetingRange();
    auto* RangedTarget=Enemy(Samurai->GetActorLocation()+FVector(0,WaveReach*.9f,0));
    TestTrue(TEXT("Crescent targeting reaches beyond normal melee"),WaveReach>Attack->AttackForwardOffset+Attack->GetEffectiveAttackRadius());
    Attack->HandleAttackTimer();
    TestEqual(TEXT("Attack timer fires one wave without the melee montage"),Waves().Num(),1);
    TestTrue(TEXT("First Crescent montage actually plays"),SamuraiAnim->Montage_IsPlaying(Attack->CrescentMontage));
    if(auto* Instance=SamuraiAnim->GetActiveInstanceForMontage(Attack->CrescentMontage))
        TestTrue(TEXT("Crescent montage cannot move the player with root motion"),Instance->IsRootMotionDisabled());
    TestTrue(TEXT("Crescent montage fits the attack interval"),FMath::IsNearlyEqual(
        Attack->CrescentMontage->GetPlayLength()/SamuraiAnim->Montage_GetPlayRate(Attack->CrescentMontage),Attack->GetEffectiveAttackInterval(),.001f));
    if(!Waves().IsEmpty())TestTrue(TEXT("Wave aims at the target independently of visual facing"),FVector::DotProduct(Waves()[0]->GetActorForwardVector(),FVector::RightVector)>.99f);
    TestFalse(TEXT("Wave-only attack does not enter melee animation state"),Attack->bIsAttacking);
    TestNull(TEXT("No normal melee montage is active"),Attack->ActiveAttackMontage.Get());
    TestTrue(TEXT("Wave-only attack keeps its normal cooldown"),Attack->NextAttackReadyTime>World->GetTimeSeconds());
    TestFalse(TEXT("Cooldown blocks an immediate second wave"),Attack->StartTargetedAttack());
    TestTrue(TEXT("Rejected attack keeps the primary montage playing"),SamuraiAnim->Montage_IsPlaying(Attack->CrescentMontage));
    TestEqual(TEXT("Distant target waits for the travelling wave"),RangedTarget->GetHealthComponent()->GetCurrentHealth(),10000.f);
    Clear();Attack->StopAutoAttack();
    TestFalse(TEXT("Stopping attacks stops Crescent presentation"),SamuraiAnim->Montage_IsPlaying(Attack->CrescentMontage));
    for(int32 Index=0;Index<2;++Index)
    {
        Attack->NextAttackReadyTime=0;
        TestTrue(TEXT("Next Crescent attack starts"),Attack->StartTargetedAttack());
        auto* Expected=Index==0?Attack->CrescentAlternateMontage.Get():Attack->CrescentMontage.Get();
        TestTrue(TEXT("Crescent alternates second then first montage"),SamuraiAnim->Montage_IsPlaying(Expected));
        TestEqual(TEXT("Each animated attack emits exactly one wave"),Waves().Num(),1);
        Clear();Attack->StopAutoAttack();
    }
    const auto PrimaryCrescent=Attack->CrescentMontage,AlternateCrescent=Attack->CrescentAlternateMontage;
    Attack->CrescentAlternateMontage=nullptr;Attack->NextAttackReadyTime=0;
    TestTrue(TEXT("Missing alternate still attacks"),Attack->StartTargetedAttack());
    TestTrue(TEXT("Missing alternate plays primary"),SamuraiAnim->Montage_IsPlaying(PrimaryCrescent));
    Clear();Attack->StopAutoAttack();
    Attack->CrescentMontage=nullptr;Attack->NextAttackReadyTime=0;
    TestTrue(TEXT("No presentation montage still fires waves"),Attack->StartTargetedAttack());
    TestEqual(TEXT("Missing animation preserves one wave"),Waves().Num(),1);
    Clear();Attack->StopAutoAttack();
    Attack->CrescentMontage=PrimaryCrescent;Attack->CrescentAlternateMontage=AlternateCrescent;
    Attack->NextAttackReadyTime=0;Attack->StartTargetedAttack();Clear();
    Samurai->StartDashVisual(.2f,FVector::ForwardVector);
    auto* DashMontage=SamuraiAnim->GetCurrentActiveMontage();
    if(TestNotNull(TEXT("Dash montage starts"),DashMontage))
        TestEqual(TEXT("Current montage is the saved Samurai dash"),DashMontage->GetFName(),FName(TEXT("AM_DashSamurai")));
    TestFalse(TEXT("Dash interrupts the Crescent montage"),SamuraiAnim->Montage_IsPlaying(Attack->ActiveCrescentMontage));
    Attack->StopAutoAttack();
    TestTrue(TEXT("Stopping Crescent does not stop the dash montage"),SamuraiAnim->Montage_IsPlaying(DashMontage));
    TestFalse(TEXT("Crescent cannot interrupt a dash"),Attack->StartTargetedAttack());
    Samurai->EndDashVisual();SamuraiAnim->Montage_Stop(0.f);
    auto* MeleeTarget=Enemy(Samurai->GetActorLocation()+FVector(Attack->AttackForwardOffset,0,0));
    Attack->bIsAttacking=true;Attack->bAttackNotifyConsumed=false;Attack->PerformAttackTrace();
    TestEqual(TEXT("Stale melee notify deals no Crescent melee damage"),MeleeTarget->GetHealthComponent()->GetCurrentHealth(),10000.f);
    TestEqual(TEXT("Stale melee notify cannot duplicate a wave"),Waves().Num(),0);
    TestFalse(TEXT("Direct melee trace is disabled for ordinary Crescent attacks"),Attack->ExecuteMeleeAttackTrace());
    Attack->bIsAttacking=false;Attack->NextAttackReadyTime=0;
    Samurai->bComboAbilityActive=true;
    TestFalse(TEXT("Combo ability blocks wave-only attacks"),Attack->StartTargetedAttack());
    Samurai->bComboAbilityActive=false;
    Acquire(TEXT("GrandEntrance"));Attack->ArmGrandEntranceAfterSwap();
    TestTrue(TEXT("Crescent still fires with Grand Entrance armed"),Attack->StartTargetedAttack());
    TestFalse(TEXT("Wave launch consumes the separate Grand Entrance proc"),Attack->bGrandEntranceReady);
    TestTrue(TEXT("Grand Entrance retains its special circular damage"),MeleeTarget->GetHealthComponent()->GetCurrentHealth()<10000.f);
    Clear();Attack->StopAutoAttack();Attack->AttackMontage=OriginalMontage;Attack->ActiveAttackDirection=FVector::ZeroVector;
    Attack->NextAttackReadyTime=0;RangedTarget->Destroy();MeleeTarget->Destroy();U->RestoreRunState(Base);SetAutoTargeting(OriginalAutoTargeting);
    auto* W=Wave(FVector(5000,0,60));auto* Target=Enemy(FVector(5000,0,0));auto* Restricted=Enemy(FVector(5200,0,0),EPlayerAttackSource::Ninja);
    Hit(W,Target);Hit(W,Target);Hit(W,Restricted);
    TestEqual(TEXT("Wave hits each enemy once"),Target->GetHealthComponent()->GetCurrentHealth(),9900.f);
    TestEqual(TEXT("Wave respects source restriction"),Restricted->GetHealthComponent()->GetCurrentHealth(),10000.f);
    TestTrue(TEXT("Crescent hit slows by 30 percent"),FMath::IsNearlyEqual(Target->GetCrescentMovementMultiplier(),.7f));
    TestEqual(TEXT("Restricted enemy is not slowed"),Restricted->GetCrescentMovementMultiplier(),1.f);
    Clear();
    auto* Mover=Target->FindComponentByClass<UEnemyLightweightMovementComponent>();
    Mover->SetMovementEnabled(true);Mover->SetMoveSpeed(100);Mover->RefreshSpawnZ();Mover->RequestMove(FVector::RightVector);
    const FVector Before=Target->GetActorLocation();Mover->TickComponent(1.f,LEVELTICK_All,nullptr);Mover->StopMovement();
    TestTrue(TEXT("Slow changes actual movement"),FMath::IsNearlyEqual(FVector::Dist2D(Before,Target->GetActorLocation()),70.f,.1f));
    Advance(4.f);Target->ApplyCrescentSlow(.3f,5.f);Advance(1.1f);
    TestTrue(TEXT("Reapplication refreshes five seconds"),Target->GetCrescentMovementMultiplier()<1.f);
    Advance(4.f);TestEqual(TEXT("Slow expires without stacking permanent speed changes"),Target->GetCrescentMovementMultiplier(),1.f);
    for(int32 Rank=0;Rank<5;++Rank)Acquire(TEXT("CrescentSlow"));
    U->HandleSamuraiDirectHit(Target,1,10000);TestTrue(TEXT("Slow scaling reaches 55 percent"),FMath::IsNearlyEqual(Target->GetCrescentMovementMultiplier(),.45f));
    U->RestoreRunState(Base);Acquire(TEXT("CrescentDoubleCut"));Attack->CrossingBladesAttackCounter=0;
    Samurai->SetActorLocation(FVector(10000,0,0));Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
    for(int32 Index=0;Index<4;++Index)
    {
        Attack->SpawnBladeWavesForAttack(100);auto Spawned=Waves();
        TestEqual(TEXT("Every fourth attack fires four waves"),Spawned.Num(),Index==3?4:1);
        if(Index==3)for(float Angle:{0.f,90.f,180.f,270.f})
            TestTrue(TEXT("Plus pattern covers each direction"),Spawned.ContainsByPredicate([Angle](const ASamuraiBladeWave* V){return FVector::DotProduct(V->GetActorForwardVector(),FVector::ForwardVector.RotateAngleAxis(Angle,FVector::UpVector))>.99f;}));
        Clear();
    }
    for(int32 Rank=0;Rank<3;++Rank)TestTrue(TEXT("Useful frequency rank acquired"),Acquire(TEXT("CrescentDoubleCutFrequency")));
    TestFalse(TEXT("Redundant fourth Crescent frequency rank rejected"),Acquire(TEXT("CrescentDoubleCutFrequency")));
    Acquire(TEXT("CrescentArc"));auto* Arc=Card(TEXT("CrescentArc"));const auto ArcBalance=Arc->BalanceParameters;Arc->BalanceParameters.Add(TEXT("Chance"),1.f);
    Attack->SpawnBladeWavesForAttack(100);TestEqual(TEXT("Arc combines with each Double Cut direction"),Waves().Num(),12);Clear();
    Arc->BalanceParameters=ArcBalance;U->RestoreRunState(Base);
    Attack->SpawnBladeWavesForAttack(100);auto* Original=Waves()[0];const float BaseDamage=Original->Damage,BaseSpeed=Original->Speed,BaseRange=Original->TravelDistance;Clear();
    Acquire(TEXT("CrescentDamage"));Acquire(TEXT("CrescentSpeed"));Acquire(TEXT("CrescentRange"));
    Attack->SpawnBladeWavesForAttack(100);auto* Scaled=Waves()[0];
    TestTrue(TEXT("Wave damage scales"),FMath::IsNearlyEqual(Scaled->Damage,BaseDamage*1.2f));
    TestTrue(TEXT("Wave speed scales"),FMath::IsNearlyEqual(Scaled->Speed,BaseSpeed*1.2f));
    TestTrue(TEXT("Wave range scales"),FMath::IsNearlyEqual(Scaled->TravelDistance,BaseRange*1.2f));Clear();
    U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));Acquire(TEXT("CrescentFieldPact"));Acquire(TEXT("CrescentPowerPact"));
    Attack->SpawnBladeWavesForAttack(100);auto* PactWave=Waves()[0];
    TestTrue(TEXT("Wave damage tradeoffs multiply"),FMath::IsNearlyEqual(PactWave->Damage,BaseDamage*.7f*1.5f));
    TestTrue(TEXT("Power pact halves speed"),FMath::IsNearlyEqual(PactWave->Speed,BaseSpeed*.5f));
    TestEqual(TEXT("Wave tradeoffs do not reduce field damage basis"),PactWave->FieldDamage,BaseDamage);Clear();
    U->RestoreRunState(Base);Acquire(TEXT("CrescentSplit"));Acquire(TEXT("CrescentField"));
    auto* Split=Card(TEXT("CrescentSplit"));auto* FieldCard=Card(TEXT("CrescentField"));
    const auto SplitBalance=Split->BalanceParameters,FieldBalance=FieldCard->BalanceParameters;
    TestEqual(TEXT("Saved split angle sends branches sideways"),Split->GetBalanceValue(TEXT("Angle"),0.f),90.f);
    Split->BalanceParameters.Add(TEXT("Chance"),1.f);FieldCard->BalanceParameters.Add(TEXT("Chance"),1.f);
    auto* SplitWave=Wave(FVector(15000,0,60));
    FieldCard->BalanceParameters.Add(TEXT("Chance"),0.f);
    auto* SplitEnemy=Enemy(FVector(15000,0,0));Hit(SplitWave,SplitEnemy);
    TestEqual(TEXT("One split makes two child waves"),Waves().Num(),3);
    TestEqual(TEXT("First hit does not truncate the field to one wave footprint"),Fields().Num(),0);
    for(auto* Child:Waves())if(Child!=SplitWave)
    {
        Child->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);Child->Movement->Deactivate();
        TestFalse(TEXT("Split waves cannot split again"),Child->bCanSplit);
        TestTrue(TEXT("Child inherits successful field despite zero current chance"),Child->bFieldPending);
        TestEqual(TEXT("Split damage is half"),Child->Damage,50.f);
        const auto BeforeFields=Fields();
        const int32 BeforeWaves=Waves().Num();
        auto* ChildTarget=Enemy(FVector(15500,500,0));Hit(Child,ChildTarget);
        TestEqual(TEXT("Child hits do not create more waves"),Waves().Num(),BeforeWaves);
        Child->SetActorLocation(Child->FieldTrailStart+Child->GetActorForwardVector()*200.f);
        Child->FinishWave();
        for(auto* F:Fields())if(!BeforeFields.Contains(F))CheckFieldBox(F,Child);
        TestTrue(TEXT("Split waves apply slow"),ChildTarget->GetCrescentMovementMultiplier()<1.f);
    }
    TestEqual(TEXT("Each finished child leaves its own strip"),Fields().Num(),2);
    Hit(SplitWave,Restricted);TestEqual(TEXT("Further parent hits do not create a premature field"),Fields().Num(),2);
    SplitWave->FinishWave();TestEqual(TEXT("Finished parent also leaves one strip"),Fields().Num(),3);Clear();

    // Sweep real projectiles through an off-center first target, a target farther
    // ahead, and targets to either side. Do not inject overlap callbacks here.
    // Let these spawned actors enter play so Unreal dispatches overlap events;
    // the surrounding cases deliberately drive damage callbacks themselves.
    const bool bWorldWasPlaying=World->HasBegunPlay();
    World->SetBegunPlay(true);
    const FVector BranchTestStart(100000,0,100);
    const FVector Forward=FRotator(0,37,0).Vector();
    const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
    auto* FirstBranchTarget=Enemy(BranchTestStart+Forward*150.f+Right*40.f);
    auto* AheadTarget=Enemy(BranchTestStart+Forward*350.f);
    auto* LeftTarget=Enemy(FirstBranchTarget->GetActorLocation()-Right*280.f);
    auto* RightTarget=Enemy(FirstBranchTarget->GetActorLocation()+Right*280.f);
    auto* PiercingWave=World->SpawnActor<ASamuraiBladeWave>(Attack->BladeWaveClass,BranchTestStart,Forward.Rotation(),Params);
    PiercingWave->InitializeBladeWave(Samurai,U,Forward,100,300,600,1000,false);
    auto* OriginalVisual=PiercingWave->AssignedVisual.Get();
    for(int32 Frame=0;Frame<45;++Frame)
        for(auto* MovingWave:Waves())
        {
            MovingWave->Movement->TickComponent(.01f,LEVELTICK_All,nullptr);
            if(!MovingWave->IsActorBeingDestroyed())MovingWave->Tick(.01f);
        }
    TestEqual(TEXT("Real first collision creates exactly two branches"),Waves().Num(),3);
    TestFalse(TEXT("Splitting does not consume the original wave"),PiercingWave->IsActorBeingDestroyed());
    TestTrue(TEXT("Original keeps travelling beyond the first enemy"),FVector::DotProduct(PiercingWave->GetActorLocation()-FirstBranchTarget->GetActorLocation(),Forward)>100.f);
    TestTrue(TEXT("Original remains moving forwards after splitting"),FVector::DotProduct(PiercingWave->Movement->Velocity,Forward)>0.f);
    TestEqual(TEXT("Original retains its full travel range"),PiercingWave->TravelDistance,600.f);
    TestTrue(TEXT("Original keeps its launch point"),PiercingWave->LaunchOrigin.Equals(BranchTestStart));
    TestEqual(TEXT("Original keeps its existing slash visual"),PiercingWave->AssignedVisual.Get(),OriginalVisual);
    TestEqual(TEXT("Original damages the next enemy at full strength"),AheadTarget->GetHealthComponent()->GetCurrentHealth(),9900.f);
    TestEqual(TEXT("Branches do not immediately hit their triggering enemy"),FirstBranchTarget->GetHealthComponent()->GetCurrentHealth(),9900.f);
    TestEqual(TEXT("Left branch damages enemies beside the first hit"),LeftTarget->GetHealthComponent()->GetCurrentHealth(),9950.f);
    TestEqual(TEXT("Right branch damages enemies beside the first hit"),RightTarget->GetHealthComponent()->GetCurrentHealth(),9950.f);
    for(auto* Child:Waves())if(Child!=PiercingWave)
    {
        TestTrue(TEXT("Branches start at the struck enemy instead of the approaching wave"),Child->LaunchOrigin.Equals(FirstBranchTarget->GetActorLocation(),.01f));
        TestTrue(TEXT("Branches travel perpendicular to the incoming wave"),FMath::Abs(FVector::DotProduct(Child->GetActorForwardVector(),Forward))<.001f);
        TestFalse(TEXT("Side branches cannot recursively split"),Child->bCanSplit);
        TestFalse(TEXT("Side branches do not return"),Child->bReturns);
    }
    Clear();
    for(auto* E:{FirstBranchTarget,AheadTarget,LeftTarget,RightTarget})E->Destroy();
    World->SetBegunPlay(bWorldWasPlaying);
    auto* DryWave=Wave(FVector(16500,0,60));
    TestFalse(TEXT("Parent field roll fails at zero chance"),DryWave->bFieldPending);
    FieldCard->BalanceParameters.Add(TEXT("Chance"),1.f);
    Hit(DryWave,SplitEnemy);
    TestEqual(TEXT("Failed field proc still allows two split children"),Waves().Num(),3);
    for(auto* Child:Waves())if(Child!=DryWave)
    {
        TestFalse(TEXT("Child inherits failed field despite guaranteed current chance"),Child->bFieldPending);
        Child->SetActorLocation(Child->FieldTrailStart+Child->GetActorForwardVector()*200.f);
        Child->FinishWave();
    }
    DryWave->FinishWave();
    TestEqual(TEXT("Neither failed parent nor its splits leave fields"),Fields().Num(),0);Clear();
    auto* Missed=Wave(FVector(17000,0,60));Missed->SetActorRotation(FRotator(0,70,0));
    Missed->SetActorLocation(Missed->FieldTrailStart+Missed->GetActorForwardVector()*450.f);
    Missed->FinishWave();TestEqual(TEXT("A wave that misses still leaves its whole travelled strip"),Fields().Num(),1);
    if(Fields().Num()==1)
        CheckFieldBox(Fields()[0],Missed);
    Clear();
    Split->BalanceParameters=SplitBalance;FieldCard->BalanceParameters=FieldBalance;
    for(int32 Rank=0;Rank<5;++Rank)Acquire(TEXT("CrescentSplitChance"));
    TestTrue(TEXT("Split chance caps at 90 percent with five ranks"),FMath::IsNearlyEqual(CrescentBuild::Chance(U,TEXT("CrescentSplit"),TEXT("CrescentSplitChance"),.15f,.15f),.9f));
    U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));for(int32 Rank=0;Rank<5;++Rank)Acquire(TEXT("CrescentFieldPower"));Acquire(TEXT("CrescentFieldPact"));
    auto* FieldTarget=Enemy(FVector(20000,0,0));auto* FieldRestricted=Enemy(FVector(20050,0,0),EPlayerAttackSource::Ninja);
    auto* Field=World->SpawnActor<ASamuraiWaveField>(FVector(20000,0,0),FRotator::ZeroRotator,Params);Field->Initialize(Samurai,U,100,FVector(27.5,150,60));
    TestEqual(TEXT("Field duration gains 75 percent"),Field->Remaining,5.25f);
    TestTrue(TEXT("Only the field pact increases DPS"),FMath::IsNearlyEqual(Field->DamagePerSecond,45.f,.001f));
    const float FieldTotal=Field->TotalDamage;
    TestTrue(TEXT("Five ranks and field pact apply once to total damage"),FMath::IsNearlyEqual(FieldTotal,90.f*1.75f*1.5f,.001f));
    for(int32 Step=0;Step<11;++Step)Advance(.5f);
    TestTrue(TEXT("Field deals its complete finite damage budget"),FMath::IsNearlyEqual(10000-FieldTarget->GetHealthComponent()->GetCurrentHealth(),FieldTotal,.02f));
    TestEqual(TEXT("Fields respect source restrictions"),FieldRestricted->GetHealthComponent()->GetCurrentHealth(),10000.f);
    TestEqual(TEXT("Expired field is removed"),Fields().Num(),0);
    Acquire(TEXT("CrescentEruptionPact"));auto* EruptionTarget=Enemy(FVector(25000,0,0));
    U->HandleSamuraiDirectHit(EruptionTarget,1,10000);TestEqual(TEXT("Eruption pact removes slow"),EruptionTarget->GetCrescentMovementMultiplier(),1.f);
    auto* Eruption=World->SpawnActor<ASamuraiWaveField>(FVector(25000,0,0),FRotator::ZeroRotator,Params);Eruption->Initialize(Samurai,U,100,FVector(27.5,150,60));
    Advance(.29f);TestEqual(TEXT("Eruption waits 0.3 seconds"),EruptionTarget->GetHealthComponent()->GetCurrentHealth(),10000.f);
    Advance(.02f);TestTrue(TEXT("Eruption deals the full scaled field budget"),FMath::IsNearlyEqual(10000-EruptionTarget->GetHealthComponent()->GetCurrentHealth(),FieldTotal,.02f));
    // Measure actual normal-field and eruption damage at every rank: each adds
    // 15% of the original total, with no extra DPS multiplier or rounding loss.
    for(int32 Rank=0;Rank<=5;++Rank)
    {
        U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));
        for(int32 Purchase=0;Purchase<Rank;++Purchase)Acquire(TEXT("CrescentFieldPower"));
        const float ExpectedDuration=3.f*(1.f+.15f*Rank),ExpectedTotal=90.f*(1.f+.15f*Rank);
        const FVector Location(45000+Rank*1000,0,0);
        auto* Victim=Enemy(Location);
        auto* Normal=World->SpawnActor<ASamuraiWaveField>(Location,FRotator::ZeroRotator,Params);Normal->Initialize(Samurai,U,100,FVector(27.5,150,60));
        TestTrue(TEXT("Field duration increases linearly per rank"),FMath::IsNearlyEqual(Normal->Remaining,ExpectedDuration,.001f));
        TestTrue(TEXT("Field DPS stays unchanged at every rank"),FMath::IsNearlyEqual(Normal->DamagePerSecond,30.f,.001f));
        Advance(ExpectedDuration+.1f);
        TestTrue(TEXT("Actual full-duration damage increases linearly"),FMath::IsNearlyEqual(10000-Victim->GetHealthComponent()->GetCurrentHealth(),ExpectedTotal,.03f));
        Victim->GetHealthComponent()->RestoreCurrentHealth(10000);
        Acquire(TEXT("CrescentEruptionPact"));
        auto* Burst=World->SpawnActor<ASamuraiWaveField>(Location,FRotator::ZeroRotator,Params);Burst->Initialize(Samurai,U,100,FVector(27.5,150,60));
        Advance(.29f);TestEqual(TEXT("Every eruption waits for its delay"),Victim->GetHealthComponent()->GetCurrentHealth(),10000.f);
        Advance(.02f);
        TestTrue(TEXT("Actual eruption damage matches normal field at every rank"),FMath::IsNearlyEqual(10000-Victim->GetHealthComponent()->GetCurrentHealth(),ExpectedTotal,.03f));
        Clear();Victim->Destroy();
    }
    U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));
    TestTrue(TEXT("Crescent Shrine offers its tradeoffs"),U->BeginBloodShrineSelection(3));TestEqual(TEXT("All three Crescent tradeoffs are eligible with field"),U->GetCurrentUpgradeChoices().Num(),3);
    for(auto* C:U->GetCurrentUpgradeChoices())TestTrue(TEXT("Shrine only offers own stance"),C->UpgradeId.ToString().StartsWith(TEXT("Crescent")));
    U->BeginDirectUpgradeSelection(1000);for(auto* C:U->GetCurrentUpgradeChoices())TestFalse(TEXT("Ordinary rewards exclude Crescent pacts"),C->UpgradeId.ToString().StartsWith(TEXT("Crescent"))&&C->Category==EUpgradeCategory::Cursed);
    for(const auto& Offer:U->GetCurrentUpgradeOffers())
        if(Offer.UpgradeDefinition && Offer.UpgradeDefinition->UpgradeId==TEXT("CrescentFieldPower"))
        {TestTrue(TEXT("Rare Crescent scaling displays rarity"),Offer.bDisplaysRarity);TestEqual(TEXT("Crescent scaling retains Rare tier"),Offer.RolledRarity,EUpgradeRarity::Rare);}
    Acquire(TEXT("CrescentAssist"));for(int32 Rank=0;Rank<5;++Rank)Acquire(TEXT("CrescentAssistChance"));
    TestTrue(TEXT("Assist chance reaches 30 percent"),FMath::IsNearlyEqual(CrescentBuild::Chance(U,TEXT("CrescentAssist"),TEXT("CrescentAssistChance"),.05f,.05f),.3f));
    auto* AssistCard=Card(TEXT("CrescentAssist"));const auto AssistBalance=AssistCard->BalanceParameters;AssistCard->BalanceParameters.Add(TEXT("Chance"),1.f);
    Samurai->SetActorLocation(FVector(30000,0,0));auto* AssistVictim=Enemy(FVector(30200,0,0));Enemy(FVector(30300,0,0));
    AssistVictim->ApplyPlayerDamage(20000,EPlayerAttackSource::Samurai);Advance(.01f);
    TestTrue(TEXT("A kill requests the equipped Ninja assist"),Assist->bAssistActive);
    TestFalse(TEXT("Busy assist cannot overlap itself"),Assist->TryBloodAssist());Assist->DeactivateAssistEffect(true);
    SetParty(TEXT("ActiveCharacter"),Ninja);TestFalse(TEXT("Ninja-active kills do not trigger Crescent assist"),CrescentBuild::TryKillAssist(World));SetParty(TEXT("ActiveCharacter"),Samurai);
    AssistCard->BalanceParameters=AssistBalance;
    FPlayerUpgradeRunState Legacy=Base;Legacy.SamuraiMastery=42;
    for(auto Id:{TEXT("SamuraiHeavyBlade"),TEXT("SamuraiTempo"),TEXT("SamuraiArea")}){Legacy.Levels.Add(Id,1);Legacy.Definitions.Add(Id,Card(Id));Legacy.AccumulatedMagnitudes.Add(Id,.5f);}
    U->RestoreRunState(Legacy);TestEqual(TEXT("Crescent save keeps mastery"),U->GetSamuraiMasteryPoints(),42);
    for(auto Id:{TEXT("SamuraiHeavyBlade"),TEXT("SamuraiTempo"),TEXT("SamuraiArea")}){TestEqual(TEXT("Old generic scaling is removed"),U->GetUpgradeLevelById(Id),0);TestEqual(TEXT("Old magnitudes are removed"),U->GetAccumulatedUpgradeMagnitude(Id),0.f);}
    for(auto Id:{TEXT("CrescentDamage"),TEXT("CrescentSpeed"),TEXT("CrescentRange")})TestEqual(TEXT("Generic ranks convert to Crescent"),U->GetUpgradeLevelById(Id),1);
    Acquire(TEXT("CrescentDamage"));FPlayerUpgradeRunState Other;U->CaptureRunState(Other);Other.Levels.Remove(TEXT("BladeWave"));Other.Definitions.Remove(TEXT("BladeWave"));Other.Levels.Add(TEXT("Iaijutsu"),1);Other.Definitions.Add(TEXT("Iaijutsu"),Card(TEXT("Iaijutsu")));U->RestoreRunState(Other);
    TestFalse(TEXT("Other stance cannot retain Crescent ranks"),U->HasUpgradeId(TEXT("CrescentDamage")));
    TestEqual(TEXT("Changing saved stance converts damage ranks"),U->GetUpgradeLevelById(TEXT("IaijutsuDamage")),2);
    // An elevated, rotated and non-uniformly scaled wave leaves an exact box
    // snapshot. Test points distinguish it from both the old circle and an AABB.
    U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));
    const FVector BoxCenter(65000,0,300);const FRotator BoxRotation(0,45,0);
    const FVector BoxScale(1.25,1.5,2),UnscaledExtent(35,200,60),BoxExtent=UnscaledExtent*BoxScale;
    auto MakeBoxField=[&]()
    {
        auto* V=Wave(BoxCenter);V->SetActorRotation(BoxRotation);V->SetActorScale3D(BoxScale);
        V->Collision->SetBoxExtent(UnscaledExtent);V->bFieldPending=true;V->SpawnField();
        const auto Spawned=Fields();
        if(!TestEqual(TEXT("Exactly one field spawned"),Spawned.Num(),1)){Clear();return static_cast<ASamuraiWaveField*>(nullptr);}
        auto* F=Spawned[0];CheckFieldBox(F,V);
        V->SetActorLocation(BoxCenter+FVector(1000,0,0));V->SetActorRotation(FRotator::ZeroRotator);V->SetActorScale3D(FVector(3));
        TestTrue(TEXT("Moving and resizing source cannot change field"),F->GetActorLocation().Equals(BoxCenter)
            && F->HitboxHalfExtent.Equals(BoxExtent) && F->GetActorRotation().Equals(BoxRotation));
        V->Destroy();return F;
    };
    auto GeometryEnemy=[&](FVector Local,EPlayerAttackSource Restriction=EPlayerAttackSource::Other)
    {
        auto* E=Enemy(BoxCenter+BoxRotation.RotateVector(Local),Restriction);
        E->GetCapsuleComponent()->SetCapsuleSize(5,10);return E;
    };
    auto* Inside=GeometryEnemy(FVector(0,BoxExtent.Y-25,0));
    auto* Corner=GeometryEnemy(FVector(BoxExtent.X-2,BoxExtent.Y-2,0));
    auto* OutsideThickness=GeometryEnemy(FVector(BoxExtent.X+20,0,0));
    auto* OutsideWidth=GeometryEnemy(FVector(0,BoxExtent.Y+20,0));
    auto* Above=GeometryEnemy(FVector(0,0,BoxExtent.Z+30));
    auto* Below=GeometryEnemy(FVector(0,0,-BoxExtent.Z-30));
    auto* SourceRestricted=GeometryEnemy(FVector::ZeroVector,EPlayerAttackSource::Ninja);
    auto* ExtraCollision=NewObject<UBoxComponent>(Inside);ExtraCollision->SetupAttachment(Inside->GetRootComponent());
    ExtraCollision->SetBoxExtent(FVector(4));ExtraCollision->SetCollisionObjectType(ECC_Pawn);
    ExtraCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);ExtraCollision->SetCollisionResponseToAllChannels(ECR_Overlap);ExtraCollision->RegisterComponent();
    if(auto* F=MakeBoxField())
    {
        F->DealDamage(10);
        TestEqual(TEXT("Inside enemy is hit once despite multiple collision components"),Inside->GetHealthComponent()->GetCurrentHealth(),9990.f);
        TestEqual(TEXT("Rectangle corners receive damage"),Corner->GetHealthComponent()->GetCurrentHealth(),9990.f);
        for(auto* E:{OutsideThickness,OutsideWidth,Above,Below,SourceRestricted})
            TestEqual(TEXT("Box pulse excludes outside or source-restricted targets"),E->GetHealthComponent()->GetCurrentHealth(),10000.f);
        F->Destroy();
    }
    Inside->GetHealthComponent()->RestoreCurrentHealth(10000);Corner->GetHealthComponent()->RestoreCurrentHealth(10000);
    Acquire(TEXT("CrescentEruptionPact"));
    if(MakeBoxField())
    {
        Advance(.29f);TestEqual(TEXT("Box eruption retains delay"),Inside->GetHealthComponent()->GetCurrentHealth(),10000.f);
        Advance(.02f);
        TestTrue(TEXT("Box eruption hits inside enemy after source wave is gone"),FMath::IsNearlyEqual(Inside->GetHealthComponent()->GetCurrentHealth(),9910.f,.01f));
        TestTrue(TEXT("Box eruption also covers its corners"),FMath::IsNearlyEqual(Corner->GetHealthComponent()->GetCurrentHealth(),9910.f,.01f));
        for(auto* E:{OutsideThickness,OutsideWidth,Above,Below,SourceRestricted})
            TestEqual(TEXT("Eruption uses the same exact box and source restrictions"),E->GetHealthComponent()->GetCurrentHealth(),10000.f);
    }
    Clear();U->RestoreRunState(Base);
    for(int32 Rank=0;Rank<5;++Rank)Acquire(TEXT("CrescentSlowDuration"));
    auto* LongSlow=Enemy(FVector(40000,0,0));
    U->HandleSamuraiDirectHit(LongSlow,1,10000);
    Advance(5.1f);TestTrue(TEXT("Slow Duration survives the old five-second expiry"),LongSlow->GetCrescentMovementMultiplier()<1.f);
    Advance(5.f);TestEqual(TEXT("Five duration ranks expire after ten seconds"),LongSlow->GetCrescentMovementMultiplier(),1.f);

    U->RestoreRunState(Base);Acquire(TEXT("CrescentArc"));Acquire(TEXT("CrescentField"));
    for(int32 Rank=0;Rank<5;++Rank){Acquire(TEXT("CrescentArcChance"));Acquire(TEXT("CrescentFieldChance"));}
    TestTrue(TEXT("Arc Volley Chance caps at 65 percent"),FMath::IsNearlyEqual(CrescentBuild::Chance(U,TEXT("CrescentArc"),TEXT("CrescentArcChance"),.15f,.1f),.65f));
    TestTrue(TEXT("Wake Chance conservatively caps at 40 percent"),FMath::IsNearlyEqual(CrescentBuild::Chance(U,TEXT("CrescentField"),TEXT("CrescentFieldChance"),.15f,.05f),.4f));
    auto* ArcScaling=Card(TEXT("CrescentArcChance"));auto* WakeScaling=Card(TEXT("CrescentFieldChance"));
    const auto SavedArc=Arc->BalanceParameters,SavedField=FieldCard->BalanceParameters;
    const auto SavedArcScaling=ArcScaling->BalanceParameters,SavedWakeScaling=WakeScaling->BalanceParameters;
    Arc->BalanceParameters.Add(TEXT("Chance"),0.f);FieldCard->BalanceParameters.Add(TEXT("Chance"),0.f);
    ArcScaling->BalanceParameters.Add(TEXT("PerRank"),1.f);WakeScaling->BalanceParameters.Add(TEXT("PerRank"),1.f);
    Samurai->SetActorLocation(FVector(50000,0,0));Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
    Attack->ActiveAttackDirection=FVector::ForwardVector;
    Attack->SpawnBladeWavesForAttack(100);
    TestEqual(TEXT("Arc scaling alone drives real fan spawning"),Waves().Num(),3);
    for(auto* V:Waves())TestTrue(TEXT("Wake scaling alone arms real fields"),V->bFieldPending);
    Clear();Arc->BalanceParameters=SavedArc;FieldCard->BalanceParameters=SavedField;
    ArcScaling->BalanceParameters=SavedArcScaling;WakeScaling->BalanceParameters=SavedWakeScaling;

    U->RestoreRunState(Base);Acquire(TEXT("ReturningBlade"));
    Attack->SpawnBladeWavesForAttack(100);
    auto* ReturnWave=Waves()[0];const float OutDamage=ReturnWave->Damage;
    ReturnWave->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);ReturnWave->Movement->Deactivate();
    TestTrue(TEXT("Returning Blade enables the primary wave's return"),ReturnWave->bReturns);
    const FVector Launch=ReturnWave->GetActorLocation();
    auto* ReturnVictim=Enemy(FVector(80000,0,0));
    Hit(ReturnWave,ReturnVictim);Hit(ReturnWave,ReturnVictim);
    Samurai->SetActorLocation(FVector(50000,2000,0));
    ReturnWave->SetActorLocation(Launch+FVector(ReturnWave->TravelDistance,0,0));
    ReturnWave->BeginReturn();
    TestTrue(TEXT("Return retraces toward launch point despite player movement"),ReturnWave->Movement->Velocity.GetSafeNormal2D().Equals(-FVector::ForwardVector,.001f));
    TestTrue(TEXT("Return damage is half the outbound damage"),FMath::IsNearlyEqual(ReturnWave->Damage,OutDamage*.5f));
    Hit(ReturnWave,ReturnVictim);Hit(ReturnWave,ReturnVictim);
    TestTrue(TEXT("One outbound and one return hit total 150 percent damage"),FMath::IsNearlyEqual(10000-ReturnVictim->GetHealthComponent()->GetCurrentHealth(),OutDamage*1.5f,.02f));
    ReturnWave->BeginReturn();TestTrue(TEXT("Return cannot halve damage repeatedly"),FMath::IsNearlyEqual(ReturnWave->Damage,OutDamage*.5f));
    Clear();

    U->RestoreRunState(Base);Acquire(TEXT("ReturningBlade"));Acquire(TEXT("CrescentSplit"));Acquire(TEXT("CrescentField"));
    const auto ReturnSplitBalance=Split->BalanceParameters,ReturnFieldBalance=FieldCard->BalanceParameters;
    Split->BalanceParameters.Add(TEXT("Chance"),1.f);FieldCard->BalanceParameters.Add(TEXT("Chance"),1.f);
    Samurai->SetActorLocation(FVector(55000,0,0));Attack->SpawnBladeWavesForAttack(100);
    auto* ReturningSplit=Waves()[0];
    ReturningSplit->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);ReturningSplit->Movement->Deactivate();
    Hit(ReturningSplit,ReturnVictim);
    TestEqual(TEXT("Outbound split creates only two children"),Waves().Num(),3);
    TestEqual(TEXT("Outbound parent waits for its completed path"),Fields().Num(),0);
    ReturningSplit->SetActorLocation(ReturningSplit->LaunchOrigin+FVector(ReturningSplit->TravelDistance,0,0));
    ReturningSplit->BeginReturn();Hit(ReturningSplit,ReturnVictim);
    TestEqual(TEXT("Returning hit cannot split a second time"),Waves().Num(),3);
    TestEqual(TEXT("Returning hit cannot create a second parent field"),Fields().Num(),1);
    for(auto* Child:Waves())if(Child!=ReturningSplit)
    {
        TestFalse(TEXT("Split children do not inherit Returning Blade"),Child->bReturns);
        TestFalse(TEXT("Return children cannot recursively split"),Child->bCanSplit);
        Child->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);Child->Movement->Deactivate();
        auto* ChildVictim=Enemy(FVector(82000,0,0));Hit(Child,ChildVictim);
        Child->FinishWave();
    }
    TestEqual(TEXT("Both split children retain their own one field opportunity"),Fields().Num(),3);
    Clear();Attack->SpawnBladeWavesForAttack(100);
    auto* MissedReturn=Waves()[0];MissedReturn->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);MissedReturn->Movement->Deactivate();
    MissedReturn->SetActorLocation(MissedReturn->LaunchOrigin+FVector(MissedReturn->TravelDistance,0,0));
    const FVector FarEndpoint=MissedReturn->GetActorLocation();MissedReturn->BeginReturn();
    TestEqual(TEXT("A returning wave that misses commits one outbound strip"),Fields().Num(),1);
    if(Fields().Num()==1)TestTrue(TEXT("Miss strip spans launch to far endpoint"),Fields()[0]->GetActorLocation().Equals((FarEndpoint+MissedReturn->FieldTrailStart)*.5f,.01f));
    FieldCard->BalanceParameters.Add(TEXT("Chance"),0.f);
    Hit(MissedReturn,ReturnVictim);Hit(MissedReturn,ReturnVictim);
    TestEqual(TEXT("First hit on return can use the single split opportunity"),Waves().Num(),3);
    TestEqual(TEXT("Return hit does not reroll the endpoint field"),Fields().Num(),1);
    for(auto* Child:Waves())if(Child!=MissedReturn)
    {
        TestTrue(TEXT("Return split inherits the consumed parent's successful proc"),Child->bFieldPending);
        Child->Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);Child->Movement->Deactivate();
        const auto BeforeFields=Fields();
        Child->SetActorLocation(Child->FieldTrailStart+Child->GetActorForwardVector()*200.f);
        Child->FinishWave();
        for(auto* F:Fields())if(!BeforeFields.Contains(F))CheckFieldBox(F,Child);
    }
    TestEqual(TEXT("Both return splits leave their own complete inherited strips"),Fields().Num(),3);
    Split->BalanceParameters=ReturnSplitBalance;FieldCard->BalanceParameters=ReturnFieldBalance;Clear();
    // A stopped/shortened diagonal wave must damage the entire actual path,
    // including both end caps, without using its longer configured range.
    const auto TrailBalance=FieldCard->BalanceParameters;
    FieldCard->BalanceParameters.Add(TEXT("Chance"),1.f);
    for(bool bEruption:{false,true})
    {
        U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));
        if(bEruption)Acquire(TEXT("CrescentEruptionPact"));
        const FVector Start(bEruption?95000:90000,0,200);
        const FRotator Heading(0,35,0);
        auto* V=Wave(Start);V->SetActorRotation(Heading);V->SetActorScale3D(FVector(1.2,1.4,1));
        const FVector Extent=V->Collision->GetScaledBoxExtent();
        auto TrailEnemy=[&](FVector Local,EPlayerAttackSource Restriction=EPlayerAttackSource::Other)
        {
            auto* E=Enemy(Start+Heading.RotateVector(Local),Restriction);
            E->GetCapsuleComponent()->SetCapsuleSize(5,10);return E;
        };
        auto* AtStart=TrailEnemy(FVector::ZeroVector);
        auto* AtMiddle=TrailEnemy(FVector(160,0,0));
        auto* AtEnd=TrailEnemy(FVector(320,0,0));
        auto* StartCap=TrailEnemy(FVector(-Extent.X+2,0,0));
        auto* EndCap=TrailEnemy(FVector(320+Extent.X-2,0,0));
        auto* SideEdge=TrailEnemy(FVector(160,Extent.Y-2,0));
        auto* BeforeStart=TrailEnemy(FVector(-Extent.X-20,0,0));
        auto* BeyondEnd=TrailEnemy(FVector(320+Extent.X+20,0,0));
        auto* Untravelled=TrailEnemy(FVector(500,0,0));
        auto* OutsideSide=TrailEnemy(FVector(160,Extent.Y+20,0));
        auto* AboveTrail=TrailEnemy(FVector(160,0,Extent.Z+30));
        auto* RestrictedTrail=TrailEnemy(FVector(160,0,0),EPlayerAttackSource::Ninja);
        const TArray<AEnemyBase*> Covered={AtStart,AtMiddle,AtEnd,StartCap,EndCap,SideEdge};
        const TArray<AEnemyBase*> Excluded={BeforeStart,BeyondEnd,Untravelled,OutsideSide,AboveTrail,RestrictedTrail};
        V->SetActorLocation(Start+Heading.Vector()*120.f);Hit(V,AtMiddle);
        TestEqual(TEXT("Early hit does not commit or shorten the trail"),Fields().Num(),0);
        AtMiddle->GetHealthComponent()->RestoreCurrentHealth(10000);
        V->SetActorLocation(Start+Heading.Vector()*320.f);V->FinishWave();
        if(TestEqual(TEXT("Completed wave creates one continuous strip"),Fields().Num(),1))
        {
            auto* Trail=Fields()[0];CheckFieldBox(Trail,V);
            TestTrue(TEXT("Strip includes actual travel plus wave end thickness"),FMath::IsNearlyEqual(Trail->HitboxHalfExtent.X,160.f+Extent.X,.01f));
            TestTrue(TEXT("Strip retains scaled wave width"),FMath::IsNearlyEqual(Trail->HitboxHalfExtent.Y,Extent.Y,.01f));
            if(bEruption)
            {
                Advance(.29f);
                for(auto* E:Covered)TestEqual(TEXT("Full-strip eruption waits after travel ends"),E->GetHealthComponent()->GetCurrentHealth(),10000.f);
                Advance(.02f);
            }
            else Advance(3.1f);
            for(auto* E:Covered)TestTrue(TEXT("Start, middle, end and edges receive exactly one field budget"),FMath::IsNearlyEqual(E->GetHealthComponent()->GetCurrentHealth(),9910.f,.03f));
            for(auto* E:Excluded)TestEqual(TEXT("Outside, untravelled and restricted targets are not damaged"),E->GetHealthComponent()->GetCurrentHealth(),10000.f);
        }
        Clear();for(auto* E:Covered)E->Destroy();for(auto* E:Excluded)E->Destroy();
    }
    U->RestoreRunState(Base);Acquire(TEXT("CrescentField"));FieldCard->BalanceParameters.Add(TEXT("Chance"),0.f);
    auto* NoProc=Wave(FVector(100000,0,200));NoProc->SetActorLocation(NoProc->FieldTrailStart+FVector(400,0,0));NoProc->FinishWave();
    TestEqual(TEXT("Failed field proc leaves no path strip"),Fields().Num(),0);
    FieldCard->BalanceParameters=TrailBalance;
    Cleanup();return true;
}
#endif
