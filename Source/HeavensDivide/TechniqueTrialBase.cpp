#include "TechniqueTrialBase.h"

#include "AutoAttackComponent.h"
#include "BloodShrineWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MinimapMarkerComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATechniqueTrialBase::ATechniqueTrialBase()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
	MinimapMarker = CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("MinimapMarker"));
	MinimapMarker->DisplayName = FText::FromString(TEXT("Technique Trial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh")); StatueMesh->SetupAttachment(SceneRoot); StatueMesh->SetRelativeScale3D(FVector(1.5,1.5,3));
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere")); InteractionSphere->SetupAttachment(SceneRoot); InteractionSphere->InitSphereRadius(InteractionRange); InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore); InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	InteractionPrompt = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPrompt")); InteractionPrompt->SetupAttachment(SceneRoot); InteractionPrompt->SetWidgetSpace(EWidgetSpace::World); InteractionPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision); InteractionPrompt->SetVisibility(false);
	TrialFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrialFloor")); TrialFloor->SetupAttachment(SceneRoot); TrialFloor->SetRelativeLocation(TrialArenaOffset); TrialFloor->SetRelativeScale3D(FVector(30,30,.5));
	if (Cube.Succeeded()) { StatueMesh->SetStaticMesh(Cube.Object); TrialFloor->SetStaticMesh(Cube.Object); }
	const FVector L[]={{0,3000,300},{0,-3000,300},{3000,0,300},{-3000,0,300}}; const FVector S[]={{30,.5,3},{30,.5,3},{.5,30,3},{.5,30,3}};
	for(int32 I=0;I<4;++I){ auto* W=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("TrialWall%d"),I)); W->SetupAttachment(SceneRoot); W->SetRelativeLocation(TrialArenaOffset+L[I]); W->SetRelativeScale3D(S[I]); if(Cube.Succeeded())W->SetStaticMesh(Cube.Object); TrialWalls.Add(W); }
}

void ATechniqueTrialBase::BeginPlay(){ Super::BeginPlay(); FindReferences(); InteractionSphere->SetSphereRadius(InteractionRange); InteractionPrompt->SetRelativeLocation(FVector(0,0,PromptVerticalOffset)); InteractionPrompt->SetDrawSize(FVector2D(FMath::Max(1.0f,PromptDrawSize.X),FMath::Max(1.0f,PromptDrawSize.Y))); InteractionPrompt->SetRelativeScale3D(FVector(FMath::Max(0.01f,PromptWorldScale))); InteractionPrompt->SetWidgetClass(UBloodShrineWidget::StaticClass()); InteractionPrompt->InitWidget(); if(auto* W=Cast<UBloodShrineWidget>(InteractionPrompt->GetUserWidgetObject())){W->ConfigureForWorldSpace();W->ShowInteractionPrompt(PromptTitle);} }
void ATechniqueTrialBase::Tick(float D){Super::Tick(D);if(TrialState==ETechniqueTrialState::Inactive)UpdatePrompt();FacePromptToCamera();}
void ATechniqueTrialBase::EndPlay(const EEndPlayReason::Type R){StopChallenge();CleanupRuntime(true);Super::EndPlay(R);}
bool ATechniqueTrialBase::CanInteract_Implementation(APawn* P)const{return TrialState==ETechniqueTrialState::Inactive&&P&&(!PlayerController||!PlayerController->IsAnyObjectiveActive())&&FVector::DistSquared2D(P->GetActorLocation(),GetActorLocation())<=FMath::Square(InteractionRange);}
void ATechniqueTrialBase::Interact_Implementation(APawn* P){EnterTrial(P);}

bool ATechniqueTrialBase::EnterTrial(APawn* P)
{
	if(!CanInteract_Implementation(P))return false; FindReferences(); if(!PlayerController||!MainSpawner||PlayerController->IsPlayerDead()||!PlayerController->TryBeginObjective(this))return false;
	auto* M=PlayerController->GetCharacterManager(); auto* Before=M?M->GetActiveCharacter():nullptr; if(!Before){PlayerController->EndObjective(this);return false;} ReturnTransform=Before->GetActorTransform();ReturnTransform.AddToTranslation(ReturnOffset);
	if(bLockSwappingDuringTrial)PlayerController->SetSwapLocked(true);
	UAutoAttackComponent* SA=M->GetSamurai()?M->GetSamurai()->FindComponentByClass<UAutoAttackComponent>():nullptr; UAutoAttackComponent* NA=M->GetNinja()?M->GetNinja()->FindComponentByClass<UAutoAttackComponent>():nullptr;
	bSamuraiAutoAttackWasEnabled=SA&&SA->IsAutoAttackEnabled();bNinjaAutoAttackWasEnabled=NA&&NA->IsAutoAttackEnabled();if(bSuspendAutoAttacksDuringTrial){if(SA)SA->SetAutoAttackEnabled(false);if(NA)NA->SetAutoAttackEnabled(false);}
	if(!PrepareActiveCharacter()){CleanupRuntime(false);return false;}
	bRuntimeOwned=true; MainSpawner->SetTrialSuspended(true); GetActiveCharacter()->SetActorLocation(GetGroundedTrialPlayerLocation(GetActiveCharacter()),false,nullptr,ETeleportType::TeleportPhysics); TrialState=ETechniqueTrialState::Active; MinimapMarker->SetMarkerState(EMinimapMarkerState::Active); InteractionPrompt->SetVisibility(false);
	if(auto* H=PlayerController->GetPlayerHealthComponent())H->OnDeath.AddUniqueDynamic(this,&ATechniqueTrialBase::HandlePlayerDeath);
	if(!BeginChallenge()){AbortTrial();return false;} OnTrialEntered.Broadcast(); return true;
}

