// Copyright Epic Games, Inc. All Rights Reserved.
#include "SamuraiBladeWave.h"
#include "CrescentBuild.h"
#include "SamuraiWaveField.h"
#include "TimerManager.h"
#include "SurvivorAbilityComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystemInstanceController.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASamuraiBladeWave::ASamuraiBladeWave()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ASamuraiBladeWave::HandleOverlap);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Visual->SetStaticMesh(Cube.Object);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
}

void ASamuraiBladeWave::InitializeBladeWave(ASamuraiCharacter* InSamurai, UPlayerUpgradeComponent* InUpgrades, FVector Direction,
	float InDamage, float InWidth, float InTravelDistance, float InSpeed, bool bInReturns, float InAreaScale, float InFieldDamage,
    TOptional<bool> InInheritedFieldProc)
{
	SourceSamurai = InSamurai;
    SourceUpgrades = InUpgrades;
    WaveWidth = InWidth; TravelDistance = InTravelDistance; VisualAreaScale = InAreaScale;
    LaunchOrigin = GetActorLocation();
    bReturnToLaunch = InUpgrades && InUpgrades->HasUpgradeId(TEXT("ReturningBlade"));
    ReturnDamageScale = bReturnToLaunch
        ? FMath::Max(0.f, CrescentBuild::Value(InUpgrades, TEXT("ReturningBlade"), TEXT("ReturnDamageMultiplier"), .5f)) : 1.f;
    FieldDamage = InFieldDamage < 0.f ? InDamage : InFieldDamage;
    // Split waves inherit both successful and failed results without another RNG roll.
    // Resolve before activating presentation so debris matches the inherited field.
    bFieldTriggered = InInheritedFieldProc.IsSet() ? InInheritedFieldProc.GetValue()
        : InUpgrades && InUpgrades->HasUpgradeId(TEXT("BladeWave")) && InUpgrades->HasUpgradeId(TEXT("CrescentField"))
            && FMath::FRand() < CrescentBuild::Chance(InUpgrades, TEXT("CrescentField"), TEXT("CrescentFieldChance"), .15f, .05f);
    bFieldPending = bFieldTriggered;
 auto* Abilities=InSamurai&&InSamurai->GetOwner()?InSamurai->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>():nullptr;
 if(Abilities){WaveThickness=Abilities->Tuning(4,TEXT("WaveThickness"),WaveThickness);WaveHeight=Abilities->Tuning(4,TEXT("WaveHeight"),WaveHeight);VFXAuthoredDuration=Abilities->Tuning(4,TEXT("WaveVFXAuthoredDuration"),VFXAuthoredDuration);}
	Damage = FMath::Max(0.0f, InDamage);
	Speed = FMath::Max(1.0f, InSpeed);
	bReturns = bInReturns;
	Direction.Z = 0.0f;
	if (!Direction.Normalize()) { Destroy(); return; }
	// Preserve Blueprint-authored transforms before the collision visualization is resized.
	// Niagara must not inherit the cube's nonuniform scale as well as its own Area parameter.
	TInlineComponentArray<UNiagaraComponent*> Effects(this);
	WaveEffects.Reset();
	for (UNiagaraComponent* Effect : Effects)
	{
		Effect->SetAutoDestroy(false);
		Effect->DeactivateImmediate();
		Effect->AttachToComponent(Collision, FAttachmentTransformRules::KeepWorldTransform);
		if (!VFXAreaScaleParameter.IsNone())
			Effect->SetVariableFloat(VFXAreaScaleParameter, FMath::Max(0.0f, InAreaScale));
		WaveEffects.Add(Effect);
	}
	Collision->SetBoxExtent(FVector(WaveThickness * 0.5f, FMath::Max(1.0f, InWidth) * 0.5f, WaveHeight * 0.5f));
	Visual->SetRelativeScale3D(FVector(WaveThickness / 100.0f, FMath::Max(1.0f, InWidth) / 100.0f, WaveHeight / 100.0f));
	SetActorRotation(Direction.Rotation());
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction * Speed;
	Collision->IgnoreActorWhenMoving(InSamurai, true);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	const float Duration = FMath::Max(0.01f, FMath::Max(1.0f, InTravelDistance) / Speed);
 if(Abilities)
 {
  const auto* Settings=Abilities->FamilyPresentation(4);
  if(Settings&&Settings->PulseSystem)
  {
   bGroundSlash = Settings->bGroundSlashMotion;
   if (bGroundSlash)
   {
    GroundPresentation = *Settings;
    PhaseOrigin = GetActorLocation();
    PhaseDirection = Direction;
    PhaseDistance = FMath::Max(1.f, InTravelDistance);
    PhaseAge = 0.f;
    FollowGround();
    StartGroundVisual();
    SetActorTickEnabled(true);
   }
   else
   {
   AssignedVisual=Abilities->FamilyAccent(4,GetActorLocation(),GetActorLocation()+Direction*InTravelDistance,InWidth*0.5f,FLinearColor::White,Duration*(bReturns?2:1));
   if(AssignedVisual.IsValid()){AssignedVisual->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);AssignedVisual->SetActorRotation(Direction.Rotation());}
   }
   if(!Settings->bShowFallbackWithNiagara){Visual->SetVisibility(false);for(auto E:WaveEffects)if(E.IsValid())E->DeactivateImmediate();WaveEffects.Reset();}
  }
 }
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : WaveEffects)
	{
		if (!Effect.IsValid()) continue;
		Effect->SetCustomTimeDilation(FMath::Max(0.001f, VFXAuthoredDuration) / (Duration * (bReturns ? 2.0f : 1.0f)));
		Effect->Activate(true);
	}
	if (!bGroundSlash)
		GetWorldTimerManager().SetTimer(PhaseTimer, this, bReturns ? &ASamuraiBladeWave::BeginReturn : &ASamuraiBladeWave::FinishWave, Duration, false);
    // Ground following may have moved the collision box since actor spawning.
    FieldTrailStart = Collision->GetComponentLocation();
	OnOutboundStarted.Broadcast(this);
}

