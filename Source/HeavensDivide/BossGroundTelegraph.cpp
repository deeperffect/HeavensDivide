#include "BossGroundTelegraph.h"
#include "IndicatorAttackVFX.h"
#include "CombatAudio.h"

#include "CharacterBase.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"

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

void ABossGroundTelegraph::InitializeTelegraph(ASurvivorPlayerController* InPlayerController, float InRadius, float InDuration, float InDamage, UMaterialInterface* InMaterial)
{
	PlayerController = InPlayerController;
	Radius = FMath::Max(1.0f, InRadius);
	Duration = FMath::Max(0.01f, InDuration);
	Damage = FMath::Max(0.0f, InDamage);
	Elapsed = 0.f;
	bResolved = false;
	UMaterialInterface* Surface=InMaterial && InMaterial->GetMaterial()->MaterialDomain==MD_Surface ? InMaterial
		: LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorCircle"));
	InitializePersistentSurfaceCircle(Radius,Surface);
	if (!DynamicMaterial) { CancelTelegraph(); return; }
	bInitialized = true;
	SetActorTickEnabled(true);
}

void ABossGroundTelegraph::InitializePersistentCircle(float InRadius, UMaterialInterface* InMaterial)
{
	UMaterialInterface* Surface=InMaterial && InMaterial->GetMaterial()->MaterialDomain==MD_Surface ? InMaterial
		: LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorCircle"));
	InitializePersistentSurfaceCircle(InRadius,Surface);
}

void ABossGroundTelegraph::SetTelegraphFillAmount(float FillAmount)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("FillAmount"), FMath::Clamp(FillAmount, 0.0f, 1.0f));
		DynamicMaterial->SetScalarParameterValue(TEXT("Fill"), FMath::Clamp(FillAmount, 0.0f, 1.0f));
	}
}

void ABossGroundTelegraph::InitializePersistentSurfaceCircle(float InRadius,UMaterialInterface* InMaterial)
{
	Radius=FMath::Max(1.f,InRadius);
	InitializePersistentRectangle(Radius*2,Radius*2,InMaterial);
	ImpactShape = EEncounterShape::Circle; ImpactSize = Radius; ImpactWidth = 0;
}

void ABossGroundTelegraph::InitializePersistentRectangle(float InLength, float InWidth, UMaterialInterface* InMaterial)
{
	bInitialized = false;
	bImpactVFXSpawned = false;
	ImpactShape = EEncounterShape::Lane; ImpactSize = FMath::Max(1.f, InLength); ImpactWidth = FMath::Max(1.f, InWidth);
	CircleDecalVisual->SetHiddenInGame(true);
	CircleDecalVisual->SetVisibility(false);
	TelegraphVisual->SetHiddenInGame(false);
	TelegraphVisual->SetVisibility(true);
	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		TelegraphVisual->SetStaticMesh(PlaneMesh);
	}
	const float Length = FMath::Max(1.0f, InLength);
	const float Width = FMath::Max(1.0f, InWidth);
	TelegraphVisual->SetWorldScale3D(FVector(Length / 100.0f, Width / 100.0f, 1.f));
	if (InMaterial) TelegraphVisual->SetMaterial(0, InMaterial);
	DynamicMaterial = TelegraphVisual->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("LaneAspect"), Width / Length);
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
	if (Damage > 0) SpawnImpactVFX();
	if (ASurvivorPlayerController* PC = PlayerController.Get())
	{
		if (const ACharacterBase* Player = Cast<ACharacterBase>(PC->GetPawn()))
		{
			bHit = FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius);
			if (bHit) PC->ApplyDamageToPlayer(Damage);
		}
	}
	UCombatAudioLibrary::PlayEvent(this, TEXT("BossGroundHit"), GetActorLocation());
	OnTelegraphResolved(bHit);
	if(DynamicMaterial)DynamicMaterial->SetScalarParameterValue(TEXT("Impact"),1);
	SetLifeSpan(.35f);
}

void ABossGroundTelegraph::CancelTelegraph()
{
	bResolved = true;
	Destroy();
}

void ABossGroundTelegraph::PlayImpact()
{
	if (IsActorBeingDestroyed()) return;
	SpawnImpactVFX();
	bResolved=true;
	SetTelegraphFillAmount(1);
	if(DynamicMaterial)DynamicMaterial->SetScalarParameterValue(TEXT("Impact"),1);
	SetLifeSpan(.35f);
}

void ABossGroundTelegraph::SpawnImpactVFX()
{
	if (bImpactVFXSpawned) return;
	bImpactVFXSpawned = true;
	AIndicatorAttackVFX::Spawn(this, GetActorLocation(), ImpactShape, ImpactSize, ImpactWidth,
		GetActorRotation().Yaw, EIndicatorVFX::Impact);
}
