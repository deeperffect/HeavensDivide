#pragma once
#include "CoreMinimal.h"
#include "UpgradePresentation.generated.h"
class UNiagaraSystem;
class USoundBase;
class USoundConcurrency;
class UMaterialInterface;
USTRUCT(BlueprintType)
struct FUpgradePresentation
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") bool bOverrideFamilyVisuals=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UNiagaraSystem> PulseSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UNiagaraSystem> LineSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UNiagaraSystem> WarningSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UNiagaraSystem> ImpactSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UNiagaraSystem> DetonationSystem;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") bool bShowFallbackWithNiagara=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") bool bShowFallbackWithoutNiagara=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UMaterialInterface> FallbackRingMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") TObjectPtr<UMaterialInterface> FallbackLineMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") FVector LocationOffset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX",meta=(ToolTip="Additional world-space offset for line endpoints; visual only.")) FVector EndOffset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") FRotator RotationOffset=FRotator::ZeroRotator;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") FVector Scale=FVector::OneVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") bool bScaleSystemToRadius=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX",meta=(ClampMin="1")) float AuthoredRadius=100;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") FName FallbackColorParameter=TEXT("Tint");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") FName FallbackIntensityParameter=TEXT("Intensity");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") bool bFadeFallback=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") bool bOverrideColor=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX") FLinearColor Color=FLinearColor::White;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX",meta=(ClampMin="0",ToolTip="0 uses the attack's scheduled visual duration. Does not change damage timing.")) float LifetimeOverride=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX",meta=(ClampMin="0")) float LifetimeMultiplier=1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX",meta=(ClampMin="0")) float FallbackLineThickness=3.5f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX",meta=(ClampMin="0")) float MinimumSpawnInterval=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara Parameters") FName RadiusParameter=TEXT("User.Radius");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara Parameters") FName DurationParameter=TEXT("User.Duration");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara Parameters") FName StartParameter=TEXT("User.StartPosition");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara Parameters") FName EndParameter=TEXT("User.EndPosition");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara Parameters") FName ColorParameter=TEXT("User.Color");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> Sound;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundConcurrency> SoundConcurrency;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio",meta=(ClampMin="0")) float SoundVolume=1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio",meta=(ClampMin="0.01")) float SoundPitch=1;
};
