#include "TechniqueTrialBase.h"
#include "TwinSoulTrial.h"
#include "SamuraiTechniqueTrial.h"
#include "BloodShrine.h"
#include "BossToriiGate.h"

#if !UE_BUILD_SHIPPING
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "AutoAttackComponent.h"
#include "Components/WidgetComponent.h"
#include "ObjectiveInteractionComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "SurvivorPlayerController.h"
#include "SamuraiCharacter.h"

static UWorld* FindTrialDebugWorld()
{
	if(!GEngine)return nullptr;
	for(const FWorldContext& Context:GEngine->GetWorldContexts())
	{
		if(UWorld* World=Context.World();World&&World->IsGameWorld())return World;
	}
	return nullptr;
}

static void DebugEnterSpawnedTrial(const TArray<FString>& Args)
{
	UWorld* World=FindTrialDebugWorld();
	ASurvivorPlayerController* PC=World?Cast<ASurvivorPlayerController>(World->GetFirstPlayerController()):nullptr;
	ACharacterBase* Character=PC&&PC->GetCharacterManager()?PC->GetCharacterManager()->GetActiveCharacter():nullptr;
	if(!World||!Character){UE_LOG(LogTemp,Error,TEXT("[TrialRuntime] DEBUG no game world/active character"));return;}
	const FString Requested=Args.IsEmpty()?TEXT("Samurai"):Args[0];
	for(TActorIterator<ATechniqueTrialBase> It(World);It;++It)
	{
		if(!Requested.Equals(TEXT("Samurai"),ESearchCase::IgnoreCase)||!It->IsA<ASamuraiTechniqueTrial>())continue;
		Character->SetActorLocation(It->GetActorLocation()+FVector(100,0,100),false,nullptr,ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG controller interaction request Trial=%s Type=%s"),*It->GetName(),*Requested);
		PC->DebugTriggerInteract();
		ACharacterBase* Samurai=PC->GetCharacterManager()?PC->GetCharacterManager()->GetSamurai():nullptr;
		UAutoAttackComponent* Attack=Samurai?Samurai->FindComponentByClass<UAutoAttackComponent>():nullptr;
		UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG SamuraiTrial State=%d AutoAttackEnabled=%s"),static_cast<int32>(It->GetTrialState()),Attack&&Attack->IsAutoAttackEnabled()?TEXT("true"):TEXT("false"));
		return;
	}
	for(TActorIterator<ATwinSoulTrial> It(World);It;++It)
	{
		if(Requested.Equals(TEXT("TwinSoul"),ESearchCase::IgnoreCase))
		{
			Character->SetActorLocation(It->GetActorLocation()+FVector(100,0,100),false,nullptr,ETeleportType::TeleportPhysics);
			It->Tick(0.0f);
			UWidgetComponent* Prompt=It->FindComponentByClass<UWidgetComponent>();
			UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG TwinSoulPrompt Visible=%s HiddenInGame=%s Widget=%s"),Prompt&&Prompt->IsVisible()?TEXT("true"):TEXT("false"),Prompt&&Prompt->bHiddenInGame?TEXT("true"):TEXT("false"),*GetNameSafe(Prompt?Prompt->GetUserWidgetObject():nullptr));
			UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG Enter request Trial=%s Type=TwinSoul"),*It->GetName());
			PC->DebugTriggerInteract();
			return;
		}
	}
	for(TActorIterator<ABloodShrine> It(World);It;++It)
	{
		if(!Requested.Equals(TEXT("BloodShrine"),ESearchCase::IgnoreCase))continue;
		Character->SetActorLocation(It->GetActorLocation()+FVector(100,0,100),false,nullptr,ETeleportType::TeleportPhysics);
		UObjectiveInteractionComponent* Prompt=It->FindComponentByClass<UObjectiveInteractionComponent>();if(Prompt)Prompt->RefreshPrompt();
		UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG BloodShrinePrompt Visible=%s Title=%s Range=%.1f"),Prompt&&Prompt->IsPromptVisible()?TEXT("true"):TEXT("false"),Prompt?*Prompt->Title.ToString():TEXT("None"),Prompt?Prompt->InteractionRange:0.0f);
		PC->DebugTriggerInteract();
		UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG BloodShrine State=%d"),static_cast<int32>(It->GetShrineState()));return;
	}
	for(TActorIterator<ABossToriiGate> It(World);It;++It)
	{
		if(!Requested.Equals(TEXT("Torii"),ESearchCase::IgnoreCase))continue;
		Character->SetActorLocation(It->GetActorLocation()+FVector(100,0,100),false,nullptr,ETeleportType::TeleportPhysics);
		UObjectiveInteractionComponent* Prompt=It->FindComponentByClass<UObjectiveInteractionComponent>();if(Prompt)Prompt->RefreshPrompt();
		UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] DEBUG ToriiPrompt Visible=%s Title=%s Range=%.1f State=%d"),Prompt&&Prompt->IsPromptVisible()?TEXT("true"):TEXT("false"),Prompt?*Prompt->Title.ToString():TEXT("None"),Prompt?Prompt->InteractionRange:0.0f,static_cast<int32>(It->GetGateState()));return;
	}
	UE_LOG(LogTemp,Error,TEXT("[TrialRuntime] DEBUG no spawned trial matches %s"),*Requested);
}

static FAutoConsoleCommand DebugEnterTrialCommand(
	TEXT("hd.DebugEnterSpawnedTrial"),
	TEXT("Invokes the real interaction path for a spawned Samurai or TwinSoul trial."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DebugEnterSpawnedTrial));
#endif
