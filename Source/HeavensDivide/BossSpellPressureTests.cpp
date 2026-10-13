#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BossSpellGeometry.h"
#include "CinderOracleBoss.h"
#include "EncounterHazard.h"
#include "TacticalEnemy.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "MinimapBounds.h"
#include "CombatGround.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSpellPressureTest,"HeavensDivide.Encounters.BossSpellPressure",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBossSpellPressureTest::RunTest(const FString&)
{
    FMinimalViewInfo View; View.Location=FVector(-900,0,1400); View.Rotation=(-View.Location).Rotation(); View.FOV=70;
    TArray<FVector> Corners,Grid,Narrow;
    TestTrue(TEXT("Perspective camera projects all four screen edges onto floor"),BossSpellGeometry::GroundViewCorners(View,16.f/9,0,Corners));
    BossSpellGeometry::ScreenCircleCenters(Corners,540,64,Grid);
    TestTrue(TEXT("Screen pattern is dense and bounded"),Grid.Num()>3 && Grid.Num()<=64);
    for (auto Corner:Corners) TestTrue(TEXT("Grid reaches every screen corner"),Grid.ContainsByPredicate([&](FVector P){return P.Equals(Corner,.1);}));
    BossSpellGeometry::GroundViewCorners(View,1,0,Corners);
    BossSpellGeometry::ScreenCircleCenters(Corners,540,64,Narrow);
    TestTrue(TEXT("Different aspect ratios produce different floor patterns"),Grid.Num()!=Narrow.Num());
    View.ProjectionMode=ECameraProjectionMode::Orthographic; View.OrthoWidth=2400;
    TestTrue(TEXT("Orthographic camera also supported"),BossSpellGeometry::GroundViewCorners(View,16.f/9,0,Corners));
    View.Rotation=FRotator::ZeroRotator;
    TestFalse(TEXT("A view parallel to the ground cannot generate infinite circles"),BossSpellGeometry::GroundViewCorners(View,16.f/9,0,Corners));

    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->InitializeActorsForPlay(FURL());
    auto* Bounds=W->SpawnActor<AMinimapBounds>(); Bounds->PlayableArea->SetBoxExtent(FVector(4000,3000,100));
    Bounds->PlayableArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Floor=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10000,10000,10)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
    auto* PC=W->SpawnActor<ASurvivorPlayerController>(); auto* Player=W->SpawnActor<ACharacterBase>(); PC->Possess(Player); Player->SetOwner(PC);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Player);
    auto* HP=PC->GetPlayerHealthComponent(); HP->RestoreCurrentHealth(100); HP->DamageGracePeriod=0;
    Player->SetActorLocation(FVector(500,0,96));
    auto* Camera=W->SpawnActor<ACameraActor>(FVector(-400,0,1400),FRotator(-57.26f,0,0));
    Camera->GetCameraComponent()->FieldOfView=70; Camera->GetCameraComponent()->bConstrainAspectRatio=false; PC->SetViewTarget(Camera);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Class=LoadClass<ACinderOracleBoss>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle.BP_CinderOracle_C"));
    if (!TestNotNull(TEXT("Saved boss loads"),Class)) { GEngine->DestroyWorldContext(W); W->DestroyWorld(false); return false; }
    auto* Boss=W->SpawnActor<ACinderOracleBoss>(Class,FVector(0,0,400),FRotator::ZeroRotator,Params);
    Boss->SetTarget(Player); Boss->CachedSurvivorController=PC; Boss->ObservedCharacterManager=PC->GetCharacterManager(); Boss->bOracleActive=true;
    auto Hazards=[&]() { TArray<AEncounterHazard*> Out; for(TActorIterator<AEncounterHazard> It(W);It;++It) if(!It->IsActorBeingDestroyed()) Out.Add(*It); return Out; };
    auto Clear=[&]() { for(auto* H:Hazards()) H->Destroy(); };
    TestEqual(TEXT("Saved Falling Embers count"),Boss->EmberCount,10);
    Boss->CastAbility(0); TestEqual(TEXT("Falling Embers produces ten warnings"),Hazards().Num(),10); Clear();
    Boss->CastAbility(1); TestEqual(TEXT("Lances retain three strikes"),Hazards().Num(),3);
    for(auto* H:Hazards()) { TestEqual(TEXT("Longer lance"),H->Size,2600.f); TestEqual(TEXT("Wider lance"),H->Width,340.f); }
    TestEqual(TEXT("Fan spans 110 degrees"),Boss->LanceSideAngle*2,110.f); Clear();
    Boss->CastAbility(2); for(auto* H:Hazards()) TestEqual(TEXT("Nova detonates after 0.85 seconds"),H->Warning,.85f); Clear();
    Boss->CastAbility(3); const int32 GroundHazards=Hazards().Num();
    TestTrue(TEXT("Camera-filling ground produces many circles"),GroundHazards>3 && GroundHazards<=64);
    for(auto* H:Hazards()) { TestTrue(TEXT("Whole field starts within a short sweep"),H->Delay<=.351f); TestEqual(TEXT("Persistent fields retain three seconds"),H->ActiveTime,3.f); }
    Boss->CastAbility(5);
    TestEqual(TEXT("Screen fields do not truncate the overlapping twenty-blast spiral"),Hazards().Num(),GroundHazards+20);
    Clear(); Boss->CastAbility(5); TestEqual(TEXT("Spiral produces twenty blasts"),Hazards().Num(),20); Clear();
    Player->SetActorLocation(FVector(3500,2000,96)); Boss->CastAbility(6);
    TestEqual(TEXT("Eclipse ring and two crossing lanes survive arena limits"),Hazards().Num(),3);
    for(auto* H:Hazards()) if(H->Footprint==EEncounterShape::Ring)
    {
        TestTrue(TEXT("Eclipse stays centered on the captured player"),FVector::Dist2D(H->GetActorLocation(),Player->GetActorLocation())<1);
        TestFalse(TEXT("Eclipse hole remains safe from the ring"),H->ContainsPoint(FVector(3500,2000,4)));
        for(int32 X:{-1,1}) for(int32 Y:{-1,1}) TestTrue(TEXT("No arena corner can escape Eclipse's exterior"),H->ContainsPoint(FVector(X*4000,Y*3000,4)));
    }
    Clear();
    TestNull(TEXT("Ordinary hazards still reject oversized arena footprints"),AEncounterHazard::Spawn(Player,FVector::ZeroVector,EEncounterShape::Circle,10000,0,1,1));
    Player->SetActorLocation(FVector(500,0,96)); Boss->CastAbility(4);
    TestEqual(TEXT("Summon creates two guardian casters"),Boss->Guardians.Num(),2);
    TestEqual(TEXT("Summon adds twenty ordinary monsters"),Boss->Horde.Num(),20);
    Clear();
    // Exercise the actual spawned guardian's initial cooldown and behavior route.
    for(auto Guard:Boss->Guardians) if(Guard.IsValid())
    {
        Guard->DispatchBeginPlay(); Guard->CachedSurvivorController=PC; Guard->ObservedCharacterManager=PC->GetCharacterManager();
        TestTrue(TEXT("Guardian initial cooldown is authored"),Guard->Cooldown>=1.2f && Guard->Cooldown<=2.8f);
        TestTrue(TEXT("Summon feet are grounded"),FMath::Abs(Guard->GetActorLocation().Z-Guard->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())<1);
        Guard->Tick(2.81f); Guard->UpdateEnemyBehavior(.1f);
        TestTrue(TEXT("Boss-spawned guardian naturally begins casting"),Guard->CastRemaining>0);
        bool Found=false;
        for(auto* H:Hazards()) if(H->GetOwner()==Guard.Get())
        {
            Found=true; HP->RestoreCurrentHealth(100); H->Tick(H->Warning+.01f);
            TestEqual(TEXT("Guardian's targeted circle actually damages player"),HP->GetCurrentHealth(),88.f);
        }
        TestTrue(TEXT("Guardian creates its own targeted warning"),Found); Clear();
    }
    Boss->CastAbility(4); Boss->CastAbility(4); Boss->CastAbility(4);
    TestEqual(TEXT("Repeated summons cap living guardians"),Boss->Guardians.Num(),4);
    TestEqual(TEXT("Repeated summons cap horde at sixty"),Boss->Horde.Num(),60);
    Boss->StopBossCombat(); TestEqual(TEXT("Shutdown removes horde tracking"),Boss->Horde.Num(),0);
    TestEqual(TEXT("Shutdown removes guardians"),Boss->Guardians.Num(),0); TestEqual(TEXT("Shutdown removes warnings"),Hazards().Num(),0);
    for(TActorIterator<AEnemyBase> It(W);It;++It) if(It->GetOwner()==Boss) TestTrue(TEXT("All summoned monsters are destroyed"),It->IsActorBeingDestroyed());
    GEngine->DestroyWorldContext(W); W->DestroyWorld(false); return true;
}
#endif
