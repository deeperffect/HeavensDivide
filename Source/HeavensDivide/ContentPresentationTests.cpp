#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Blueprint.h"
#include "CharacterBase.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "SurvivorAbilityComponent.h"
#include "UpgradeDefinition.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentPresentationTest,"HeavensDivide.Presentation.ContentPass",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FContentPresentationTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto* Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 if(PC)
 for(const TCHAR* Id:{TEXT("OverkillBurst"),TEXT("BloodTransfer"),TEXT("BattleStance"),TEXT("VenomousKunai"),TEXT("EmbeddedBlades"),TEXT("GrandEntrance"),TEXT("TagTeam")})
 {
  const auto* Card=PC->GetPlayerUpgrades()->FindUpgradeDefinition(Id);
  if(!TestNotNull(FString(Id)+TEXT(" remains in active upgrade pool"),Card))continue;
  auto* System=FCString::Strcmp(Id,TEXT("TagTeam"))==0?Card->Presentation.LineSystem.Get():Card->Presentation.PulseSystem.Get();
  if(!TestNotNull(FString(Id)+TEXT(" has a real effect"),System))continue;
  System->WaitForCompilationComplete(true,false);
  TestTrue(FString(Id)+TEXT(" Niagara compiles"),System->IsValid());
  TestTrue(FString(Id)+TEXT(" editable runtime presentation"),Card->bHasRuntimePresentation);
  TArray<FNiagaraVariable> Params;System->GetExposedParameters().GetParameters(Params);
  const auto& Presentation=Card->Presentation;
  const FNiagaraVariable Scale(FNiagaraTypeDefinition::GetFloatDef(),Presentation.SystemScaleParameter);
  TestTrue(FString(Id)+TEXT(" binds an actual vendor scale parameter"),System->GetExposedParameters().IndexOf(Scale)!=INDEX_NONE);
  auto* Accent=World->SpawnActor<AAbilityAccent>();
  const bool bLine=FCString::Strcmp(Id,TEXT("TagTeam"))==0;
  Accent->Initialize(FVector(200,0,0),Presentation.AuthoredRadius*2,Presentation.Color,.5f,bLine,&Presentation);
  auto* FX=Accent->FindComponentByClass<UNiagaraComponent>();
  if(TestNotNull(TEXT("Niagara component"),FX))
   TestEqual(FString(Id)+TEXT(" radius is applied once through vendor scale"),FX->GetOverrideParameters().GetParameterValue<float>(Scale),float(Presentation.Scale.X)*(Presentation.bScaleSystemToRadius?2.f:1.f)*(Presentation.bMultiplyAuthoredSystemScale?System->GetExposedParameters().GetParameterValue<float>(Scale):1.f));
  Accent->Destroy();
 }
 for(const TCHAR* Name:{TEXT("Samurai"),TEXT("Ninja")})
 {
  auto* CharacterClass=LoadClass<ACharacterBase>(nullptr,*FString::Printf(TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_%s.BP_%s_C"),Name,Name));
  if(TestNotNull(TEXT("Character Blueprint"),CharacterClass))
   TestNotNull(FString(Name)+TEXT(" active ability already has VFX"),CharacterClass->GetDefaultObject<ACharacterBase>()->ComboAbility.VFX.Get());
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
