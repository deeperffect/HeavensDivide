#include "TechniqueTrialBase.h"

#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MinimapMarkerComponent.h"
#include "NinjaCharacter.h"
#include "ObjectiveInteractionComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "TrialArenaAnchor.h"
#include "TrialArenaSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ATechniqueTrialBase::ATechniqueTrialBase()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
	MinimapMarker = CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("MinimapMarker"));
	MinimapMarker->DisplayName = FText::FromString(TEXT("Technique Trial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh")); StatueMesh->SetupAttachment(SceneRoot); StatueMesh->SetRelativeScale3D(FVector(1.5,1.5,3));
	ObjectiveInteraction = CreateDefaultSubobject<UObjectiveInteractionComponent>(TEXT("ObjectiveInteraction")); ObjectiveInteraction->SetupAttachment(SceneRoot); ObjectiveInteraction->ConfigureDefaults(FText::FromString(TEXT("Samurai Trial")),300.0f,1.0f,240.0f);
	ObjectiveInteraction->SetPresentationVisual(StatueMesh);
	TrialFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrialFloor")); TrialFloor->SetupAttachment(SceneRoot); TrialFloor->SetRelativeLocation(TrialArenaOffset); TrialFloor->SetRelativeScale3D(FVector(30,30,.5));
	TrialPlayerStart = CreateDefaultSubobject<USceneComponent>(TEXT("TrialPlayerStart")); TrialPlayerStart->SetupAttachment(SceneRoot); TrialPlayerStart->SetRelativeLocation(TrialArenaOffset+TrialPlayerOffset);
	if (Cube.Succeeded()) { StatueMesh->SetStaticMesh(Cube.Object); TrialFloor->SetStaticMesh(Cube.Object); }
	const FVector L[]={{0,3000,300},{0,-3000,300},{3000,0,300},{-3000,0,300}}; const FVector S[]={{30,.5,3},{30,.5,3},{.5,30,3},{.5,30,3}};
	for(int32 I=0;I<4;++I){ auto* W=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("TrialWall%d"),I)); W->SetupAttachment(SceneRoot); W->SetRelativeLocation(TrialArenaOffset+L[I]); W->SetRelativeScale3D(S[I]); if(Cube.Succeeded())W->SetStaticMesh(Cube.Object); TrialWalls.Add(W); }
}

void ATechniqueTrialBase::BeginPlay(){ Super::BeginPlay(); FindReferences(); if(TrialPlayerStart)TrialPlayerStart->SetRelativeLocation(TrialArenaOffset+TrialPlayerOffset); const FVector LegacyOrigin=TrialFloor?TrialFloor->GetComponentLocation():GetActorTransform().TransformPosition(TrialArenaOffset); ArenaAnchor=GetWorld()->GetSubsystem<UTrialArenaSubsystem>()->ReserveAnchor(this); ArenaOrigin=LegacyOrigin; if(ArenaAnchor){ArenaOrigin=ArenaAnchor->GetActorLocation();const FVector Delta=ArenaOrigin-LegacyOrigin;if(TrialFloor)TrialFloor->AddWorldOffset(Delta);if(TrialPlayerStart)TrialPlayerStart->AddWorldOffset(Delta);for(UStaticMeshComponent* Wall:TrialWalls)if(Wall)Wall->AddWorldOffset(Delta);RelocateAdditionalArenaComponents(Delta);} bArenaReady=TrialFloor&&TrialFloor->IsRegistered()&&TrialPlayerStart&&TrialPlayerStart->IsRegistered()&&TrialFloor->GetCollisionEnabled()!=ECollisionEnabled::NoCollision; UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] Setup Trial=%s Entrance=%s Anchor=%s ArenaOrigin=%s Floor=%s PlayerStart=%s Ready=%s"),*GetName(),*GetActorLocation().ToCompactString(),*GetNameSafe(ArenaAnchor),*ArenaOrigin.ToCompactString(),*GetNameSafe(TrialFloor),*GetNameSafe(TrialPlayerStart),bArenaReady?TEXT("true"):TEXT("false")); }
void ATechniqueTrialBase::EndPlay(const EEndPlayReason::Type R){StopChallenge();CleanupRuntime(true);if(GetWorld())GetWorld()->GetSubsystem<UTrialArenaSubsystem>()->ReleaseAnchor(this);Super::EndPlay(R);}
bool ATechniqueTrialBase::CanInteract_Implementation(APawn* P)const{return TrialState==ETechniqueTrialState::Inactive&&P&&ObjectiveInteraction&&ObjectiveInteraction->IsInRange(P)&&(!PlayerController||!PlayerController->IsAnyObjectiveActive());}
void ATechniqueTrialBase::Interact_Implementation(APawn* P){EnterTrial(P);}