void ASamuraiBladeWave::FollowGround()
{
    const FVector Position = GetActorLocation();
    const float Trace = FMath::Max(1.f, GroundPresentation.GroundTraceDistance);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BladeWaveGround), false, this);
    if (SourceSamurai.IsValid()) Query.AddIgnoredActor(SourceSamurai.Get());
    FHitResult Hit;
    // Restrict ground detection to static geometry, so enemies cannot lift the wave.
    if (GetWorld()->LineTraceSingleByObjectType(Hit, Position + FVector(0,0,Trace),
        Position - FVector(0,0,Trace), FCollisionObjectQueryParams(ECC_WorldStatic), Query))
    {
        GroundAnchor = Hit.ImpactPoint + FVector(0,0,GroundPresentation.GroundOffset);
        SetActorLocation(FVector(Position.X, Position.Y, Hit.ImpactPoint.Z + WaveHeight * .5f), false);
    }
    else
        GroundAnchor = Position - FVector(0,0,WaveHeight * .5f) + FVector(0,0,GroundPresentation.GroundOffset);
}

void ASamuraiBladeWave::StartGroundVisual()
{
    auto* Accent = GetWorld()->SpawnActor<AAbilityAccent>(GroundAnchor, GetActorRotation());
    if (!Accent) return;
    AssignedVisual = Accent;
    const float Duration = FMath::Max(.1f, GroundPresentation.MaxTravelTime);
    Accent->Initialize(GroundAnchor + PhaseDirection * PhaseDistance,
        Collision->GetUnscaledBoxExtent().Y, FLinearColor::White, Duration, false, &GroundPresentation);
    Accent->SetActorRotation(GetActorRotation() + GroundPresentation.RotationOffset);
    // Override only this wave's debris emitters. Crescent, glowing ground marks,
    // trails and sparks keep their authored settings on the shared Niagara asset.
    // Component overrides survive StartGroundSlash's activation reset and returns.
    if (auto* Effect = Accent->FindComponentByClass<UNiagaraComponent>(); Effect && Effect->GetAsset())
    {
        // Keep even the earliest rocks alive through the longest outbound phase
        // and upgraded field. The field owns exact cleanup, not particle expiry.
        const float Rate = FMath::Max(0.f, GroundPresentation.SlowdownRate);
        const float Fit = Rate > 0.f ? PhaseDistance / (Speed * (FMath::Max(0.f, GroundPresentation.SlowdownDelay) + 1.f / Rate)) : 1.f;
        const float MaxTravel = FMath::Max(.1f, GroundPresentation.MaxTravelTime)
            * (GroundPresentation.bFitSlowdownToRange ? FMath::Max(1.f, Fit) : 1.f);
        const float FieldDuration = FMath::Max(.01f, CrescentBuild::Value(SourceUpgrades.Get(), TEXT("CrescentField"), TEXT("Duration"), 3.f))
            * (1.f + CrescentBuild::Scaling(SourceUpgrades.Get(), TEXT("CrescentFieldPower"), .15f));
        // The authored scale curve stays visible throughout this interval, even
        // if duration ranks are acquired during travel. Air debris keeps its physics.
        Effect->SetVariableFloat(TEXT("User.GroundDebrisLifetime"), 3.f * (MaxTravel + FieldDuration + 1.f));
        for (const auto& Emitter : Effect->GetAsset()->GetEmitterHandles())
            if (Emitter.GetName().ToString().StartsWith(TEXT("Debris")))
                Effect->SetEmitterEnable(Emitter.GetName(), bFieldTriggered
                    && (!bReturning || !Emitter.GetName().ToString().StartsWith(TEXT("DebrisGround"))));
        // Range/speed scaling can extend travel beyond the presentation default.
        Accent->StartGroundSlash(GroundPresentation.DebrisMaterial, MaxTravel + FieldDuration + 1.f);
    }
    else Accent->StartGroundSlash(GroundPresentation.DebrisMaterial, Duration + 1.f);
}

