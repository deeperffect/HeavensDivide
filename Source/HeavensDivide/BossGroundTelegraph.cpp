#include "BossGroundTelegraph.h"

#include "CharacterBase.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ABossGroundTelegraph::ABossGroundTelegraph()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	TelegraphVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphVisual"));
	SetRootComponent(TelegraphVisual);
	TelegraphVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TelegraphVisual->SetCastShadow(false);
	CircleDecalVisual = CreateDefaultSubobject<UDecalComponent>(TEXT("CircleDecalVisual"));
	CircleDecalVisual->SetupAttachment(TelegraphVisual);
	CircleDecalVisual->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	CircleDecalVisual->SetComponentTickEnabled(false);
	CircleDecalVisual->SetHiddenInGame(true);
	CircleDecalVisual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded()) TelegraphVisual->SetStaticMesh(Cylinder.Object);
}

void ABossGroundTelegraph::BeginPlay()
{
	Super::BeginPlay();
}

void ABossGroundTelegraph::InitializeTelegraph(ASurvivorPlayerController* InPlayerController, float InRadius, float InDuration, float InDamage, UMaterialInterface* InMaterial)
{
	PlayerController = InPlayerController;
	Radius = FMath::Max(1.0f, InRadius);
	Duration = FMath::Max(0.01f, InDuration);
	Damage = FMath::Max(0.0f, InDamage);
	// V1 deliberately uses a static, fully visible circle. Damage timing remains
	// independent and one-shot; no directional/fill material behavior is required.
	TelegraphVisual->SetHiddenInGame(true);
	TelegraphVisual->SetVisibility(false);
	CircleDecalVisual->DecalSize = FVector(64.0f, Radius, Radius);
	if (InMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(InMaterial, this);
		CircleDecalVisual->SetDecalMaterial(DynamicMaterial);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("FillColor"), TelegraphColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), 0.0f);
	}
	CircleDecalVisual->SetHiddenInGame(false);
	CircleDecalVisual->SetVisibility(true);
	bInitialized = true;
	SetActorTickEnabled(true);
}

void ABossGroundTelegraph::InitializePersistentCircle(float InRadius, UMaterialInterface* InMaterial)
{
	Radius = FMath::Max(1.0f, InRadius);
	TelegraphVisual->SetHiddenInGame(true);
	TelegraphVisual->SetVisibility(false);
	CircleDecalVisual->DecalSize = FVector(64.0f, Radius, Radius);
	if (InMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(InMaterial, this);
		CircleDecalVisual->SetDecalMaterial(DynamicMaterial);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("FillColor"), TelegraphColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), 1.0f);
	}
	CircleDecalVisual->SetHiddenInGame(false);
	CircleDecalVisual->SetVisibility(true);
	SetActorTickEnabled(false);
}

void ABossGroundTelegraph::SetTelegraphFillAmount(float FillAmount)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), FMath::Clamp(FillAmount, 0.0f, 1.0f));
	}
}

void ABossGroundTelegraph::InitializePersistentRectangle(float InLength, float InWidth, UMaterialInterface* InMaterial)
{
	CircleDecalVisual->SetHiddenInGame(true);
	CircleDecalVisual->SetVisibility(false);
	TelegraphVisual->SetHiddenInGame(false);
	TelegraphVisual->SetVisibility(true);
	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		TelegraphVisual->SetStaticMesh(CubeMesh);
	}
	const float Length = FMath::Max(1.0f, InLength);
	const float Width = FMath::Max(1.0f, InWidth);
	TelegraphVisual->SetWorldScale3D(FVector(Length / 100.0f, Width / 100.0f, 0.025f));
	if (InMaterial) TelegraphVisual->SetMaterial(0, InMaterial);
	DynamicMaterial = TelegraphVisual->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("FillColor"), TelegraphColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), 0.0f);
	}
	SetActorTickEnabled(false);
}

void ABossGroundTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || bResolved) return;
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
	SetTelegraphFillAmount(Alpha);
	if (Elapsed < Duration) return;

	bResolved = true;
	bool bHit = false;
	if (ASurvivorPlayerController* PC = PlayerController.Get())
	{
		if (const ACharacterBase* Player = Cast<ACharacterBase>(PC->GetPawn()))
		{
			bHit = FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius);
			if (bHit) PC->ApplyDamageToPlayer(Damage);
		}
	}
	OnTelegraphResolved(bHit);
	Destroy();
}

void ABossGroundTelegraph::CancelTelegraph()
{
	bResolved = true;
	Destroy();
}