bool ATechniqueTrialBase::EnterTrial(APawn* P)
{
	const bool bCanInteract=CanInteract_Implementation(P);
	if(!bCanInteract)return false; FindReferences(); if(!PlayerController||!MainSpawner||PlayerController->IsPlayerDead()){UE_LOG(LogTemp,Error,TEXT("[TrialInteract] EntryReferencesInvalid Trial=%s Controller=%s Spawner=%s PlayerDead=%s"),*GetName(),*GetNameSafe(PlayerController),*GetNameSafe(MainSpawner),(PlayerController&&PlayerController->IsPlayerDead())?TEXT("true"):TEXT("false"));return false;}
	auto* M=PlayerController->GetCharacterManager(); auto* Before=M?M->GetActiveCharacter():nullptr; if(!Before){PlayerController->EndObjective(this);return false;} ReturnTransform=Before->GetActorTransform();ReturnTransform.AddToTranslation(ReturnOffset);
	FVector ArrivalLocation; if(!IsTrialArenaValid(Before,ArrivalLocation)){UE_LOG(LogTemp,Error,TEXT("[TrialRuntime] ERROR Trial arena/start invalid; refusing teleport Trial=%s Anchor=%s Floor=%s"),*GetName(),*GetNameSafe(ArenaAnchor),*GetNameSafe(TrialFloor));return false;}
	if(!PlayerController->TryBeginObjective(this))return false;
	if(bLockSwappingDuringTrial)PlayerController->SetSwapLocked(true);
	UAutoAttackComponent* SA=M->GetSamurai()?M->GetSamurai()->FindComponentByClass<UAutoAttackComponent>():nullptr; UAutoAttackComponent* NA=M->GetNinja()?M->GetNinja()->FindComponentByClass<UAutoAttackComponent>():nullptr;
	bSamuraiAutoAttackWasEnabled=SA&&SA->IsAutoAttackEnabled();bNinjaAutoAttackWasEnabled=NA&&NA->IsAutoAttackEnabled();
	if(!PrepareActiveCharacter()){CleanupRuntime(false);return false;}
	// Character-mode changes may restart the newly active character's autoattack.
	// Apply the existing state-preserving Trial suppression after that transition.
	if(ShouldSuspendAutoAttacksDuringTrial()){if(SA)SA->SetAutoAttackEnabled(false);if(NA)NA->SetAutoAttackEnabled(false);}
	bRuntimeOwned=true; MainSpawner->SetTrialSuspended(true); UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] Enter Trial=%s Return=%s Arrival=%s FloorTop=%.1f"),*GetName(),*ReturnTransform.GetLocation().ToCompactString(),*ArrivalLocation.ToCompactString(),TrialFloor->Bounds.Origin.Z+TrialFloor->Bounds.BoxExtent.Z); GetActiveCharacter()->SetActorLocation(ArrivalLocation,false,nullptr,ETeleportType::TeleportPhysics); TrialState=ETechniqueTrialState::Active; MinimapMarker->SetMarkerState(EMinimapMarkerState::Active); if(ObjectiveInteraction)ObjectiveInteraction->HidePrompt();
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
	if(PlayerController){auto* M=PlayerController->GetCharacterManager();if(ShouldSuspendAutoAttacksDuringTrial()&&M){if(auto* A=M->GetSamurai()?M->GetSamurai()->FindComponentByClass<UAutoAttackComponent>():nullptr)A->SetAutoAttackEnabled(bSamuraiAutoAttackWasEnabled&&!PlayerController->IsPlayerDead());if(auto* A=M->GetNinja()?M->GetNinja()->FindComponentByClass<UAutoAttackComponent>():nullptr)A->SetAutoAttackEnabled(bNinjaAutoAttackWasEnabled&&!PlayerController->IsPlayerDead());}if(bLockSwappingDuringTrial)PlayerController->SetSwapLocked(false);PlayerController->EndObjective(this);} bRuntimeOwned=false;
}
void ATechniqueTrialBase::FindReferences(){PlayerController=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));if(!MainSpawner&&GetWorld())for(TActorIterator<AEnemySpawner>It(GetWorld());It;++It){MainSpawner=*It;break;}}
ACharacterBase* ATechniqueTrialBase::GetActiveCharacter()const{return PlayerController&&PlayerController->GetCharacterManager()?PlayerController->GetCharacterManager()->GetActiveCharacter():nullptr;}

