#include "SwapVFXSetupLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraDataInterfaceCurve.h"
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
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraDecalRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

bool USwapVFXSetupLibrary::ConfigureFangCircleSlash(UNiagaraSystem* System)
{
#if WITH_EDITOR
    if (!System || System->GetPathName() != TEXT("/Game/HeavensDivide/VFX/Stances/NS_FangSlash_360.NS_FangSlash_360")) return false;
    System->Modify();
    int32 Count = 0;
    for (auto& Handle : System->GetEmitterHandles())
    {
        if (!Handle.GetIsEnabled()) continue;
        auto* Data = Handle.GetEmitterData();
        if (!Data) continue;
        for (auto* Renderer : Data->GetRenderers())
            if (auto* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
                for (auto& Override : Mesh->OverrideMaterials)
                {
                    auto* Old = Override.ExplicitMat.Get();
                    if (!Old) continue;
                    FString Name = Old->GetName();
                    if (Name.StartsWith(TEXT("MI_Fang360_"))) { ++Count; continue; }
                    if (Name != TEXT("MI_MasterStrip_ADD5") && Name != TEXT("MI_MasterStrip_AB")) continue;
                    auto* Material = LoadObject<UMaterialInterface>(nullptr, *(TEXT("/Game/HeavensDivide/VFX/Stances/MI_Fang360_") + Name));
                    if (!Material) return false;
                    Mesh->Modify();Override.ExplicitMat = Material;Mesh->PostEditChange();++Count;
                }
    }
    if (Count != 2) return false;
    System->PostEditChange();System->RequestCompile(true);System->WaitForCompilationComplete(true,false);
    return System->IsValid();
#else
    return false;
#endif
}

bool USwapVFXSetupLibrary::TintSmokeSystem(UNiagaraSystem* System,FLinearColor Color)
{
#if WITH_EDITOR
 if(!System)return false;
 const FNiagaraVariable Var(FNiagaraTypeDefinition(UNiagaraDataInterfaceColorCurve::StaticClass()),TEXT("User.User_SmokeGradient"));
 auto* Curve=Cast<UNiagaraDataInterfaceColorCurve>(System->GetExposedParameters().GetDataInterface(Var));
 if(!Curve)return false;
 System->Modify();Curve->Modify();Curve->CurveAsset=nullptr;
 FRichCurve* Channels[]={&Curve->RedCurve,&Curve->GreenCurve,&Curve->BlueCurve};
 const float Values[]={Color.R,Color.G,Color.B};
 for(int32 I=0;I<3;++I){Channels[I]->Reset();Channels[I]->AddKey(0,Values[I]*.12f);Channels[I]->AddKey(.45f,Values[I]);Channels[I]->AddKey(1,Values[I]*.3f);}
 Curve->UpdateLUT();Curve->PostEditChange();System->PostEditChange();System->RequestCompile(true);System->WaitForCompilationComplete(true,false);
 return System->IsValid();
#else
 return false;
#endif
}

bool USwapVFXSetupLibrary::CompleteGroundSlashEffects(UNiagaraSystem* System, UNiagaraSystem* Original)
{
#if WITH_EDITOR
    if (!System || !Original || System == Original) return false;
    System->Modify();
    // Copy both halves of each event-driven trail. Copying only the visible
    // emitter leaves it waiting for events from a different system.
    TMap<FGuid, FGuid> RemappedIds;
    TSet<FGuid> AddedIds;
    for (const auto& Source : Original->GetEmitterHandles())
    {
        const FString Name = Source.GetName().ToString();
        if (!Name.StartsWith(TEXT("Trail")) && Name != TEXT("StarParticles") && Name != TEXT("StarParticles002")) continue;
        const auto* Existing = System->GetEmitterHandles().FindByPredicate(
            [&Source](const FNiagaraEmitterHandle& Handle) { return Handle.GetName() == Source.GetName(); });
        if (Existing) RemappedIds.Add(Source.GetId(), Existing->GetId());
        else
        {
            const FGuid NewId = FNiagaraEditorUtilities::AddEmitterToSystem(
                *System, *Source.GetInstance().Emitter, Source.GetInstance().Version);
            auto& Copy = *System->GetEmitterHandles().FindByPredicate(
                [&NewId](const FNiagaraEmitterHandle& Handle) { return Handle.GetId() == NewId; });
            Copy.SetName(Source.GetName(), *System);
            Copy.GetEmitterData()->RemoveParent();
            Copy.SetIsEnabled(Source.GetIsEnabled(), *System, false);
            RemappedIds.Add(Source.GetId(), NewId);
            AddedIds.Add(NewId);
        }
    }
    if (RemappedIds.Num() != 10) return false;
    int32 SlashMeshes = 0;
    for (auto& Handle : System->GetEmitterHandles())
    {
        auto* Data = Handle.GetEmitterData();
        if (!Data) continue;
        if (AddedIds.Contains(Handle.GetId()))
        {
            for (const auto& Event : Data->GetEventHandlers())
            {
                const auto* NewId = RemappedIds.Find(Event.SourceEmitterID);
                if (!NewId) return false;
                Data->GetEventHandlerByIdUnsafe(Event.Script->GetUsageId())->SourceEmitterID = *NewId;
            }
        }
        if (Handle.GetName() == TEXT("SlashMeshTUT"))
        {
            for (auto* Renderer : Data->GetRenderers())
                if (auto* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
                {
                    Mesh->Modify();
                    // The source crescent is upright in X/Z. Keep the correction
                    // on its mesh renderer so ground trails retain their orientation.
                    for (auto& Entry : Mesh->Meshes)
                    {
                        Entry.Rotation = FRotator::ZeroRotator;
                        if (!Entry.Mesh) return false;
                        // Center across the trail and put the bottom, rather than
                        // the middle, at ground height. Mesh-space offsets follow
                        // particle rotation and scale, including split/return waves.
                        const auto Bounds = Entry.Mesh->GetBounds();
                        const FVector Center = Bounds.Origin * Entry.Scale;
                        const float Bottom = Center.Z - Bounds.BoxExtent.Z * FMath::Abs(Entry.Scale.Z);
                        Entry.PivotOffsetSpace = ENiagaraMeshPivotOffsetSpace::Mesh;
                        Entry.PivotOffset = FVector(0.f, -Center.Y, -Bottom);
                        UE_LOG(LogTemp, Display, TEXT("GroundSlash upright mesh: bounds=%s pivot=%s"),
                            *Center.ToString(), *Entry.PivotOffset.ToString());
                    }
                    Mesh->PostEditChange();
                    ++SlashMeshes;
                }
        }
    }
    if (SlashMeshes != 1) return false;
    System->PostEditChange();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    return System->IsReadyToRun();
#else
    return false;
#endif
}

int32 USwapVFXSetupLibrary::BindGroundSlashTrailOrientation(UNiagaraSystem* System)
{
    int32 Count = 0;
#if WITH_EDITOR
    if (!System) return 0;
    System->Modify();
    const FNiagaraVariable Parameter(FNiagaraTypeDefinition::GetQuatDef(), TEXT("User.GroundTrailOrientation"));
    System->GetExposedParameters().SetParameterValue(UNiagaraDecalRendererProperties::GetDefaultOrientation(), Parameter, true);
    for (const auto& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetName() != TEXT("DecalBrightTUT")) continue;
        if (auto* Data = Handle.GetEmitterData())
        {
            // Local simulation would drag already-spawned marks along with the wave.
            if (Data->bLocalSpace) return -2;
            for (auto* Renderer : Data->GetRenderers())
                if (auto* Decal = Cast<UNiagaraDecalRendererProperties>(Renderer))
                {
                    Decal->Modify();
                    Decal->DecalOrientationBinding.SetValue(Parameter.GetName(), Handle.GetInstance().ToBase(), Decal->SourceMode);
                    Decal->PostEditChange();
                    ++Count;
                }
        }
    }
    System->PostEditChange();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    if (!System->IsReadyToRun()) return -1;
#endif
    return Count;
}

int32 USwapVFXSetupLibrary::BindGroundSlashDebris(UNiagaraSystem* System)
{
    int32 Count = 0;
#if WITH_EDITOR
    if (!System) return 0;
    System->Modify();
    const FNiagaraVariable Parameter(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()), TEXT("User.DebrisMaterial"));
    System->GetExposedParameters().AddParameter(Parameter);
    const FNiagaraVariable LifetimeParameter(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.GroundDebrisLifetime"));
    if (!System->GetExposedParameters().FindParameterVariable(LifetimeParameter))
        System->GetExposedParameters().SetParameterValue(5.f, LifetimeParameter, true);
    int32 LifetimeBindings = 0;
    for (const auto& Handle : System->GetEmitterHandles())
    {
        if (!Handle.GetName().ToString().StartsWith(TEXT("Debris"))) continue;
        if (auto* Data = Handle.GetEmitterData())
        {
            if (Handle.GetName().ToString().StartsWith(TEXT("DebrisGround")))
            {
                if (Data->bLocalSpace) return -2;
                auto* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
                auto* Graph = Source ? Source->NodeGraph.Get() : nullptr;
                if (!Graph) return -2;
                // Only the stationary ground rocks persist. Air debris retains
                // its authored gravity, lifetime and scale animation.
                for (const auto& Node : Graph->Nodes)
                    if (auto* Module = Cast<UNiagaraNodeFunctionCall>(Node);
                        Module && Module->FunctionScript && Module->FunctionScript->GetFName() == TEXT("InitializeParticle"))
                    {
                        Handle.GetEmitterBase()->Modify();
                        Graph->Modify();
                        auto& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Module,
                            FNiagaraParameterHandle(FName(*(Module->GetFunctionName() + TEXT(".Lifetime")))),
                            FNiagaraTypeDefinition::GetFloatDef(), FGuid(), FGuid());
                        if (Pin.LinkedTo.IsEmpty() || Pin.LinkedTo[0]->PinName != LifetimeParameter.GetName())
                        {
                            Pin.BreakAllPinLinks();
                            FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin, LifetimeParameter, {});
                        }
                        ++LifetimeBindings;
                        break; // Adding a parameter node invalidates the Nodes iterator.
                    }
            }
            for (auto* Renderer : Data->GetRenderers())
                if (auto* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
                {
                    Mesh->Modify();
                    Mesh->bOverrideMaterials = true;
                    if (Mesh->OverrideMaterials.IsEmpty()) Mesh->OverrideMaterials.AddDefaulted();
                    for (auto& Override : Mesh->OverrideMaterials) Override.UserParamBinding.Parameter = Parameter;
                    Mesh->PostEditChange();
                    ++Count;
                }
        }
    }
    if (LifetimeBindings != 2) return -2;
    System->PostEditChange();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    if (!System->IsReadyToRun()) return -1;
#endif
    return Count;
}

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


