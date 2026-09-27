#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BossGroundTelegraph.h"
#include "NinjaFloorTrap.h"
#include "NinjaTechniqueTrial.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackIndicatorTest,"HeavensDivide.Combat.AttackIndicators",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAttackIndicatorTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 World->InitializeActorsForPlay(FURL());
 auto* Circle=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorCircle_Decal.M_AttackIndicatorCircle_Decal"));
 auto* Rectangle=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorRectangle.M_AttackIndicatorRectangle"));
 if(!TestNotNull(TEXT("Circle material saved"),Circle)||!TestNotNull(TEXT("Rectangle material saved"),Rectangle)) {World->DestroyWorld(false);return false;}
 auto* Warning=World->SpawnActor<ABossGroundTelegraph>();
 auto* GreenCircle=NewObject<UMaterialInstanceConstant>(World);
 auto* BlueRectangle=NewObject<UMaterialInstanceConstant>(World);
#if WITH_EDITOR
 GreenCircle->SetParentEditorOnly(Circle);
 GreenCircle->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("FillColor")),FLinearColor::Green);
 BlueRectangle->SetParentEditorOnly(Rectangle);
 BlueRectangle->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("FillColor")),FLinearColor::Blue);
#endif
 Warning->InitializeTelegraph(nullptr,375,2,10,GreenCircle);
 auto* Decal=Warning->FindComponentByClass<UDecalComponent>();
 auto* MID=Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial());
 TestTrue(TEXT("Timed circle inherits authored fill color"),MID->K2_GetVectorParameterValue(TEXT("FillColor")).Equals(FLinearColor::Green));
 TestTrue(TEXT("Circle footprint preserves damage radius"),Decal->DecalSize.Equals(FVector(64,375,375)));
 TestEqual(TEXT("Circle starts empty"),MID->K2_GetScalarParameterValue(TEXT("FillAmount")),0.f);
 Warning->Tick(1);
 TestEqual(TEXT("Circle fills with attack windup"),MID->K2_GetScalarParameterValue(TEXT("FillAmount")),.5f);
 Warning->Tick(1);
 TestTrue(TEXT("Timed warning ends at impact"),Warning->IsActorBeingDestroyed());
 auto* Persistent=World->SpawnActor<ABossGroundTelegraph>();
 Persistent->InitializePersistentRectangle(800,200,BlueRectangle);
 auto* Mesh=Persistent->FindComponentByClass<UStaticMeshComponent>();
 TestEqual(TEXT("Rectangle uses a flat plane"),Mesh->GetStaticMesh()->GetName(),FString(TEXT("Plane")));
 TestTrue(TEXT("Rectangle preserves footprint"),Mesh->GetComponentScale().Equals(FVector(8,2,1)));
 auto* RectMID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
 TestTrue(TEXT("Boss and Ogre rectangle inherit authored fill color"),RectMID->K2_GetVectorParameterValue(TEXT("FillColor")).Equals(FLinearColor::Blue));
 TestEqual(TEXT("Rectangle aspect follows attack shape"),RectMID->K2_GetScalarParameterValue(TEXT("LaneAspect")),.25f);
 Persistent->SetTelegraphFillAmount(.75f);
 TestEqual(TEXT("Boss windup drives fill"),RectMID->K2_GetScalarParameterValue(TEXT("FillAmount")),.75f);
 Persistent->InitializePersistentCircle(200,GreenCircle);
 TestTrue(TEXT("Persistent circle inherits authored fill color"),Cast<UMaterialInstanceDynamic>(Persistent->FindComponentByClass<UDecalComponent>()->GetDecalMaterial())->K2_GetVectorParameterValue(TEXT("FillColor")).Equals(FLinearColor::Green));
 TestTrue(TEXT("Switching shape removes rectangular scale"),Mesh->GetComponentScale().Equals(FVector::OneVector));
 auto* Trap=World->SpawnActor<ANinjaFloorTrap>();
 Trap->InitializeForTrial(World->SpawnActor<ANinjaTechniqueTrial>());
 Trap->DispatchBeginPlay();
 auto* Hazard=Trap->FindComponentByClass<UBoxComponent>();
 Hazard->SetBoxExtent(FVector(400,125,100));
 Trap->ActivateTrap();Trap->Tick(1);
 auto* TrapMesh=Trap->FindComponentByClass<UStaticMeshComponent>();
 TestTrue(TEXT("Floor warning fits its actual hazard"),TrapMesh->GetComponentScale().Equals(FVector(8,2.5,1)));
 Trap->Tick(.75f);
 auto* TrapMID=Cast<UMaterialInstanceDynamic>(TrapMesh->GetMaterial(0));
 TestEqual(TEXT("Floor warning fills over windup"),TrapMID->K2_GetScalarParameterValue(TEXT("FillAmount")),.5f);
 Trap->DeactivateTrap();
 TestFalse(TEXT("Cancelled floor warning is hidden"),TrapMesh->IsVisible());
 World->DestroyWorld(false);
 return true;
}
#endif
