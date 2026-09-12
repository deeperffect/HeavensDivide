#include "SurvivorAbilityComponent.h"
#include "PlayerUpgradeComponent.h"
#include "UpgradeDefinition.h"
#include "Engine/World.h"
#include "EnemyStatusEffectComponent.h"

UUpgradeDefinition* USurvivorAbilityComponent::TuningDefinition(int32 Family,int32 Slot) const
{
 if(!Upgrades||Family<0||Family>=20)return nullptr;
 const auto& S=BuildFamilies[Family];const FName Id=Slot==3?S.Synergy:Slot>=0&&Slot<3?S.Branches[Slot]:S.Id;
 if(const auto* Cached=TuningCache.Find(Id);Cached&&Cached->IsValid())return Cached->Get();
 auto* Definition=Upgrades->FindUpgradeDefinition(Id);if(Definition)TuningCache.Add(Id,Definition);return Definition;
}
float USurvivorAbilityComponent::Tuning(int32 Family,FName Key,float Fallback,int32 Slot) const
{
 if(Family<0||Family>=20)return Fallback;
 if(Slot==-1)for(int32 BranchIndex=2;BranchIndex>=0;--BranchIndex)
  if(Branch(Family,BranchIndex))if(auto* D=TuningDefinition(Family,BranchIndex))
   if(D->BalanceParameters.Contains(Key))return D->GetBalanceValue(Key,Fallback);
 if(auto* D=TuningDefinition(Family,Slot))return D->GetBalanceValue(Key,Fallback);
 return Fallback;
}
FBuildFamilySpec USurvivorAbilityComponent::FamilySpec(int32 Family) const
{
 FBuildFamilySpec S=BuildFamilies[Family];
 S.Damage=FMath::Max(0.0f,Tuning(Family,TEXT("Damage"),S.Damage));
 S.Radius=FMath::Max(1.0f,Tuning(Family,TEXT("Radius"),S.Radius));
 S.Cooldown=FMath::Max(0.1f,Tuning(Family,TEXT("Cooldown"),S.Cooldown));
 S.Count=FMath::Clamp(FMath::RoundToInt(Tuning(Family,TEXT("Count"),S.Count)),1,64);
 S.Interval=FMath::Max(0.1f,Tuning(Family,TEXT("Interval"),S.Interval));
 S.ReactionFactor=FMath::Max(0.0f,Tuning(Family,TEXT("DamageMultiplier"),S.ReactionFactor,3));
 S.ReactionRadius=FMath::Max(1.0f,Tuning(Family,TEXT("Radius"),S.ReactionRadius,3));
 S.ReactionCount=FMath::Clamp(FMath::RoundToInt(Tuning(Family,TEXT("Count"),S.ReactionCount,3)),1,64);
 return S;
}
const FUpgradePresentation* USurvivorAbilityComponent::FamilyPresentation(int32 Family,int32 Slot) const
{
 if(Family<0||Family>=20)return nullptr;
 if(Slot>=0)if(auto* D=TuningDefinition(Family,Slot);D&&D->Presentation.bOverrideFamilyVisuals)return &D->Presentation;
 if(Slot!=3)for(int32 B=2;B>=0;--B)if(Branch(Family,B))
  if(auto* D=TuningDefinition(Family,B);D&&D->Presentation.bOverrideFamilyVisuals)return &D->Presentation;
 if(auto* D=TuningDefinition(Family))return &D->Presentation;
 return nullptr;
}
AAbilityAccent* USurvivorAbilityComponent::FamilyAccent(int32 Family,FVector Position,FVector End,float Radius,FLinearColor Color,float Duration,bool bBeam,int32 Slot,int32 Stage)
{
 if(Family<0||Family>=20)return Accent(Position,End,Radius,Color,Duration,bBeam);
 if(bResolvingReaction)Slot=3;
 const auto* Settings=FamilyPresentation(Family,Slot);
 if(Stage==2&&(!Settings||!Settings->ImpactSystem))return nullptr;
 return Accent(Position,End,Radius,Color,Duration,bBeam,Settings,Stage);
}

void USurvivorAbilityComponent::ApplyConfiguredStatus(int32 Family,AEnemyBase* Enemy,EEnemyStatusEffect Status,EPlayerAttackSource Source)
{
 if(!IsValid(Enemy)||Enemy->IsDead())return;
 const int32 Count=FMath::Clamp(FMath::RoundToInt(Tuning(Family,TEXT("StatusStacks"),1,bResolvingReaction?3:-1)),0,64);
 for(int32 i=0;i<Count&&!Enemy->IsDead();++i)Enemy->GetStatusEffectComponent()->ApplyStatus(Status,Upgrades,Source,true);
}