void ASamuraiBladeWave::ReleaseGroundVisual()
{
    if (AssignedVisual.IsValid()) AssignedVisual->ReleaseGroundSlash(GroundPresentation.DebrisLifetime);
    AssignedVisual.Reset();
}

void ASamuraiBladeWave::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bGroundSlash) return;
    PhaseAge += DeltaSeconds;
    const float Travelled = FVector::DotProduct(GetActorLocation() - PhaseOrigin, PhaseDirection);
    if (Travelled >= PhaseDistance)
    {
        const FVector End = PhaseOrigin + PhaseDirection * PhaseDistance;
        SetActorLocation(FVector(End.X, End.Y, GetActorLocation().Z), false);
    }
    FollowGround();
    if (AssignedVisual.IsValid()) AssignedVisual->MoveAnchor(GroundAnchor);
    const float Delay = FMath::Max(0.f, GroundPresentation.SlowdownDelay);
    const float Rate = FMath::Max(0.f, GroundPresentation.SlowdownRate);
    const float Fit = Rate > 0.f ? PhaseDistance / (Speed * (Delay + 1.f / Rate)) : 1.f;
    const bool bCrescent = SourceUpgrades.IsValid() && SourceUpgrades->HasUpgradeId(TEXT("BladeWave"));
    const float TimeScale = GroundPresentation.bFitSlowdownToRange && Rate > 0.f
        ? (bCrescent ? FMath::Max(.01f, Fit) : FMath::Clamp(Fit, .01f, 1.f)) : 1.f;
    if (Travelled >= PhaseDistance || PhaseAge >= FMath::Max(.1f, GroundPresentation.MaxTravelTime) * FMath::Max(1.f, TimeScale)
        || Movement->Velocity.SizeSquared() <= 25.f)
    {
        if (bReturns && !bReturning) BeginReturn(); else FinishWave();
        return;
    }
    const float SlowDelta = FMath::Min(DeltaSeconds, FMath::Max(0.f, PhaseAge - Delay * TimeScale));
    if (SlowDelta > 0.f && GroundPresentation.SlowdownRate > 0.f)
        Movement->Velocity = FMath::VInterpTo(Movement->Velocity, FVector::ZeroVector, SlowDelta, Rate / TimeScale);
}

