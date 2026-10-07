#pragma once
#include "PlayerUpgradeComponent.h"
#include "SurvivorAbilityComponent.h"

// Cosmetic only. The shared accent spawner enforces per-card rates and a global live-effect budget.
inline void PlayUpgradeProcVFX(const UPlayerUpgradeComponent* Upgrades, FName Id, FVector Position, float Radius = 60.f)
{
 if (!Upgrades || !Upgrades->GetOwner()) return;
 const auto* Card = Upgrades->FindUpgradeDefinition(Id);
 if (!Card || !Card->Presentation.PulseSystem) return;
 if (auto* FX = Upgrades->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
  FX->UpgradeAccent(Id, Position, Radius, FLinearColor::White);
}

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

inline void PlayScaledUpgradeBurst(const UObject* Context, UNiagaraSystem* System, FVector Origin, float Scale)
{
 if(!System)return;
 const FNiagaraVariable Parameter(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Scale"));
 const auto& Defaults=System->GetExposedParameters();
 const bool bParameter=Defaults.IndexOf(Parameter)!=INDEX_NONE;
 if(auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(Context,System,Origin,FRotator::ZeroRotator,
     bParameter?FVector::OneVector:FVector(Scale),true,false))
 {
  if(bParameter)FX->SetVariableFloat(TEXT("User.Scale"),Defaults.GetParameterValue<float>(Parameter)*Scale);
  FX->Activate(true);
 }
}
