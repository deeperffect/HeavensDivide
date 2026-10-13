#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CinderOracleBoss.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "HealthComponent.h"
#include "EnemyLightweightMovementComponent.h"
#include "SurvivorPlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScorpionBossTest,"HeavensDivide.Encounters.ScorpionBoss",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FScorpionBossTest::RunTest(const FString&)
{
    auto* Class=LoadClass<ACinderOracleBoss>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle.BP_CinderOracle_C"));
    if(!TestNotNull(TEXT("Scorpion boss Blueprint loads"),Class))return false;
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    auto* PC=W->SpawnActor<ASurvivorPlayerController>();PC->VictoryWidgetClass=nullptr;
    auto* Player=W->SpawnActor<ACharacterBase>();Player->SetOwner(PC);PC->Possess(Player);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Player);
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Boss=W->SpawnActor<ACinderOracleBoss>(Class,FVector(1200,0,400),FRotator::ZeroRotator,Params);
    auto* Mesh=Boss->GetMesh();Mesh->InitAnim(true);
    TestTrue(TEXT("New boss name is active"),Boss->GetBossDisplayName().ToString().Contains(TEXT("PALE STINGER")));
    TestTrue(TEXT("Scorpion mesh is active"),Mesh->GetSkeletalMeshAsset()&&Mesh->GetSkeletalMeshAsset()->GetName()==TEXT("SK_PaleStinger"));
    TestTrue(TEXT("Scaled model meets the capsule floor"),FMath::IsNearlyEqual(Mesh->GetRelativeLocation().Z,-Boss->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),.01));
    TestTrue(TEXT("Boss model is twice its initial size"),Mesh->GetRelativeScale3D().Equals(FVector(1.30),.001));
    TestTrue(TEXT("Boss collision scales with the model"),FMath::IsNearlyEqual(Boss->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),300.f)&&FMath::IsNearlyEqual(Boss->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),400.f));
    TestNull(TEXT("Permanent boss overlay is removed"),Mesh->GetOverlayMaterial());
    TestEqual(TEXT("Boss Blueprint cannot be pushed"),Boss->PushbackMultiplier,0.f);
    TestEqual(TEXT("Native boss cannot be pushed"),GetDefault<ACinderOracleBoss>()->PushbackMultiplier,0.f);
    for(const TCHAR* Bone:{TEXT("Foot_L1"),TEXT("Foot_L2"),TEXT("Foot_L3"),TEXT("Foot_R1"),TEXT("Foot_R2"),TEXT("Foot_R3"),TEXT("Claw_L_Outer"),TEXT("Claw_R_Outer"),TEXT("Tail_Tip")})
        TestTrue(FString::Printf(TEXT("Scorpion bone %s exists"),Bone),Mesh->GetBoneIndex(Bone)!=INDEX_NONE);
    auto* Anim=Mesh->GetAnimInstance();
    if(TestNotNull(TEXT("Creature animation Blueprint initializes"),Anim))
    {
        Mesh->TickAnimation(.1f,false);Mesh->RefreshBoneTransforms();
        const FVector Forward=(Mesh->GetSocketLocation(TEXT("Head"))-Mesh->GetSocketLocation(TEXT("Thorax"))).GetSafeNormal2D();
        TestTrue(TEXT("Scorpion faces the actor's forward direction"),FVector::DotProduct(Forward,Boss->GetActorForwardVector())>.95f);
        const FVector IdleFoot=Mesh->GetSocketLocation(TEXT("Foot_L1"))-Boss->GetActorLocation();
        auto* Movement=Boss->FindComponentByClass<UEnemyLightweightMovementComponent>();
        Movement->RefreshSpawnZ();Movement->SetMoveSpeed(150);Movement->SetMovementEnabled(true);
        Movement->RequestMove(FVector::ForwardVector);Movement->TickComponent(.1f,LEVELTICK_All,nullptr);
        TestTrue(TEXT("Real lightweight chase reports its velocity"),Boss->GetVelocity().Size()>140);
        Mesh->TickAnimation(.35f,false);Mesh->RefreshBoneTransforms();
        TestTrue(TEXT("Chasing produces a walking pose"),(Mesh->GetSocketLocation(TEXT("Foot_L1"))-Boss->GetActorLocation()-IdleFoot).Size()>10);
        Movement->StopMovement();
        const FVector BeforePush=Boss->GetActorLocation();
        for(const auto Source:{EPlayerAttackSource::Samurai,EPlayerAttackSource::Ninja,EPlayerAttackSource::Other})
        {
            Boss->ApplyAttackPushback(BeforePush-FVector(100,0,0),Source,5000,.1f);
            Movement->TickComponent(.2f,LEVELTICK_All,nullptr);
            TestTrue(TEXT("Attacks cannot displace the boss"),Boss->GetActorLocation().Equals(BeforePush,.001));
        }
        TestTrue(TEXT("Wobble montage plays through the animation graph"),Anim->Montage_Play(Boss->OracleCastMontage)>0);
        Mesh->TickAnimation(.5f,false);Mesh->RefreshBoneTransforms();
        TestTrue(TEXT("Cast montage remains active during windup"),Anim->Montage_IsPlaying(Boss->OracleCastMontage));
        Anim->Montage_Stop(0.f);Boss->GetCharacterMovement()->Velocity=FVector::ZeroVector;
        Mesh->TickAnimation(.2f,false);Mesh->RefreshBoneTransforms();
        const float BodyZ=Mesh->GetSocketLocation(TEXT("Thorax")).Z;
        const float Duration=Boss->GetDeathPresentationSeconds();
        TestTrue(TEXT("Boss has the authored death presentation"),Duration>2.7f&&Duration<3.1f);
        Boss->SetTarget(Player);
        Boss->GetHealthComponent()->SetMaxHealthPreservePercent(6000);
        FScriptDelegate D;D.BindUFunction(Boss,TEXT("HandleDeath"));Boss->GetHealthComponent()->OnDeath.AddUnique(D);
        Boss->GetHealthComponent()->ApplyDamage(100000);
        TestTrue(TEXT("Boss is dead immediately"),Boss->IsDead());
        TestFalse(TEXT("Victory records immediately"),PC->IsRunInProgress());
        TestTrue(TEXT("Death keeps the world animating"),UGameplayStatics::GetGlobalTimeDilation(W)>.99f);
        TestTrue(TEXT("Victory presentation waits for the death clip"),W->GetTimerManager().TimerExists(PC->VictoryPresentationTimerHandle));
        Mesh->TickAnimation(1.4f,false);Mesh->RefreshBoneTransforms();
        TestTrue(TEXT("Death animation lowers the scorpion body"),Mesh->GetSocketLocation(TEXT("Thorax")).Z<BodyZ-35);
        ++GFrameCounter;W->GetTimerManager().Tick(.001f);
        ++GFrameCounter;W->GetTimerManager().Tick(Duration-.15f);
        TestTrue(TEXT("Victory does not freeze early"),UGameplayStatics::GetGlobalTimeDilation(W)>.99f);
        ++GFrameCounter;W->GetTimerManager().Tick(.2f);
        TestTrue(TEXT("Victory freezes only after the collapse"),UGameplayStatics::GetGlobalTimeDilation(W)<.001f);
    }
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return !HasAnyErrors();
}
#endif