bool ATechniqueTrialBase::PrepareActiveCharacter()
{
	return !bForceSamuraiOnEntry||(PlayerController&&PlayerController->ForceSamuraiActive());
}

FVector ATechniqueTrialBase::GetGroundedTrialPlayerLocation(ACharacterBase* Character) const
{
	FVector Location=TrialPlayerStart?TrialPlayerStart->GetComponentLocation():GetArenaWorldTransform().TransformPositionNoScale(TrialPlayerOffset);
	if(!Character||!TrialFloor)return Location;
	const float FloorTop=TrialFloor->Bounds.Origin.Z+TrialFloor->Bounds.BoxExtent.Z;
	const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
	Location.Z=FloorTop+(Capsule?Capsule->GetScaledCapsuleHalfHeight():0.0f)+2.0f;
	return Location;
}

FTransform ATechniqueTrialBase::GetArenaWorldTransform() const
{
	return FTransform(TrialFloor?TrialFloor->GetComponentQuat():FQuat::Identity,ArenaOrigin);
}

bool ATechniqueTrialBase::IsTrialArenaValid(const ACharacterBase* Character, FVector& OutArrivalLocation) const
{
	OutArrivalLocation=GetGroundedTrialPlayerLocation(const_cast<ACharacterBase*>(Character));
	const bool bFloorValid=IsValid(TrialFloor);
	const bool bRegistered=bFloorValid&&TrialFloor->IsRegistered();
	const bool bCollisionEnabled=bFloorValid&&TrialFloor->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
	const FBox FloorBox=bFloorValid?TrialFloor->Bounds.GetBox():FBox(EForceInit::ForceInit);
	const FVector Probe(OutArrivalLocation.X,OutArrivalLocation.Y,bFloorValid?TrialFloor->Bounds.Origin.Z:0.0f);
	const bool bXYInside=bFloorValid&&FloorBox.IsInsideXY(Probe);
	const bool bZValid=bFloorValid&&OutArrivalLocation.Z>=FloorBox.Max.Z;
	const bool bValid=bArenaReady&&bFloorValid&&bRegistered&&bCollisionEnabled&&bXYInside&&bZValid;
	if(!bValid){UE_LOG(LogTemp,Error,TEXT("[TrialInteract] ArenaValidation Trial=%s Initialized=%d FloorValid=%d Registered=%d Collision=%d XYInside=%d ZValid=%d Arrival=%s FloorCenter=%s"),*GetName(),bArenaReady,bFloorValid,bRegistered,bCollisionEnabled,bXYInside,bZValid,*OutArrivalLocation.ToCompactString(),bFloorValid?*TrialFloor->Bounds.Origin.ToCompactString():TEXT("None"));}
	return bValid;
}
