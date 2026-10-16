#include "ObjectiveBeaconComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

UObjectiveBeaconComponent::UObjectiveBeaconComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/HeavensDivide/Materials/Objectives/M_ObjectiveBeaconAura"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Pointer(TEXT("/Game/HeavensDivide/Materials/Objectives/M_ObjectiveBeaconArrow"));
    AuraMaterial=Ground.Object;ArrowMaterial=Pointer.Object;
}
void UObjectiveBeaconComponent::Configure(UStaticMeshComponent* Visual,FLinearColor Color)
{
    SourceVisual=Visual;Tint=Color;
}
void UObjectiveBeaconComponent::BeginPlay()
{
    Super::BeginPlay();
    auto* Owner=GetOwner();
    if(!Owner||Owner->GetNetMode()==NM_DedicatedServer||!SourceVisual||!SourceVisual->GetStaticMesh())return;
    Aura=NewObject<UStaticMeshComponent>(Owner,TEXT("ObjectiveBeaconAura"));
    Owner->AddInstanceComponent(Aura);Aura->SetupAttachment(Owner->GetRootComponent());
    Aura->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Aura->SetCollisionEnabled(ECollisionEnabled::NoCollision);Aura->SetCanEverAffectNavigation(false);
    Aura->SetCastShadow(false);Aura->SetRelativeLocation(FVector(0,0,9));
    Aura->SetAbsolute(false,true,true);Aura->SetWorldScale3D(FVector(5.f));
    Aura->SetMaterial(0,AuraMaterial);Aura->RegisterComponent();
    if(auto* MID=Aura->CreateDynamicMaterialInstance(0))MID->SetVectorParameterValue(TEXT("Tint"),Tint);

    Arrow=NewObject<UMaterialBillboardComponent>(Owner,TEXT("ObjectiveBeaconArrow"));
    Owner->AddInstanceComponent(Arrow);Arrow->SetupAttachment(SourceVisual);
    Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);Arrow->SetCanEverAffectNavigation(false);Arrow->SetCastShadow(false);
    Arrow->SetAbsolute(false,true,true);
    const FBox Bounds=SourceVisual->GetStaticMesh()->GetBoundingBox();
    const double ScaleZ=FMath::Max(.01,FMath::Abs(SourceVisual->GetComponentScale().Z));
    Arrow->SetRelativeLocation(FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Max.Z+150./ScaleZ));
    if(auto* MID=UMaterialInstanceDynamic::Create(ArrowMaterial,Arrow))
    {
        MID->SetVectorParameterValue(TEXT("Tint"),Tint);
        Arrow->AddElement(MID,nullptr,false,110.f,110.f,nullptr);
    }
    Arrow->SetTranslucentSortPriority(15);Arrow->RegisterComponent();
    SetBeaconState(bBeaconVisible,bBeaconAvailable);
}
void UObjectiveBeaconComponent::SetBeaconState(bool bVisible,bool bAvailable)
{
    bBeaconVisible=bVisible;bBeaconAvailable=bAvailable;
    if(Aura)Aura->SetVisibility(bVisible);
    if(Arrow)Arrow->SetVisibility(bVisible&&bAvailable);
}
bool UObjectiveBeaconComponent::IsArrowVisible() const {return Arrow&&Arrow->IsVisible();}
bool UObjectiveBeaconComponent::IsAuraVisible() const {return Aura&&Aura->IsVisible();}
void UObjectiveBeaconComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Arrow)Arrow->DestroyComponent();
    if(Aura)Aura->DestroyComponent();
    Super::EndPlay(Reason);
}
