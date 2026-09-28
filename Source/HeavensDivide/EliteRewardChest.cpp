#include "EliteRewardChest.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AEliteRewardChest::AEliteRewardChest()
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.bTickEvenWhenPaused=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 ChestMesh=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Chest"));
 ChestMesh->SetupAttachment(RootComponent);
 ChestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Assets/Environment/Ancient_Ruins/Meshes/SK_Small_Treasure_Chest"));
 ChestMesh->SetSkinnedAssetAndUpdate(Mesh.Object);
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("RewardCamera"));
 Camera->SetupAttachment(RootComponent);
 const FVector CameraPosition(170,-210,165);
 Camera->SetRelativeLocation(CameraPosition);
 Camera->SetRelativeRotation((FVector(0,0,35)-CameraPosition).Rotation());
 Camera->FieldOfView=55;
 RewardGlow=CreateDefaultSubobject<UNiagaraComponent>(TEXT("RewardGlow"));
 RewardGlow->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Glow(TEXT("/Game/Assets/VFX/LootDropsV2/Particles/NiagaraSystems/LootDrop05/NS_LootDrop05_lvl5_yellow"));
 RewardGlow->SetAsset(Glow.Object);
 static ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/Assets/Sounds/Pickups/MS_XpPickup"));
 OpeningSound=Sound.Object;
 Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("RewardLight"));
 Light->SetupAttachment(RootComponent);
 Light->SetRelativeLocation(FVector(0,0,60));
 Light->SetLightColor(FLinearColor(1,.55f,.12f));
 Light->SetIntensity(700);
 Light->SetAttenuationRadius(220);
 Light->SetCastShadows(false);
}
void AEliteRewardChest::InitializeReward(int32 Value,TSubclassOf<AExperiencePickup> Pickup,UExperienceComponent* InExperience,UCharacterManagerComponent* InManager)
{
 TotalXP=FMath::Max(0,Value);PickupClass=Pickup;Experience=InExperience;Manager=InManager;
 ClosedLid=ChestMesh->GetBoneTransformByName(LidBone,EBoneSpaces::ComponentSpace);
}
void AEliteRewardChest::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(bRewardReleased)return;
 if(!bOpening)
 {
  auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
  auto* Character=Manager?Manager->GetActiveCharacter():nullptr;
  if(!PC||!Character||TotalXP<=0||!PC->IsRunInProgress()||PC->IsPlayerDead()||PC->IsSelectingUpgrade()||UGameplayStatics::IsGamePaused(this))return;
  if(FVector::DistSquared2D(Character->GetActorLocation(),GetActorLocation())>FMath::Square(CollectRadius))return;
  if(!PC->SetPause(true))return;
  Player=PC;PreviousView=PC->GetViewTarget();bOpening=true;bOwnsPause=true;
  OpeningStarted=LastRealTime=FPlatformTime::Seconds();
  RewardGlow->SetComponentTickEnabled(false);
  PC->SetViewTarget(this);
 }
 const double Now=FPlatformTime::Seconds();
 const float Elapsed=Now-OpeningStarted;
 const float Dt=FMath::Clamp(float(Now-LastRealTime),0.f,.1f);LastRealTime=Now;
 const float Open=FMath::SmoothStep(.8f,1.45f,Elapsed);
 if(!bPlayedOpeningSound&&Elapsed>=.8f)
 {
  bPlayedOpeningSound=true;
  if(OpeningSound)UGameplayStatics::PlaySound2D(this,OpeningSound,.85f,.75f);
 }
 FTransform Lid=ClosedLid;
 Lid.SetRotation(ClosedLid.GetRotation()*FQuat::Slerp(FQuat::Identity,LidOpenRotation.Quaternion(),Open));
 ChestMesh->SetBoneTransformByName(LidBone,Lid,EBoneSpaces::ComponentSpace);
 ChestMesh->RefreshBoneTransforms();
 ChestMesh->SetRelativeRotation(FRotator(0,0,Elapsed<.8f?FMath::Sin(Elapsed*48)*2.5f*(Elapsed/.8f):0));
 Camera->SetFieldOfView(FMath::Lerp(55.f,46.f,FMath::Clamp(Elapsed/OpeningDuration,0.f,1.f)));
 if(auto* PC=Player.Get())PC->UpdateCameraManager(Dt);
 Light->SetIntensity(700+Open*2300);
 // The world is paused; explicitly advance only the celebration's cosmetic system.
 if(Dt>0)RewardGlow->AdvanceSimulation(1,Dt);
 if(Elapsed>=FMath::Max(1.5f,OpeningDuration))FinishOpening();
}
void AEliteRewardChest::RestorePlayer()
{
 if(auto* PC=Player.Get())
 {
  if(PC->GetViewTarget()==this)PC->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():PC->GetPawn());
  if(bOwnsPause)PC->SetPause(false);
 }
 bOwnsPause=false;
}
void AEliteRewardChest::FinishOpening()
{
 if(bRewardReleased)return;
 bRewardReleased=true;
 RestorePlayer();
 const int32 Count=FMath::Min(TotalXP,FMath::Clamp(OrbCount,1,64));
 int32 FailedXP=0;
 for(int32 I=0;I<Count;++I)
 {
  const int32 Value=TotalXP/Count+(I<TotalXP%Count?1:0);
  const float Angle=I*2.39996323f;
  FVector Landing=GetActorLocation()+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*FMath::Lerp(100.f,280.f,FMath::Sqrt(float(I+1)/Count));
  FHitResult Hit;
  FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Landing+FVector(0,0,200),Landing-FVector(0,0,600),Objects,FCollisionQueryParams(SCENE_QUERY_STAT(ChestOrbGround),false,this)))Landing.Z=Hit.ImpactPoint.Z+18;
  const FTransform Spawn(FRotator::ZeroRotator,GetActorLocation()+FVector(0,0,65));
  auto* Orb=GetWorld()->SpawnActorDeferred<AExperiencePickup>(PickupClass,Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!Orb){FailedXP+=Value;continue;}
  Orb->LaunchFromChest(Landing,.6f+.012f*I,90.f+I%5*15.f);
  Orb->FinishSpawning(Spawn);
  Orb->InitializePickup(Value,Experience,Manager);
 }
 if(FailedXP>0&&Experience)Experience->AddXP(FailedXP);
 RewardGlow->SetComponentTickEnabled(true);
 RewardGlow->Deactivate();Light->SetIntensity(0);
 SetActorTickEnabled(false);SetLifeSpan(3);
}
void AEliteRewardChest::EndPlay(const EEndPlayReason::Type Reason)
{
 RestorePlayer();Super::EndPlay(Reason);
}
