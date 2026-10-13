#include "EliteRewardChest.h"
#include "ChestRewardWidget.h"
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
#include "CombatGround.h"
#include "RunChallengeSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/GameInstance.h"

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
 static ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/HeavensDivide/Audio/MetaSounds/MS_ChestOpen"));
 OpeningSound=Sound.Object;
 static ConstructorHelpers::FObjectFinder<USoundBase> Jackpot(TEXT("/Game/HeavensDivide/Audio/MetaSounds/MS_ChestJackpot"));
 JackpotSound=Jackpot.Object;
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
 if(bRewardInitialized||bRewardReleased)return;
 bRewardInitialized=true;
 FRandomStream Random(FMath::Rand());RolledReward=ChestRewards::Roll(Value,Random);
 DisplayedReward=RolledReward;
 PickupClass=Pickup;Experience=InExperience;Manager=InManager;
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
  if(!PC||!Character||RolledReward.XP<=0||!PC->IsRunInProgress()||PC->IsPlayerDead()||PC->IsSelectingUpgrade()||UGameplayStatics::IsGamePaused(this))return;
  if(FVector::DistSquared2D(Character->GetActorLocation(),GetActorLocation())>FMath::Square(CollectRadius))return;
  if(!PC->SetPause(true))return;
  PC->BeginMenuMovement();
  Player=PC;PreviousView=PC->GetViewTarget();bOpening=true;bOwnsPause=true;
  DisplayedReward=RolledReward;
  if(auto* GI=GetGameInstance())if(auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>())
  {
   const auto Currency=Meta->PreviewCurrencyReward(RolledReward.Embers,RolledReward.Sigils);
   DisplayedReward.Embers=Currency.X;DisplayedReward.Sigils=Currency.Y;
  }
  OpeningStarted=LastRealTime=FPlatformTime::Seconds();
  RewardGlow->SetComponentTickEnabled(false);
  if(PC->IsLocalController() && PC->GetLocalPlayer())
  {
   RewardWidget=CreateWidget<UChestRewardWidget>(PC);
   if(RewardWidget)
   {
    bPreviousMouseCursor=PC->bShowMouseCursor;PC->bShowMouseCursor=true;
    RewardWidget->Present(this,DisplayedReward,0,GetRevealDuration());RewardWidget->AddToViewport(150);
    FInputModeUIOnly Input;Input.SetWidgetToFocus(RewardWidget->TakeWidget());Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Input);RewardWidget->SetKeyboardFocus();
   }
  }
 }
 const double Now=FPlatformTime::Seconds();
 if(bSkipRequested && Now-OpeningStarted>.25)
 { OpeningStarted=FMath::Min(OpeningStarted,Now-(GetRevealDuration()-.65f));bSkipRequested=false; }
 const float Elapsed=Now-OpeningStarted;
 const float RevealTime=Elapsed*3.6f/GetRevealDuration();
 if(RewardWidget)RewardWidget->Present(this,DisplayedReward,Elapsed,GetRevealDuration());
 const float Dt=FMath::Clamp(float(Now-LastRealTime),0.f,.1f);LastRealTime=Now;
 const float Open=FMath::SmoothStep(.8f,1.45f,RevealTime);
 if(!bPlayedOpeningSound&&RevealTime>=.8f)
 {
  bPlayedOpeningSound=true;
  if(OpeningSound)UGameplayStatics::PlaySound2D(this,OpeningSound,.85f,.75f);
 }
 if(RolledReward.IsJackpot()&&!bPlayedJackpotSound&&RevealTime>=1.1f)
 {
  bPlayedJackpotSound=true;
  if(JackpotSound)UGameplayStatics::PlaySound2D(this,JackpotSound,.9f,1.f);
 }
 FTransform Lid=ClosedLid;
 Lid.SetRotation(ClosedLid.GetRotation()*FQuat::Slerp(FQuat::Identity,LidOpenRotation.Quaternion(),Open));
 ChestMesh->SetBoneTransformByName(LidBone,Lid,EBoneSpaces::ComponentSpace);
 ChestMesh->RefreshBoneTransforms();
 ChestMesh->SetRelativeRotation(FRotator(0,0,Elapsed<.8f?FMath::Sin(Elapsed*48)*2.5f*(Elapsed/.8f):0));
 Camera->SetFieldOfView(FMath::Lerp(55.f,46.f,FMath::Clamp(Elapsed/GetRevealDuration(),0.f,1.f)));
 if(auto* PC=Player.Get())PC->UpdateCameraManager(Dt);
 Light->SetIntensity(700+Open*(RolledReward.IsJackpot()?4600:2300));
 // The world is paused; explicitly advance only the celebration's cosmetic system.
 if(Dt>0)RewardGlow->AdvanceSimulation(1,Dt);
 if(Elapsed>=GetRevealDuration())FinishOpening();
}
void AEliteRewardChest::RestorePlayer()
{
 const bool bHadWidget=RewardWidget!=nullptr;
 if(RewardWidget){RewardWidget->RemoveFromParent();RewardWidget=nullptr;}
 if(auto* PC=Player.Get())
 {
  if(bHadWidget)
  {
   PC->bShowMouseCursor=bPreviousMouseCursor;
   FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Input);
  }
  if(PC->GetViewTarget()==this)PC->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():PC->GetPawn());
  if(bOwnsPause) { PC->SetPause(false); PC->EndMenuMovement(); }
 }
 bOwnsPause=false;
}
void AEliteRewardChest::FinishOpening()
{
 if(bRewardReleased)return;
 bRewardReleased=true;
 RunChallenges::AwardCurrencies(this,RolledReward.Embers,RolledReward.Sigils);
 RestorePlayer();
 const int32 TotalXP=RolledReward.XP;
 const int32 Count=FMath::Min(TotalXP,FMath::Clamp(OrbCount,1,64));
 int32 FailedXP=0;
 for(int32 I=0;I<Count;++I)
 {
  const int32 Value=TotalXP/Count+(I<TotalXP%Count?1:0);
  const float Angle=I*2.39996323f;
  FVector Landing=GetActorLocation()+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*FMath::Lerp(100.f,280.f,FMath::Sqrt(float(I+1)/Count));
  Landing=CombatGround::ClampInsideArena(GetWorld(),Landing,18.f);
  FVector Ground;
  if(CombatGround::FindSurface(GetWorld(),Landing,this,Ground))Landing.Z=Ground.Z+18;
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
