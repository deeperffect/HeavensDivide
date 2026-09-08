#include "EnemyDeathComponent.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UEnemyDeathSoundConcurrency::UEnemyDeathSoundConcurrency()
{
	Concurrency.MaxCount = 16;
	Concurrency.bLimitToOwner = false;
	Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	Concurrency.VoiceStealReleaseTime = 0.02f;
}

UEnemyDeathComponent::UEnemyDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Burst(TEXT("/Game/Assets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_Hit_Basic_Once"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/Assets/Sounds/Ninja/MS_Ninja_Impact"));
	DeathNiagaraSystem = Burst.Object;
	DeathSound = Sound.Object;
}

void UEnemyDeathComponent::StartDeathPresentation(FSimpleDelegate OnComplete)
{
	if (bStarted || !IsValid(GetOwner())) return;
	bStarted = true;
	Completion = MoveTemp(OnComplete);
	AActor* Owner = GetOwner();
	// No presentation cost on a dedicated server; gameplay has already run.
	if (GetNetMode() == NM_DedicatedServer)
	{
		Finish();
		return;
	}
	if (DeathNiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathNiagaraSystem,
			Owner->GetActorLocation() + NiagaraSpawnOffset, Owner->GetActorRotation(), FVector::OneVector,
			true, true, ENCPoolMethod::AutoRelease);
	}
	if (DeathSound)
	{
		USoundConcurrency* Concurrency = DeathSoundConcurrency ? DeathSoundConcurrency.Get() : GetMutableDefault<UEnemyDeathSoundConcurrency>();
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, Owner->GetActorLocation(), 1.0f, 1.0f, 0.0f, nullptr, Concurrency);
	}

	TInlineComponentArray<UMeshComponent*> Meshes(Owner);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh->IsVisible() || Mesh->bHiddenInGame || Mesh->IsA<UWidgetComponent>()) continue;
		if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh))
		{
			SkeletalMesh->bPauseAnims = true;
		}
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Material = Mesh->GetMaterial(Slot);
			float ExistingValue;
			if (!Material || !Material->GetScalarParameterValue(FMaterialParameterInfo(DissolveParameterName), ExistingValue)) continue;
			// Reuse this mesh's MID, never modify a shared material asset.
			if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(Slot))
			{
				DissolveMaterials.AddUnique(MID);
			}
		}
		// Overlay passes must dissolve too, otherwise they leave an opaque silhouette.
		if (UMaterialInterface* Overlay = Mesh->GetOverlayMaterial())
		{
			float ExistingValue;
			if (Overlay->GetScalarParameterValue(FMaterialParameterInfo(DissolveParameterName), ExistingValue))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Overlay, this);
				Mesh->SetOverlayMaterial(MID);
				DissolveMaterials.Add(MID);
			}
			else Mesh->SetOverlayMaterial(nullptr);
		}
	}
	ApplyDissolve(0.0f);
	if (!FMath::IsFinite(DissolveDuration) || DissolveDuration <= 0.0f) Finish();
	else SetComponentTickEnabled(true);
}

void UEnemyDeathComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bStarted || bFinished) return;
	Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(DissolveDuration, UE_SMALL_NUMBER), 0.0f, 1.0f);
	ApplyDissolve(Alpha);
	if (Alpha >= 1.0f) Finish();
}

void UEnemyDeathComponent::ApplyDissolve(float Alpha)
{
	for (UMaterialInstanceDynamic* Material : DissolveMaterials)
	{
		Material->SetScalarParameterValue(DissolveParameterName, FMath::Lerp(DissolveStartValue, DissolveEndValue, Alpha));
	}
}

void UEnemyDeathComponent::Finish()
{
	if (bFinished) return;
	bFinished = true;
	SetComponentTickEnabled(false);
	ApplyDissolve(1.0f);
	FSimpleDelegate Callback = MoveTemp(Completion);
	Callback.ExecuteIfBound();
}

void UEnemyDeathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);
	Completion.Unbind();
	DissolveMaterials.Reset();
	Super::EndPlay(EndPlayReason);
}
