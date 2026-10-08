#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SwapVFXSetupLibrary.generated.h"

class UNiagaraSystem;

/** Editor scripting access used by Tools/create_swap_vfx.py. */
UCLASS()
class USwapVFXSetupLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Bind full-circle material variants on the Fang copy of Slash 15. */
    UFUNCTION(BlueprintCallable, Category="Editor|Upgrade VFX")
    static bool ConfigureFangCircleSlash(UNiagaraSystem* System);
    /** Configure duplicated vendor effects for stance procs; never pass the source asset. */
    UFUNCTION(BlueprintCallable, Category="Editor|Upgrade VFX")
    static bool ConfigureStanceEffect(UNiagaraSystem* System, FLinearColor Color, bool bPoisonPool);
    UFUNCTION(BlueprintCallable, Category="Editor|Upgrade VFX")
    static bool TintSmokeSystem(UNiagaraSystem* System, FLinearColor Color);
    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static TArray<UObject*> GetEmitters(UNiagaraSystem* System);

    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static bool RebuildSystem(UNiagaraSystem* System, FLinearColor PreviewColor);

    // Lightweight Niagara enums are not exposed by UE's Python wrappers.
    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static bool SetPropertyText(UObject* Object, FName Property, const FString& Value);
    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static bool IsMaterialInputConnected(UObject* Expression, FName InputName);
    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static int32 PreparePortalLocalSpace(UNiagaraSystem* System);

    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static bool PreparePickupBurstScale(UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable, Category="Editor|Ground Slash")
    static int32 BindGroundSlashDebris(UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable, Category="Editor|Ground Slash")
    static int32 BindGroundSlashTrailOrientation(UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable, Category="Editor|Ground Slash")
    static bool CompleteGroundSlashEffects(UNiagaraSystem* System, UNiagaraSystem* Original);
};
