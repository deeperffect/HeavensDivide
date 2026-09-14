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
    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static TArray<UObject*> GetEmitters(UNiagaraSystem* System);

    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static bool RebuildSystem(UNiagaraSystem* System, FLinearColor PreviewColor);

    // Lightweight Niagara enums are not exposed by UE's Python wrappers.
    UFUNCTION(BlueprintCallable, Category="Editor|Swap VFX")
    static bool SetPropertyText(UObject* Object, FName Property, const FString& Value);
};
