#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TankMeleeEnemyBase.h"
#include "BossGroundTelegraph.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankIndicatorTest, "HeavensDivide.Combat.TankIndicators",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTankIndicatorTest::RunTest(const FString&)
{
 auto* World = UWorld::CreateWorld(EWorldType::Game, false);
 World->InitializeActorsForPlay(FURL());
 auto* OgreClass = LoadClass<ATankMeleeEnemyBase>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Elites/BP_EnemyOgre.BP_EnemyOgre_C"));
 auto* GorillaClass = LoadClass<ATankMeleeEnemyBase>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Elites/BP_EnemyGorilla.BP_EnemyGorilla_C"));
 if (!TestNotNull(TEXT("Ogre Blueprint"), OgreClass) || !TestNotNull(TEXT("Gorilla Blueprint"), GorillaClass)) { World->DestroyWorld(false); return false; }
 auto* Ogre = World->SpawnActor<ATankMeleeEnemyBase>(OgreClass);
 Ogre->SetActorScale3D(FVector(1.7f));
 Ogre->SetActorRotation(FRotator(0, 90, 0));
 Ogre->ShowAttackTelegraph();
 if (TestNotNull(TEXT("Ogre uses boss ground telegraph"), Ogre->RectangleGroundTelegraph.Get()))
 {
  auto* Mesh = Ogre->RectangleGroundTelegraph->FindComponentByClass<UStaticMeshComponent>();
  auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
  TestEqual(TEXT("Same surface material as boss"), MID->Parent->GetName(), FString(TEXT("M_AttackIndicatorRectangle")));
  TestTrue(TEXT("World footprint matches attack regardless of enemy scale"), Mesh->GetComponentScale().Equals(FVector(Ogre->AttackBoxLength/100, Ogre->AttackBoxWidth/100, 1)));
  TestFalse(TEXT("Old rectangle decal hidden"), Ogre->AttackTelegraphDecal->IsVisible());
  Ogre->SetTelegraphFillAmount(.5f);
  TestEqual(TEXT("Windup drives boss renderer"), MID->K2_GetScalarParameterValue(TEXT("FillAmount")), .5f);
  Ogre->SetActorLocation(FVector(400, 500, 100));
  Ogre->SetActorRotation(FRotator(0, 170, 0));
  Ogre->UpdateAttackTelegraphSizeAndPlacement();
  TestTrue(TEXT("Indicator tracks slam center"), Ogre->RectangleGroundTelegraph->GetActorLocation().Equals(FVector(Ogre->GetBoxSlamCenter().X, Ogre->GetBoxSlamCenter().Y, Ogre->RectangleGroundTelegraph->GetActorLocation().Z)));
  TestEqual(TEXT("Indicator tracks windup facing"), Ogre->RectangleGroundTelegraph->GetActorRotation().Yaw, 170.0);
  Ogre->HideAttackTelegraph();
  TestNull(TEXT("Cancelled rectangle cleaned up"), Ogre->RectangleGroundTelegraph.Get());
 }
 auto* Gorilla = World->SpawnActor<ATankMeleeEnemyBase>(GorillaClass);
 Gorilla->DispatchBeginPlay();
 Gorilla->SetActorScale3D(FVector(1.5f));
 Gorilla->UpdateContactAura();
 TestTrue(TEXT("Saved Gorilla uses contact aura"), Gorilla->UsesContactDamage());
 TestTrue(TEXT("Aura visible outside attacks"), Gorilla->ContactAuraDecal->IsVisible());
 TestEqual(TEXT("Aura matches scaled damage radius"), float(Gorilla->ContactAuraDecal->DecalSize.Y), Gorilla->ContactDamageSphere->GetScaledSphereRadius());
 if (TestNotNull(TEXT("Aura material initialized"), Gorilla->ContactAuraMID.Get()))
 {
  TestEqual(TEXT("Same circle decal as boss"), Gorilla->ContactAuraMID->Parent->GetName(), FString(TEXT("M_AttackIndicatorCircle_Decal")));
  TestEqual(TEXT("Aura is unfilled"), Gorilla->ContactAuraMID->K2_GetScalarParameterValue(TEXT("FillAmount")), 0.f);
  TestEqual(TEXT("Aura has transparent interior"), Gorilla->ContactAuraMID->K2_GetScalarParameterValue(TEXT("OutlineOnly")), 1.f);
  TestEqual(TEXT("Aura ripples continuously"), Gorilla->ContactAuraMID->K2_GetScalarParameterValue(TEXT("ContinuousRipple")), 1.f);
  TestFalse(TEXT("Gorilla body rejects ground decals"), bool(Gorilla->GetMesh()->bReceivesDecals));
  Gorilla->SetTelegraphFillAmount(.25f);
  Gorilla->HideAttackTelegraph();
  TestEqual(TEXT("Attack fill cannot animate aura"), Gorilla->ContactAuraMID->K2_GetScalarParameterValue(TEXT("FillAmount")), 0.f);
  TestTrue(TEXT("Ending attack preserves aura"), Gorilla->ContactAuraDecal->IsVisible());
 }
 Gorilla->HandleDeath();
 TestFalse(TEXT("Death hides aura"), Gorilla->ContactAuraDecal->IsVisible());
 World->DestroyWorld(false);
 return true;
}
#endif
