#include "BossGroundTelegraph.h"

#include "CharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ABossGroundTelegraph::ABossGroundTelegraph()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	TelegraphVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphVisual"));
	SetRootComponent(TelegraphVisual);
	TelegraphVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TelegraphVisual->SetCastShadow(false);
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
	TelegraphVisual->SetWorldScale3D(FVector(0.001f, 0.001f, 0.025f));
	if (InMaterial) TelegraphVisual->SetMaterial(0, InMaterial);
	DynamicMaterial = TelegraphVisual->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("FillColor"), TelegraphColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), 0.0f);
	}
	bInitialized = true;
	SetActorTickEnabled(true);
}

void ABossGroundTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || bResolved) return;
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
	if (DynamicMaterial) DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), Alpha);
	const float FilledRadius = Radius * FMath::Max(0.001f, Alpha);
	TelegraphVisual->SetWorldScale3D(FVector(FilledRadius / 50.0f, FilledRadius / 50.0f, 0.025f));
	if (Alpha < 1.0f) return;

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
