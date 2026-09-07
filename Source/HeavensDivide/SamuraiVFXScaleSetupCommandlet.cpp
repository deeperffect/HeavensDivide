#include "SamuraiVFXScaleSetupCommandlet.h"

#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "EdGraph/EdGraphSchema.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#endif

USamuraiVFXScaleSetupCommandlet::USamuraiVFXScaleSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 USamuraiVFXScaleSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bBasic = FParse::Param(*Params, TEXT("Basic"));
	const FString PackageName = bBasic
		? TEXT("/Game/Assets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_SlashTrail_Basic")
		: TEXT("/Game/Assets/VFX/SlashTrail_SoftTofu/Niagara/Lightning/NS_SlashTrail_Lightning");
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)));
	if (!System) return 1;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/SamuraiSecondaryScale") / (FPackageName::GetShortName(PackageName) + TEXT(".uasset"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
	if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *Filename) != COPY_OK) return 2;

	const FNiagaraVariable Baseline(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.SecondaryEffectScale"));
	System->Modify();
	if (!bBasic) System->GetExposedParameters().SetParameterValue(FVector3f::OneVector, Baseline, true);
	TSet<FNiagaraVariableBase> KnownParameters;
	KnownParameters.Add(Baseline);
	int32 Changed = 0;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (bBasic ? Handle.GetName() != TEXT("Empty") : (Handle.GetName() != TEXT("Lightning") && Handle.GetName() != TEXT("Spark"))) continue;
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
		if (!Source || !Source->NodeGraph) return 3;
		UNiagaraGraph* Graph = Source->NodeGraph;
		Graph->Modify();
		if (bBasic)
		{
			UNiagaraNodeOutput* Update = Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
			UNiagaraScript* ScaleScript = LoadObject<UNiagaraScript>(nullptr, TEXT("/Niagara/Modules/Update/Utility/ApplyOwnerScaleToAttributes.ApplyOwnerScaleToAttributes"));
			if (!Update || !ScaleScript) return 3;
			UNiagaraNodeFunctionCall* ScaleModule = nullptr;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Node);
				if (Function && Function->FunctionScript == ScaleScript) { ScaleModule = Function; break; }
			}
			if (!ScaleModule) ScaleModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ScaleScript, *Update);
			if (!ScaleModule) return 3;
			ScaleModule->Modify();
			for (UEdGraphPin* Pin : ScaleModule->Pins)
			{
				if (Pin->PinName == TEXT("Scale Ribbon Width"))
				{
					ScaleModule->GetSchema()->TrySetDefaultValue(*Pin, TEXT("true"));
					if (Pin->DefaultValue != TEXT("true")) return 3;
					++Changed;
					break;
				}
			}
			Data->GraphSource->MarkNotSynchronized(TEXT("Enable Basic ribbon area scaling"));
			UE_LOG(LogTemp, Display, TEXT("Basic ribbon: enabled owner-scale ribbon width on %s"), *Handle.GetName().ToString());
			continue;
		}
		// Copy the node list because creating an input override adds graph nodes.
		const auto Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Node);
			if (!Function || !Function->FunctionScript) continue;
			const FString ScriptName = Function->FunctionScript->GetName();
			FName Input;
			if (ScriptName == TEXT("ApplyOwnerScaleToAttributes")) Input = TEXT("Module.Owner Scale");
			else if (ScriptName == TEXT("ShapeLocation") && Handle.GetName() == TEXT("Lightning")) Input = TEXT("Module.Apply Owner Scale");
			else continue;
			Function->Modify();
			const auto Aliased = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(Input, Function);
			UEdGraphPin& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
				*Function, Aliased, FNiagaraTypeDefinition::GetVec3Def(), FGuid(), FGuid());
			Pin.BreakAllPinLinks();
			FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin, Baseline, KnownParameters);
			++Changed;
			UE_LOG(LogTemp, Display, TEXT("Secondary scale: %s / %s -> User.SecondaryEffectScale"), *Handle.GetName().ToString(), *ScriptName);
		}
		Data->GraphSource->MarkNotSynchronized(TEXT("Separate secondary effect scale from ribbon area scale"));
	}
	const int32 ExpectedChanges = bBasic ? 1 : 3;
	if (Changed != ExpectedChanges)
	{
		UE_LOG(LogTemp, Error, TEXT("Expected %d scale inputs; found %d. Asset was not saved."), ExpectedChanges, Changed);
		return 4;
	}
	System->RequestCompile(true);
	System->WaitForCompilationComplete(true, false);
	if (!System->IsReadyToRun()) return 5;
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(System->GetOutermost(), System, *Filename, SaveArgs) ? 0 : 6;
#else
	return 1;
#endif
}
