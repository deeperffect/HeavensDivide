#include "SwapVFXSetupLibrary.h"
#include "NiagaraSystem.h"
#if WITH_EDITOR
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterBase.h"
#include "Stateless/NiagaraStatelessDistribution.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#endif

TArray<UObject*> USwapVFXSetupLibrary::GetEmitters(UNiagaraSystem* System)
{
    TArray<UObject*> Result;
#if WITH_EDITOR
    if (System)
        for (const auto& Handle : System->GetEmitterHandles())
            if (auto* Emitter = Handle.GetEmitterBase()) Result.Add(Emitter);
#endif
    return Result;
}

bool USwapVFXSetupLibrary::SetPropertyText(UObject* Object, FName Property, const FString& Value)
{
#if WITH_EDITOR
    if (Object)
        if (FProperty* Field = Object->GetClass()->FindPropertyByName(Property))
        {
            Object->Modify();
            if (!Field->ImportText_Direct(*Value, Field->ContainerPtrToValuePtr<void>(Object), Object, PPF_None)) return false;
            FPropertyChangedEvent Event(Field);
            Object->PostEditChangeProperty(Event);
            return true;
        }
#endif
    return false;
}

bool USwapVFXSetupLibrary::RebuildSystem(UNiagaraSystem* System, FLinearColor PreviewColor)
{
#if WITH_EDITOR
    if (System)
    {
        System->Modify();
        const FNiagaraVariable Color(FNiagaraTypeDefinition::GetColorDef(), TEXT("User.SwapColor"));
        System->GetExposedParameters().SetParameterValue(PreviewColor, Color, true);
        // Bind the initial color so the existing swap component can tint the effect.
        ForEachObjectWithOuter(System, [&Color](UObject* Object)
        {
            if (auto* Field = FindFProperty<FStructProperty>(Object->GetClass(), TEXT("ColorDistribution")))
                if (Field->Struct == FNiagaraDistributionColor::StaticStruct())
                {
                    auto* Distribution = Field->ContainerPtrToValuePtr<FNiagaraDistributionColor>(Object);
                    Distribution->Mode = ENiagaraDistributionMode::Binding;
                    Distribution->ParameterBinding = Color;
                }
        }, EGetObjectsFlags::IncludeNestedObjects);
        System->PostEditChange();
        System->RequestCompile(true);
        System->WaitForCompilationComplete(true, false);
        // IsReadyToRun is always false with -nullrhi; validate compilation instead.
        return System->IsValid() && !System->HasOutstandingCompilationRequests(true);
    }
#endif
    return false;
}
