#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CinderOracleBoss.h"
#include "EncounterHazard.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "CombatGround.h"
#include "MinimapBounds.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossChargeTest,"HeavensDivide.Encounters.BossCharge",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBossChargeTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    auto MakeBox=[&](FVector Center,FVector Extent)
    {
        auto* Actor=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Actor);Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Actor->SetActorLocation(Center);return Actor;
    };
    MakeBox(FVector(0,0,-10),FVector(10000,10000,10));
    auto* Bounds=W->SpawnActor<AMinimapBounds>();Bounds->PlayableArea->SetBoxExtent(FVector(4000,3000,100));Bounds->PlayableArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* PC=W->SpawnActor<ASurvivorPlayerController>();auto* Player=W->SpawnActor<ACharacterBase>();PC->Possess(Player);Player->SetOwner(PC);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Player);
    auto* HP=PC->GetPlayerHealthComponent();HP->RestoreCurrentHealth(100);HP->DamageGracePeriod=0;
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Class=LoadClass<ACinderOracleBoss>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle.BP_CinderOracle_C"));
    auto* Boss=W->SpawnActor<ACinderOracleBoss>(Class,FVector(0,0,500),FRotator::ZeroRotator,Params);
    Boss->SetActorLocation(FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2));
    Boss->SetTarget(Player);Boss->CachedSurvivorController=PC;Boss->ObservedCharacterManager=PC->GetCharacterManager();Boss->bOracleActive=true;
    Player->SetActorLocation(FVector(1000,0,96));Boss->GetHealthComponent()->RestoreCurrentHealth(Boss->GetHealthComponent()->GetMaxHealth());
    Boss->CastAbility(7);
    TestEqual(TEXT("Phase one queues two charges"),Boss->ChargesRemaining,2);
    TestTrue(TEXT("Visible locked path precedes the rush"),Boss->ChargeWarning.IsValid()&&Boss->ChargeWarning->HasVisibleWarning());
    Boss->TickCharge(Boss->PhaseOneWarning+.01f);
    TestTrue(TEXT("Rush begins after the warning"),Boss->bCharging);
    const FVector Start=Boss->GetActorLocation();
    TestEqual(TEXT("Saved boss uses doubled charge speed"),Boss->ChargeSpeed,3600.f);
    Boss->TickCharge(.1f);
    TestTrue(TEXT("Charge travels 360 cm in one tenth of a second"),FMath::IsNearlyEqual(float(FVector::Dist2D(Start,Boss->GetActorLocation())),360.f,1.f));
    Boss->SetActorLocation(Start);
    // The contact segment catches a player crossed between frames and only hits once per rush.
    Boss->HitChargePlayer(Start,Start+FVector(1500,0,0));
    TestEqual(TEXT("Charge deals configured contact damage"),HP->GetCurrentHealth(),100.f-Boss->ChargeContactDamage);
    TestTrue(TEXT("Contact queues a physical push"),Player->GetCharacterMovement()->PendingLaunchVelocity.Size2D()>0);
    Boss->HitChargePlayer(Start,Start+FVector(1500,0,0));
    TestEqual(TEXT("Repeated overlap cannot damage again in the same charge"),HP->GetCurrentHealth(),100.f-Boss->ChargeContactDamage);
    Boss->EndCharge();TestEqual(TEXT("One charge remains"),Boss->ChargesRemaining,1);
    Boss->TickCharge(.99f);TestFalse(TEXT("Second charge cannot start before one second"),Boss->bCharging);
    Boss->TickCharge(.02f);TestTrue(TEXT("Second charge begins at one second"),Boss->bCharging);
    Boss->EndCharge();TestEqual(TEXT("Phase one ends after two charges"),Boss->ChargesRemaining,0);
    Boss->GetHealthComponent()->RestoreCurrentHealth(Boss->GetHealthComponent()->GetMaxHealth()*.4f);
    Boss->CastAbility(7);TestEqual(TEXT("Phase two queues four charges"),Boss->ChargesRemaining,4);
    Boss->TickCharge(Boss->PhaseTwoWarning+.01f);Boss->TickCharge(.15f);
    TArray<AEncounterHazard*> Trails;for(TActorIterator<AEncounterHazard> It(W);It;++It)if(!It->IsActorBeingDestroyed()&&It->Damage>0)Trails.Add(*It);
    TestTrue(TEXT("Phase two movement paints damaging trail"),Trails.Num()>0);
    for(auto* Trail:Trails)
    {
        TestEqual(TEXT("Trail is immediately active"),Trail->Warning,0.f);TestEqual(TEXT("Trail lasts three seconds"),Trail->ActiveTime,3.f);
        Player->SetActorLocation(Trail->GetActorLocation()+FVector(0,0,96));HP->RestoreCurrentHealth(100);
        Trail->Tick(.1f);TestEqual(TEXT("Standing in trail takes damage"),HP->GetCurrentHealth(),100.f-Boss->ChargeTrailDamage);
        Trail->Tick(2.91f);TestTrue(TEXT("Expired trail is removed"),Trail->IsActorBeingDestroyed());
    }
    Boss->EndCharge();
    Boss->TickCharge(.99f);TestFalse(TEXT("Phase two also waits a full second"),Boss->bCharging);
    Boss->TickCharge(.02f);TestTrue(TEXT("Phase two resumes after one second"),Boss->bCharging);
    Boss->EndCharge();Boss->TickCharge(1.01f);Boss->EndCharge();
    TestEqual(TEXT("Fourth phase two charge remains after three"),Boss->ChargesRemaining,1);Boss->TickCharge(1.01f);Boss->EndCharge();
    TestEqual(TEXT("Phase two ends after four"),Boss->ChargesRemaining,0);
    Boss->SetActorLocation(Start);Player->SetActorLocation(FVector(2000,0,96));
    const float Radius=Boss->GetCapsuleComponent()->GetScaledCapsuleRadius();
    MakeBox(FVector(700,0,500),FVector(30,1000,500));Boss->CastAbility(7);Boss->TickCharge(2);
    for(int32 I=0;I<20&&Boss->bCharging;++I)Boss->TickCharge(.1f);
    TestTrue(TEXT("Swept charge stops at solid scenery"),Boss->GetActorLocation().X<=700-30-Radius+5);
    Boss->CancelCharge();Boss->SetActorLocation(FVector(3500,1000,Start.Z));Player->SetActorLocation(FVector(3900,2000,96));
    Boss->CastAbility(7);
    TestTrue(TEXT("Diagonal warning endpoint respects the full capsule at bounds"),CombatGround::IsInsideArena(W,Boss->GetActorLocation()+Boss->ChargeDirection*Boss->ChargeRemainingDistance,Radius+4));
    Boss->TickCharge(2);for(int32 I=0;I<20&&Boss->bCharging;++I)Boss->TickCharge(.1f);
    TestTrue(TEXT("Diagonal charge stays in arena"),CombatGround::IsInsideArena(W,Boss->GetActorLocation(),Radius));
    Boss->StopBossCombat();TestEqual(TEXT("Stopping combat cancels all remaining charges"),Boss->ChargesRemaining,0);
    TestFalse(TEXT("Stopping combat removes the warning"),Boss->ChargeWarning.IsValid());
    GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
