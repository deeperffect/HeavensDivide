#include "BarrageVFXReviewCommandlet.h"
#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraScript.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeCustomHlsl.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "EdGraph/EdGraphSchema.h"
#include "UObject/UnrealType.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
namespace
{
bool Expression(UEdGraphPin& Pin,const FNiagaraTypeDefinition& Type,const FString& Code)
{
    auto* Graph=Pin.GetOwningNode()->GetGraph();
    auto* Node=NewObject<UNiagaraNodeCustomHlsl>(Graph);
    Node->CreateNewGuid();Graph->AddNode(Node,false,false);
    Node->Signature.Inputs.Reset();Node->Signature.Outputs.Reset();
    Node->Signature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("Map")));
    Node->Signature.Outputs.Add(FNiagaraVariable(Type,TEXT("CustomHLSLOutput")));
    Node->ScriptUsage=ENiagaraScriptUsage::DynamicInput;
    FindFProperty<FStrProperty>(Node->GetClass(),TEXT("CustomHlsl"))->SetPropertyValue_InContainer(Node,Code);
    static_cast<UEdGraphNode*>(Node)->AllocateDefaultPins();
    UEdGraphPin* Source=nullptr;
    for(auto* P:Pin.GetOwningNode()->Pins)if(P->Direction==EGPD_Input&&P->PinName==TEXT("Source"))Source=P;
    if(!Source||Source->LinkedTo.IsEmpty())return false;
    bool In=false,Out=false;
    for(auto* P:Node->Pins)
    {
        if(P->Direction==EGPD_Input&&P->PinName==TEXT("Map")){P->MakeLinkTo(Source->LinkedTo[0]);In=true;}
        if(P->Direction==EGPD_Output&&P->PinName==TEXT("CustomHLSLOutput")){P->MakeLinkTo(&Pin);Out=true;}
    }
    return In&&Out;
}
}
#endif
UBarrageVFXReviewCommandlet::UBarrageVFXReviewCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UBarrageVFXReviewCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    auto* S=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/VFX/MagicProjectilesVol3/Particles/NiagaraSystems/NS_Projectile_BarrageStance.NS_Projectile_BarrageStance"));
    if(!S)return 1;
    if(FParse::Param(*Params,TEXT("Fix")))
    {
        const FString Package=TEXT("/Game/HeavensDivide/VFX/Stances/NS_BarrageProjectile");
        auto* Existing=LoadObject<UNiagaraSystem>(nullptr,*(Package+TEXT(".NS_BarrageProjectile")),nullptr,LOAD_NoWarn);
        if(Existing) return Existing->IsReadyToRun()?0:2;
        S=Cast<UNiagaraSystem>(StaticDuplicateObject(S,CreatePackage(*Package),TEXT("NS_BarrageProjectile")));
        if(!S)return 2;
        const FNiagaraVariable Opacity(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.ProjectileOpacity"));
        const FNiagaraVariable Travel(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.ProjectileTravel"));
        S->GetExposedParameters().SetParameterValue(1.f,Opacity,true);
        S->GetExposedParameters().SetParameterValue(1000.f,Travel,true);
        auto* Color=LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/Modules/Update/Color/Color.Color"));
        if(!Color)return 3;
        int32 Fades=0,Beams=0;
        for(auto& H:S->GetEmitterHandles())
        {
            if(!H.GetIsEnabled())continue;
            auto* D=H.GetEmitterData();auto* Src=D?Cast<UNiagaraScriptSource>(D->GraphSource):nullptr;
            UNiagaraGraph* G=Src?Src->NodeGraph.Get():nullptr;if(!G)return 3;
            auto* Update=G->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);if(!Update)return 3;
            G->Modify();
            const auto Nodes=G->Nodes;
            for(UEdGraphNode* N:Nodes)
            {
                auto* F=Cast<UNiagaraNodeFunctionCall>(N);
                if(!F||!F->FunctionScript||F->FunctionScript->GetName()!=TEXT("BeamEmitterSetup"))continue;
                // Beams begin at the projectile tip. Their rear endpoint cannot extend past the launch point.
                for(auto* P:F->Pins) if(P->PinName==TEXT("Absolute Beam Start")||P->PinName==TEXT("Absolute Beam End"))
                    F->GetSchema()->TrySetDefaultValue(*P,TEXT("false"));
                for(const auto& Input: {TPair<FName,FString>(TEXT("Module.Beam Start"),TEXT("float3(0,0,0)")),
                    TPair<FName,FString>(TEXT("Module.Beam End"),TEXT("float3(-min(300.0, User.ProjectileTravel / max(Engine.Owner.Scale.x, 0.01)),0,0)"))})
                {
                    auto Alias=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(Input.Key,F);
                    auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*F,Alias,FNiagaraTypeDefinition::GetVec3Def(),FGuid(),FGuid());
                    Pin.BreakAllPinLinks();
                    if(!Expression(Pin,FNiagaraTypeDefinition::GetVec3Def(),Input.Value))return 3;
                }
                ++Beams;
            }
            auto* Fade=FNiagaraStackGraphUtilities::AddScriptModuleToStack(Color,*Update);
            if(!Fade)return 3;
            auto Alias=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FName(TEXT("Module.Color")),Fade);
            auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Fade,Alias,FNiagaraTypeDefinition::GetColorDef(),FGuid(),FGuid());
            Pin.BreakAllPinLinks();
            if(!Expression(Pin,FNiagaraTypeDefinition::GetColorDef(),TEXT("float4(Particles.Color.rgb, Particles.Color.a * User.ProjectileOpacity)")))return 3;
            ++Fades;D->GraphSource->MarkNotSynchronized(TEXT("Projectile travel-clamped beams and impact fade"));
        }
        if(Fades==0||Beams==0)return 4;
        S->RequestCompile(true);S->WaitForCompilationComplete(true,false);
        if(!S->IsReadyToRun())return 5;
        const FString File=FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        UE_LOG(LogTemp,Display,TEXT("BARRAGE_VFX_FIXED: %d fading emitters, %d travel-clamped beams"),Fades,Beams);
        return UPackage::SavePackage(S->GetOutermost(),S,*File,Args)?0:6;
    }
    FString Out;
    for(auto& H:S->GetEmitterHandles())
    {
        auto* D=H.GetEmitterData(); auto* Src=D?Cast<UNiagaraScriptSource>(D->GraphSource):nullptr;
        Out+=FString::Printf(TEXT("EMITTER %s local=%d\n"),*H.GetName().ToString(),D?D->bLocalSpace:0);
        if(!Src||!Src->NodeGraph)continue;
        for(UEdGraphNode* N:Src->NodeGraph->Nodes)
        {
            Out+=FString::Printf(TEXT("NODE %s %s\n"),*N->GetClass()->GetName(),*N->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            for(auto* P:N->Pins) if(P->Direction==EGPD_Input)
                Out+=FString::Printf(TEXT("  %s = %s links=%d\n"),*P->PinName.ToString(),*P->DefaultValue,P->LinkedTo.Num());
        }
    }
    FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectSavedDir()/TEXT("SteamBarrage/NiagaraGraph.txt")));
    return 0;
#else
    return 1;
#endif
}