void ASamuraiBladeWave::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(Other);
	if (!Enemy || Enemy->IsDead() || HitThisPhase.Contains(Enemy) || !SourceSamurai.IsValid()) return;
	if (!Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)) return;
	HitThisPhase.Add(Enemy);
	UHealthComponent* Health = Enemy->GetHealthComponent();
	FVector ImpactLocation, ImpactNormal;
	Enemy->GetImpactContact(GetActorLocation(), ImpactLocation, ImpactNormal);
	const float HealthBeforeHit = Health ? Health->GetCurrentHealth() : 0.f;
	const bool bApplied = Enemy->ApplyPlayerDamage(Damage, EPlayerAttackSource::Samurai);
	if (bApplied && SourceUpgrades.IsValid()) SourceUpgrades->HandleSamuraiDirectHit(Enemy, Damage, HealthBeforeHit);
	if (bApplied && SourceSamurai->GetOwner())
		if (auto* Abilities = SourceSamurai->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
		{ Abilities->BladeWaveImpact(Enemy, Damage, !bSplintered); bSplintered = true; }
    if (bApplied)
    {
        if (bCanSplit && !bSplitChecked)
        {
            bSplitChecked = true;
            if (FMath::FRand() < CrescentBuild::Chance(SourceUpgrades.Get(), TEXT("CrescentSplit"), TEXT("CrescentSplitChance"), .15f, .15f))
                SpawnSplitWaves(Enemy);
        }
    }
	if (bApplied) UImpactFeedbackLibrary::PlayImpactFeedback(this, ImpactFeedback, ImpactLocation, ImpactNormal);
	UPlayerUpgradeComponent* Upgrades = SourceUpgrades.Get();
	if (bApplied && Health && !Health->IsDead() && Upgrades && Upgrades->HasUpgradeId(TEXT("BattleStance")))
		Enemy->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed, Upgrades, EPlayerAttackSource::Samurai, false, Damage);
	if (Upgrades && Upgrades->HasUpgradeId(TEXT("MarkedBlade"))) Enemy->ApplyMark();
}

void ASamuraiBladeWave::SpawnSplitWaves(AActor* FirstTarget)
{
    auto* U = SourceUpgrades.Get();
    if (!U || !SourceSamurai.IsValid()) return;
    const float DamageScale = FMath::Max(0.f, CrescentBuild::Value(U, TEXT("CrescentSplit"), TEXT("DamageMultiplier"), .5f));
    const float SizeScale = FMath::Max(.1f, CrescentBuild::Value(U, TEXT("CrescentSplit"), TEXT("SizeMultiplier"), .6f));
    const float RangeScale = FMath::Max(.1f, CrescentBuild::Value(U, TEXT("CrescentSplit"), TEXT("RangeMultiplier"), .6f));
    const float Angle = CrescentBuild::Value(U, TEXT("CrescentSplit"), TEXT("Angle"), 90.f);
    // Branch from the struck enemy, at the travelling wave's ground height.
    // The original keeps its velocity, remaining range and presentation intact.
    FVector BranchOrigin = GetActorLocation();
    if (FirstTarget)
    {
        BranchOrigin.X = FirstTarget->GetActorLocation().X;
        BranchOrigin.Y = FirstTarget->GetActorLocation().Y;
    }
    const FVector TravelDirection = Movement->Velocity.IsNearlyZero()
        ? GetActorForwardVector() : Movement->Velocity.GetSafeNormal2D();
    for (float Side : {-1.f, 1.f})
    {
        const FVector Direction = TravelDirection.RotateAngleAxis(Side * Angle, FVector::UpVector);
        FActorSpawnParameters Params; Params.Owner = SourceSamurai.Get(); Params.Instigator = SourceSamurai.Get();
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (auto* Wave = GetWorld()->SpawnActor<ASamuraiBladeWave>(GetClass(), BranchOrigin, Direction.Rotation(), Params))
        {
            Wave->bCanSplit = false;
            Wave->HitThisPhase.Add(FirstTarget);
            Wave->InitializeBladeWave(SourceSamurai.Get(), U, Direction, Damage * DamageScale, WaveWidth * SizeScale,
                TravelDistance * RangeScale, Speed, false, VisualAreaScale * SizeScale, FieldDamage * DamageScale, bFieldTriggered);
        }
    }
}
void ASamuraiBladeWave::SpawnField()
{
    if (!bFieldPending || !SourceSamurai.IsValid() || !SourceUpgrades.IsValid()) return;
    bFieldPending = false;
    FActorSpawnParameters Params; Params.Owner = SourceSamurai.Get(); Params.Instigator = SourceSamurai.Get();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // Sweep the wave's box across its completed outbound path. Use actual travel,
    // not configured range, so slow/stopped waves cannot paint ground ahead.
    const FVector End = Collision->GetComponentLocation();
    const FVector Path = End - FieldTrailStart;
    const FVector Center = (FieldTrailStart + End) * .5f;
    const FRotator Rotation = Path.SizeSquared2D() > KINDA_SMALL_NUMBER
        ? Path.GetSafeNormal2D().Rotation() : Collision->GetComponentRotation();
    FVector HalfExtent = Collision->GetScaledBoxExtent();
    HalfExtent.X += Path.Size2D() * .5f;
    HalfExtent.Z += FMath::Abs(Path.Z) * .5f;
    if (auto* Field = GetWorld()->SpawnActor<ASamuraiWaveField>(Center, Rotation, Params))
    {
        Field->Initialize(SourceSamurai.Get(), SourceUpgrades.Get(), FieldDamage, HalfExtent,
            bGroundSlash ? AssignedVisual.Get() : nullptr);
        if (bGroundSlash) AssignedVisual.Reset(); // Field now owns the deposited trail.
    }
}

