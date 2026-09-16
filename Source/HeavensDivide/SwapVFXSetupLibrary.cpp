#include "SwapVFXSetupLibrary.h"
#include "NiagaraSystem.h"
#if WITH_EDITOR
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterBase.h"
#include "Stateless/NiagaraStatelessDistribution.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "Materials/MaterialExpression.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraSpriteRendererProperties.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

bool USwapVFXSetupLibrary::IsMaterialInputConnected(UObject* Object, FName InputName)
{
#if WITH_EDITOR
    if (auto* Expression = Cast<UMaterialExpression>(Object))
    {
        for (FExpressionInputIterator It{Expression}; It; ++It)
            if (Expression->GetInputName(It.Index) == InputName)
                return It->Expression != nullptr;
    }
#endif
    return true; // Unknown inputs must never overwrite an authored connection.
}

int32 USwapVFXSetupLibrary::PreparePortalLocalSpace(UNiagaraSystem* System)
{
    int32 Changed = 0;
#if WITH_EDITOR
    if (System)
    {
        for (const auto& Handle : System->GetEmitterHandles())
            if (auto* Data = Handle.GetEmitterData())
                if (!Data->bLocalSpace)
                {
                    System->Modify();
                    Handle.GetEmitterBase()->Modify();
                    Data->bLocalSpace = true;
                    Handle.GetEmitterBase()->PostEditChange();
                    ++Changed;
                }
        if (Changed > 0)
        {
            System->PostEditChange();
            System->RequestCompile(true);
            System->WaitForCompilationComplete(true, false);
        }
    }
#endif
    return Changed;
}

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