void ATechniqueTrialBase::FinishChallenge(){if(TrialState!=ETechniqueTrialState::Active||!PlayerController||PlayerController->IsPlayerDead())return;TrialState=ETechniqueTrialState::Result;MinimapMarker->SetMarkerState(EMinimapMarkerState::Completed);StopChallenge();OnTechniqueTrialCompleted.Broadcast();GetWorldTimerManager().SetTimer(ResultTimer,this,&ATechniqueTrialBase::ReturnToArena,ResultDisplayDuration,false);}
void ATechniqueTrialBase::AbortTrial(){if(TrialState==ETechniqueTrialState::Completed||TrialState==ETechniqueTrialState::Failed)return;TrialState=ETechniqueTrialState::Failed;MinimapMarker->SetMarkerState(EMinimapMarkerState::Failed);StopChallenge();CleanupRuntime(true);}
void ATechniqueTrialBase::HandlePlayerDeath(){AbortTrial();}
void ATechniqueTrialBase::ReturnToArena(){TrialState=bAllowReactivation?ETechniqueTrialState::Inactive:ETechniqueTrialState::Completed;MinimapMarker->SetMarkerState(bAllowReactivation?EMinimapMarkerState::Available:EMinimapMarkerState::Completed);StopChallenge();CleanupRuntime(true);OnPlayerReturned.Broadcast();}
void ATechniqueTrialBase::CleanupRuntime(bool bReturnPlayer)
{
	GetWorldTimerManager().ClearTimer(ResultTimer); if(PlayerController){if(auto* H=PlayerController->GetPlayerHealthComponent())H->OnDeath.RemoveDynamic(this,&ATechniqueTrialBase::HandlePlayerDeath);}
	if(bRuntimeOwned&&bReturnPlayer)if(auto* C=GetActiveCharacter())C->SetActorTransform(ReturnTransform,false,nullptr,ETeleportType::TeleportPhysics);
	if(bRuntimeOwned&&MainSpawner)MainSpawner->SetTrialSuspended(false);
	if(PlayerController){auto* M=PlayerController->GetCharacterManager();if(bSuspendAutoAttacksDuringTrial&&M){if(auto* A=M->GetSamurai()?M->GetSamurai()->FindComponentByClass<UAutoAttackComponent>():nullptr)A->SetAutoAttackEnabled(bSamuraiAutoAttackWasEnabled&&!PlayerController->IsPlayerDead());if(auto* A=M->GetNinja()?M->GetNinja()->FindComponentByClass<UAutoAttackComponent>():nullptr)A->SetAutoAttackEnabled(bNinjaAutoAttackWasEnabled&&!PlayerController->IsPlayerDead());}if(bLockSwappingDuringTrial)PlayerController->SetSwapLocked(false);PlayerController->EndObjective(this);} bRuntimeOwned=false;
}
void ATechniqueTrialBase::FindReferences(){PlayerController=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));if(!MainSpawner&&GetWorld())for(TActorIterator<AEnemySpawner>It(GetWorld());It;++It){MainSpawner=*It;break;}}
void ATechniqueTrialBase::UpdatePrompt(){if(InteractionPrompt&&PlayerController)InteractionPrompt->SetVisibility(CanInteract_Implementation(PlayerController->GetPawn()));}
void ATechniqueTrialBase::FacePromptToCamera(){if(InteractionPrompt&&InteractionPrompt->IsVisible()&&PlayerController&&PlayerController->PlayerCameraManager)InteractionPrompt->SetWorldRotation((PlayerController->PlayerCameraManager->GetCameraLocation()-InteractionPrompt->GetComponentLocation()).Rotation());}
ACharacterBase* ATechniqueTrialBase::GetActiveCharacter()const{return PlayerController&&PlayerController->GetCharacterManager()?PlayerController->GetCharacterManager()->GetActiveCharacter():nullptr;}

bool ATechniqueTrialBase::PrepareActiveCharacter()
{
	return !bForceSamuraiOnEntry||(PlayerController&&PlayerController->ForceSamuraiActive());
}

FVector ATechniqueTrialBase::GetGroundedTrialPlayerLocation(ACharacterBase* Character) const
{
	FVector Location=GetTrialOrigin()+TrialPlayerOffset;
	if(!Character||!TrialFloor)return Location;
	const float FloorTop=TrialFloor->Bounds.Origin.Z+TrialFloor->Bounds.BoxExtent.Z;
	const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
	Location.Z=FloorTop+(Capsule?Capsule->GetScaledCapsuleHalfHeight():0.0f)+2.0f;
	return Location;
}