bool USwapVFXSetupLibrary::ConfigureStanceEffect(UNiagaraSystem* System, FLinearColor Color, bool bPoisonPool)
{
#if WITH_EDITOR
 if (!System || !System->GetPathName().StartsWith(TEXT("/Game/HeavensDivide/VFX/Stances/"))) return false;
 System->Modify();
 auto& Parameters=System->GetExposedParameters();
 auto Float=[&](const TCHAR* Name,float Value) {
  const FNiagaraVariable Variable(FNiagaraTypeDefinition::GetFloatDef(),FName(Name));
  if(Parameters.IndexOf(Variable)!=INDEX_NONE) Parameters.SetParameterValue<float>(Value,Variable);
 };
 for(const TCHAR* Name:{TEXT("User.Color 1"),TEXT("User.Color 2"),TEXT("User.Color 01"),TEXT("User.color 02"),TEXT("User.ColorSlash"),TEXT("User.ColorSparks"),TEXT("User.ColorGroundMark")})
 {
  const FNiagaraVariable Variable(FNiagaraTypeDefinition::GetColorDef(),FName(Name));
  if(Parameters.IndexOf(Variable)!=INDEX_NONE) Parameters.SetParameterValue<FLinearColor>(Color,Variable);
 }
 if(!bPoisonPool)
  for(auto& Handle:System->GetEmitterHandles())
   if(Handle.GetName().ToString().Contains(TEXT("GroundMark")) || Handle.GetName()==TEXT("Debris"))
    Handle.SetIsEnabled(false,*System,false);
 if(bPoisonPool)
 {
  Float(TEXT("User._Scale"),1.f);
  Float(TEXT("User.User_SmokeRadius"),70.f);
  Float(TEXT("User.User_SmokeRate"),24.f);
  Float(TEXT("User.User_SmokeScale_Min"),.3f);
  Float(TEXT("User.User_SmokeScale_Max"),.6f);
  const FNiagaraVariable Velocity(FNiagaraTypeDefinition(UNiagaraDataInterfaceCurve::StaticClass()),TEXT("User.User_VelocityStrenght"));
  if(auto* Curve=Cast<UNiagaraDataInterfaceCurve>(Parameters.GetDataInterface(Velocity)))
  {
   Curve->Modify();Curve->CurveAsset=nullptr;Curve->Curve.Reset();Curve->Curve.AddKey(0,0);Curve->Curve.AddKey(1,0);
   Curve->UpdateLUT();Curve->PostEditChange();
  }
  Float(TEXT("User.User_SmokeLifetime_Min"),.8f);
  Float(TEXT("User.User_SmokeLifetime_Max"),1.4f);
  const FNiagaraVariable Wind(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.User_Wind"));
  if(Parameters.IndexOf(Wind)!=INDEX_NONE) Parameters.SetParameterValue<FVector3f>(FVector3f::ZeroVector,Wind);
  if(!TintSmokeSystem(System,Color))return false;
  PreparePortalLocalSpace(System);
  // Niagara mesh orientation can preserve tall geometry despite component Z scale.
  // Flatten vertices in world Z after particle rotation, using a project-owned material.
  auto* GroundMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/VFX/Stances/MI_PoisonGround.MI_PoisonGround"));
  if(!GroundMaterial)return false;
  for(auto& Handle:System->GetEmitterHandles())if(auto* Data=Handle.GetEmitterData())
   for(auto* Renderer:Data->GetRenderers())if(auto* Mesh=Cast<UNiagaraMeshRendererProperties>(Renderer)) {
    Mesh->Modify();Mesh->bOverrideMaterials=true;
    if(Mesh->OverrideMaterials.IsEmpty())Mesh->OverrideMaterials.AddDefaulted();
    for(auto& Override:Mesh->OverrideMaterials)Override.ExplicitMat=GroundMaterial;
    Mesh->PostEditChange();
   }
 }
 System->PostEditChange();System->RequestCompile(true);System->WaitForCompilationComplete(true,false);
 return System->IsValid();
#else
 return false;
#endif
}