void ASamuraiBladeWave::BeginReturn()
{
	if (!SourceSamurai.IsValid()) { FinishWave(); return; }
    if (bReturning) return;
    // Commit the entire outbound strip before turning. Retracing it must not
    // duplicate the field or reroll its proc, and child waves never return.
    SpawnField();
    if (bReturnToLaunch) { Damage *= ReturnDamageScale; FieldDamage *= ReturnDamageScale; }
    else if(SourceSamurai->GetOwner())if(auto* Abilities=SourceSamurai->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
  Damage*=FMath::Max(0.0f,Abilities->Tuning(4,TEXT("ReturnDamageMultiplier"),1,0));
	bReturning = true;
	bSplintered = false;
	HitThisPhase.Reset();
	FVector Direction = (bReturnToLaunch ? LaunchOrigin : SourceSamurai->GetActorLocation()) - GetActorLocation();
	Direction.Z = 0.0f;
	const float Distance = Direction.Size();
	if (!Direction.Normalize() || Distance <= KINDA_SMALL_NUMBER) { FinishWave(); return; }
	SetActorRotation(Direction.Rotation());
	Movement->Velocity = Direction * Speed;
    // Enemies still overlapping at the turnaround also receive their one return hit.
    TArray<AActor*> Overlapping;
    Collision->GetOverlappingActors(Overlapping, AEnemyBase::StaticClass());
    for (AActor* Enemy : Overlapping) HandleOverlap(Collision, Enemy, nullptr, 0, false, FHitResult());
	const float ReturnDuration = FMath::Max(0.01f, Distance / Speed);
 if (bGroundSlash)
 {
  ReleaseGroundVisual();
  PhaseOrigin = GetActorLocation();
  PhaseDirection = Direction;
  PhaseDistance = Distance;
  PhaseAge = 0.f;
  FollowGround();
  StartGroundVisual();
  OnReturnStarted.Broadcast(this);
  return;
 }
 if(AssignedVisual.IsValid())AssignedVisual->SetRemainingLifetime(ReturnDuration);
	// The player may have moved: fit the remaining animation to the actual return distance.
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : WaveEffects)
	{
		if (!Effect.IsValid()) continue;
		const auto Controller = Effect->GetSystemInstanceController();
		const float Age = Controller.IsValid() ? Controller->GetAge() : VFXAuthoredDuration * 0.5f;
		Effect->SetCustomTimeDilation(FMath::Max(0.001f, VFXAuthoredDuration - Age) / ReturnDuration);
	}
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiBladeWave::FinishWave, ReturnDuration, false);
	OnReturnStarted.Broadcast(this);
}

void ASamuraiBladeWave::FinishWave()
{
    SpawnField();
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	if (bGroundSlash) ReleaseGroundVisual();
	OnWaveFinished.Broadcast(this);
	Destroy();
}

void ASamuraiBladeWave::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
 if(AssignedVisual.IsValid())AssignedVisual->Destroy();
	Super::EndPlay(Reason);
}