bool USwapVFXSetupLibrary::PreparePickupBurstScale(UNiagaraSystem* System)
{
#if WITH_EDITOR
    UNiagaraScript* ScaleModule = LoadObject<UNiagaraScript>(nullptr,
        TEXT("/Niagara/Modules/Update/Utility/ApplyOwnerScaleToAttributes.ApplyOwnerScaleToAttributes"));
    if (!System || !ScaleModule) return false;
    System->Modify();
    for (const auto& Handle : System->GetEmitterHandles())
    {
        auto* Data = Handle.GetEmitterData();
        auto* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
        UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
        UNiagaraNodeOutput* Output = nullptr;
        if (Graph)
            for (const auto& Node : Graph->Nodes)
                if (auto* Candidate = Cast<UNiagaraNodeOutput>(Node))
                    if (Candidate->GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript) Output = Candidate;
        if (!Output) return false;
        TArray<UNiagaraNodeFunctionCall*> Modules;
        // Follow the update stack's parameter-map links; several graph query
        // helpers are private to NiagaraEditor and cannot be linked by games.
        UEdGraphNode* StackNode = Output;
        while (StackNode)
        {
            UEdGraphNode* Previous = nullptr;
            for (auto* Pin : StackNode->Pins)
                if (Pin->Direction == EGPD_Input && Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
                    if (!Pin->LinkedTo.IsEmpty()) Previous = Pin->LinkedTo[0]->GetOwningNode();
            if (auto* Module = Cast<UNiagaraNodeFunctionCall>(Previous)) Modules.Add(Module);
            StackNode = Previous;
        }
        bool bHasScale = false;
        UNiagaraNodeFunctionCall* ScaleNode = nullptr;
        for (auto* Module : Modules)
        {
            UE_LOG(LogTemp, Display, TEXT("HEAL_SCALE %s update: %s"),
                *Handle.GetName().ToString(), *GetPathNameSafe(Module->FunctionScript));
            bHasScale |= Module->FunctionScript == ScaleModule;
            if (Module->FunctionScript == ScaleModule) ScaleNode = Module;
        }
        if (!bHasScale)
        {
            Handle.GetEmitterBase()->Modify();
            Graph->Modify();
            ScaleNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ScaleModule, *Output);
            if (!ScaleNode) return false;
            Graph->NotifyGraphChanged();
        }
        // Explicitly bind the scale input and enable sprite scaling; the engine
        // module's attribute switches default to disabled when added in code.
        TSet<FName> ScaleInputs;
        if (auto* ModuleGraph = ScaleNode->GetCalledGraph())
            for (const auto& Node : ModuleGraph->Nodes)
                for (auto* Pin : Node->Pins)
                {
                    const FString Name = Pin->PinName.ToString();
                    if (Name == TEXT("Module.Scale Factor") || Name == TEXT("Module.Owner Scale"))
                        ScaleInputs.Add(Pin->PinName);
                }
        if (ScaleInputs.IsEmpty()) return false;
        for (auto* Pin : ScaleNode->Pins)
            if (Pin->Direction == EGPD_Input)
            {
                if (Pin->PinName == TEXT("Scale Initial Sprite Size")) Pin->DefaultValue = TEXT("true");
            }
        for (const FName Input : ScaleInputs)
        {
            FString InputName = Input.ToString();
            InputName.RemoveFromStart(TEXT("Module."));
            auto& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*ScaleNode,
                FNiagaraParameterHandle(FName(*(ScaleNode->GetFunctionName() + TEXT(".") + InputName))),
                FNiagaraTypeDefinition::GetVec3Def(), FGuid(), FGuid());
            if (Pin.LinkedTo.IsEmpty())
                FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,
                    FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Engine.Owner.Scale")), {});
            UE_LOG(LogTemp, Display, TEXT("HEAL_SCALE bound %s to Engine.Owner.Scale"), *InputName);
        }
        // Owner scale accumulates transient factors consumed by the size/force
        // modules later in the stack; it must run before those consumers.
        auto MapPin = [](UEdGraphNode* Node, EEdGraphPinDirection Direction) -> UEdGraphPin*
        {
            for (auto* Pin : Node->Pins)
                if (Pin->Direction == Direction && Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
                    return Pin;
            return nullptr;
        };
        auto* GroupInput = MapPin(ScaleNode, EGPD_Input);
        if (!GroupInput || GroupInput->LinkedTo.IsEmpty()) return false;
        auto* Overrides = GroupInput->LinkedTo[0]->GetOwningNode();
        if (Overrides->GetClass()->GetFName() == TEXT("NiagaraNodeParameterMapSet"))
            GroupInput = MapPin(Overrides, EGPD_Input);
        auto* GroupOutput = MapPin(ScaleNode, EGPD_Output);
        if (!GroupInput || GroupInput->LinkedTo.IsEmpty() || !GroupOutput) return false;
        auto* PreviousOutput = GroupInput->LinkedTo[0];
        const auto NextInputs = GroupOutput->LinkedTo;
        GroupInput->BreakAllPinLinks();
        GroupOutput->BreakAllPinLinks();
        for (auto* NextInput : NextInputs) PreviousOutput->MakeLinkTo(NextInput);
        UEdGraphPin* FirstInput = MapPin(Output, EGPD_Input);
        while (FirstInput && !FirstInput->LinkedTo.IsEmpty())
        {
            auto* PreviousNode = FirstInput->LinkedTo[0]->GetOwningNode();
            if (Cast<UNiagaraNodeInput>(PreviousNode)) break;
            FirstInput = MapPin(PreviousNode, EGPD_Input);
        }
        if (!FirstInput || FirstInput->LinkedTo.IsEmpty()) return false;
        auto* RootOutput = FirstInput->LinkedTo[0];
        FirstInput->BreakAllPinLinks();
        RootOutput->MakeLinkTo(GroupInput);
        GroupOutput->MakeLinkTo(FirstInput);
        // Move the linked input's parameter-map read with its override node.
        // Leaving it connected downstream creates a cycle in the graph.
        if (Overrides->GetClass()->GetFName() == TEXT("NiagaraNodeParameterMapSet"))
            for (auto* Pin : Overrides->Pins)
                if (Pin->Direction == EGPD_Input && Pin != GroupInput)
                    for (auto* Linked : Pin->LinkedTo)
                        if (auto* ReadInput = MapPin(Linked->GetOwningNode(), EGPD_Input))
                        {
                            ReadInput->BreakAllPinLinks();
                            RootOutput->MakeLinkTo(ReadInput);
                        }

        bool bHasSpriteRenderer = false;
        for (auto* Renderer : Data->GetRenderers()) bHasSpriteRenderer |= Renderer->IsA<UNiagaraSpriteRendererProperties>();
        if (bHasSpriteRenderer)
        {
            auto* SpriteScale = LoadObject<UNiagaraScript>(nullptr,
                TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize.ScaleSpriteSize"));
            bool bHasSpriteScale = false;
            UNiagaraNodeFunctionCall* SpriteScaleNode = nullptr;
            for (auto* Module : Modules)
                if (Module->FunctionScript == SpriteScale) { bHasSpriteScale = true; SpriteScaleNode = Module; }
            if (!bHasSpriteScale)
            {
                SpriteScaleNode = SpriteScale ? FNiagaraStackGraphUtilities::AddScriptModuleToStack(SpriteScale, *Output) : nullptr;
                if (!SpriteScaleNode) return false;
            }
            // These two emitters in NS_HealPickup originally had constant size.
            // Keep their new sizing steps neutral instead of adding a curve.
            const bool bOriginalConstantEmitter = System->GetName() == TEXT("NS_HealPickup")
                && (Handle.GetName() == TEXT("Fountain002") || Handle.GetName() == TEXT("Fountain008"));
            if (!bHasSpriteScale || bOriginalConstantEmitter)
            {
                for (auto* Pin : SpriteScaleNode->Pins)
                    if (Pin->Direction == EGPD_Input)
                        if (auto* Enum = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
                            if (Enum->GetName() == TEXT("ENiagara_ScaleSpriteSize"))
                                for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
                                    if (Enum->GetDisplayNameTextByIndex(Index).ToString() == TEXT("Uniform"))
                                        Pin->DefaultValue = Enum->GetNameStringByIndex(Index);
                auto& Factor = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*SpriteScaleNode,
                    FNiagaraParameterHandle(FName(*(SpriteScaleNode->GetFunctionName() + TEXT(".Uniform Scale Factor")))),
                    FNiagaraTypeDefinition::GetFloatDef(), FGuid(), FGuid());
                Factor.DefaultValue = TEXT("1.0");
            }
        }
        Graph->NotifyGraphChanged();
    }
    System->PostEditChange();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    return System->IsValid() && !System->HasOutstandingCompilationRequests(true);
#else
    return false;
#endif
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
