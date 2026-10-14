#include "MetaSkillTree.h"
#include "HeavensDivideMetaSaveGame.h"
#include "BloodshiftBalanceData.h"
#include "ComboAbilityDefinition.h"

const TArray<FMetaSkillNode>& MetaSkillTree::DefaultNodes()
{
 static const TArray<FMetaSkillNode> Catalog = []
 {
  TArray<FMetaSkillNode> Result;
  auto Add = [&Result](const TCHAR* Id, const TCHAR* Name, const TCHAR* Text, int32 Branch, int32 Tier,
      FVector2D Position, const TCHAR* Parent, const TCHAR* Effect, float Value, int32 Ranks = 3)
  {
   FMetaSkillNode N;
   N.Id=Id; N.Name=Name; N.Description=Text; N.Branch=Branch; N.Tier=Tier; N.MapPosition=Position;
   N.MaxRank=Ranks; N.BaseCost=5+Tier*5; N.Effect=Effect; N.PerRank=Value;
   if (*Parent) N.Prerequisites.Add(FName(Parent));
   Result.Add(N);
  };
  // Shared trunk: every purchase benefits both heroes, regardless of stance.
  Add(TEXT("Root.Vitality"),TEXT("Living Flame"),TEXT("+{Percent}% maximum health per rank."),0,0,{510,355},TEXT(""),TEXT("Health"),.04f);
  Add(TEXT("Root.Gather"),TEXT("Ember Reach"),TEXT("+{Percent}% XP pickup radius per rank."),0,0,{580,310},TEXT("Root.Vitality"),TEXT("Pickup"),.16f);
  Add(TEXT("Root.Strength"),TEXT("Inner Fire"),TEXT("+{Percent}% damage for both heroes per rank."),0,1,{510,275},TEXT("Root.Vitality"),TEXT("Damage"),.03f);
  Add(TEXT("Root.Step"),TEXT("Pilgrim's Step"),TEXT("+{Percent}% movement speed per rank."),0,1,{642,253},TEXT("Root.Gather"),TEXT("Move"),.02f);
  Add(TEXT("Root.Tempo"),TEXT("Steady Breath"),TEXT("+{Percent}% basic attack speed for both heroes per rank."),0,2,{455,207},TEXT("Root.Strength"),TEXT("Attack"),.02f);
  Add(TEXT("Root.Endurance"),TEXT("Deep Reserves"),TEXT("+{Percent}% maximum health per rank."),0,2,{390,257},TEXT("Root.Strength"),TEXT("Health"),.05f);
  Add(TEXT("Root.Dash"),TEXT("Second Wind"),TEXT("+{Magnitude} maximum dash charge."),0,3,{393,157},TEXT("Root.Tempo"),TEXT("Dash"),1,1);
  Add(TEXT("Root.Horizon"),TEXT("Far Horizon"),TEXT("+{Percent}% XP pickup radius."),0,3,{708,207},TEXT("Root.Step"),TEXT("Pickup"),.4f,1);
  Add(TEXT("Bond.Flow"),TEXT("Crossing Souls"),TEXT("+{Percent}% swap recharge speed per rank."),3,0,{560,218},TEXT("Root.Strength"),TEXT("Swap"),.05f);
  Add(TEXT("Bond.Relay"),TEXT("Seamless Relay"),TEXT("+{Percent}% swap recharge speed per rank."),3,1,{620,155},TEXT("Bond.Flow"),TEXT("Swap"),.05f);
  Add(TEXT("Bond.Unity"),TEXT("Two Souls, One Will"),TEXT("+{Percent}% damage for both heroes."),3,3,{548,108},TEXT("Bond.Flow"),TEXT("Damage"),.08f,1);
  Add(TEXT("Bond.Rhythm"),TEXT("Heaven Undivided"),TEXT("+{Percent}% basic attack speed for both heroes."),3,3,{473,103},TEXT("Root.Tempo"),TEXT("Attack"),.08f,1);
  Add(TEXT("Bond.Reroll"),TEXT("Reroll"),TEXT("Unlock {Magnitude} rerolls per run. Replace your level-up upgrade choices."),3,2,{324,209},TEXT("Root.Endurance"),TEXT("Reroll"),3,1);
  Add(TEXT("Bond.Banish"),TEXT("Banish"),TEXT("Unlock {Magnitude} banishes per run. Replace a chosen upgrade and remove it from future offers for that run."),3,2,{290,132},TEXT("Bond.Reroll"),TEXT("Banish"),2,1);
  // The two character trunks contain only bonuses consumed by all of their stances.
  Add(TEXT("Steel.Edge"),TEXT("Tempered Edge"),TEXT("+{Percent}% Samurai direct damage per rank."),1,0,{595,468},TEXT("Root.Vitality"),TEXT("SteelDamage"),.04f);
  Add(TEXT("Steel.Tempo"),TEXT("Draw and Cut"),TEXT("+{Percent}% Samurai basic attack speed per rank."),1,1,{660,510},TEXT("Steel.Edge"),TEXT("SteelAttack"),.03f);
  Add(TEXT("Steel.Power"),TEXT("Mountain Splitter"),TEXT("+{Percent}% Samurai direct damage per rank."),1,2,{725,552},TEXT("Steel.Tempo"),TEXT("SteelDamage"),.04f);
  Add(TEXT("Steel.Resolve"),TEXT("Perfect Form"),TEXT("+{Percent}% Samurai basic attack speed."),1,3,{804,598},TEXT("Steel.Power"),TEXT("SteelAttack"),.08f,1);
  Add(TEXT("Shadow.Edge"),TEXT("Hidden Edge"),TEXT("+{Percent}% Ninja direct damage per rank."),2,0,{425,468},TEXT("Root.Vitality"),TEXT("ShadowDamage"),.04f);
  Add(TEXT("Shadow.Tempo"),TEXT("Flickering Hands"),TEXT("+{Percent}% Ninja basic attack speed per rank."),2,1,{360,510},TEXT("Shadow.Edge"),TEXT("ShadowAttack"),.03f);
  Add(TEXT("Shadow.Power"),TEXT("Assassin's Patience"),TEXT("+{Percent}% Ninja direct damage per rank."),2,2,{295,552},TEXT("Shadow.Tempo"),TEXT("ShadowDamage"),.04f);
  Add(TEXT("Shadow.Resolve"),TEXT("Unbroken Motion"),TEXT("+{Percent}% Ninja basic attack speed."),2,3,{216,598},TEXT("Shadow.Power"),TEXT("ShadowAttack"),.08f,1);
  auto Stance = [&](const TCHAR* Id,const TCHAR* Name,const TCHAR* Text,int32 Branch,int32 Tier,
      FVector2D Position,const TCHAR* Parent,const TCHAR* Effect,float Value,const TCHAR* Required,int32 Ranks=3)
  {
   Add(Id,Name,Text,Branch,Tier,Position,Parent,Effect,Value,Ranks);
   Result.Last().RequiredStance=Required;
  };
  Stance(TEXT("Blood.Edge"),TEXT("Crimson Edge"),TEXT("+{Percent}% Blood stance direct damage per rank."),1,1,{720,425},TEXT("Steel.Power"),TEXT("SteelDamage"),.03f,TEXT("BattleStance"));
  Stance(TEXT("Blood.Reach"),TEXT("Sweeping Steel"),TEXT("+{Percent}% Blood stance melee and blood-burst radius per rank."),1,2,{805,389},TEXT("Blood.Edge"),TEXT("SteelArea"),.04f,TEXT("BattleStance"));
  Stance(TEXT("Blood.Vein"),TEXT("Unclosing Wounds"),TEXT("+{Percent}% Bleed damage while using Blood stance."),1,3,{895,354},TEXT("Blood.Reach"),TEXT("Bleed"),.2f,TEXT("BattleStance"),1);
  Stance(TEXT("Iaijutsu.Edge"),TEXT("Drawn in Silence"),TEXT("+{Percent}% Iaijutsu direct damage per rank."),1,1,{778,481},TEXT("Steel.Power"),TEXT("SteelDamage"),.03f,TEXT("Iaijutsu"));
  Stance(TEXT("Iaijutsu.Width"),TEXT("Heaven's Divide"),TEXT("+{Percent}% Iaijutsu slash width per rank."),1,2,{863,477},TEXT("Iaijutsu.Edge"),TEXT("SteelArea"),.04f,TEXT("Iaijutsu"));
  Stance(TEXT("Iaijutsu.Focus"),TEXT("Stillness Before Dawn"),TEXT("+{Percent}% Iaijutsu charge speed."),1,3,{947,471},TEXT("Iaijutsu.Width"),TEXT("IaijutsuCharge"),.12f,TEXT("Iaijutsu"),1);
  Stance(TEXT("Crescent.Edge"),TEXT("Moonlit Edge"),TEXT("+{Percent}% Crescent direct damage per rank."),1,1,{729,633},TEXT("Steel.Power"),TEXT("SteelDamage"),.03f,TEXT("BladeWave"));
  Stance(TEXT("Crescent.Width"),TEXT("Wide Horizon"),TEXT("+{Percent}% Crescent wave and lingering-field width per rank."),1,2,{777,705},TEXT("Crescent.Edge"),TEXT("SteelArea"),.04f,TEXT("BladeWave"));
  Stance(TEXT("Crescent.Flow"),TEXT("Flowing Moon"),TEXT("+{Percent}% Crescent basic attack speed."),1,3,{820,780},TEXT("Crescent.Width"),TEXT("SteelAttack"),.12f,TEXT("BladeWave"),1);
  Stance(TEXT("Fang.Edge"),TEXT("Returning Edge"),TEXT("+{Percent}% Returning Fang direct damage per rank."),2,1,{300,425},TEXT("Shadow.Power"),TEXT("ShadowDamage"),.03f,TEXT("ReturningFang"));
  Stance(TEXT("Fang.Reach"),TEXT("Patient Hunter"),TEXT("+{Percent}% Returning Fang targeting range per rank."),2,2,{215,389},TEXT("Fang.Edge"),TEXT("FangRange"),.04f,TEXT("ReturningFang"));
  Stance(TEXT("Fang.Flight"),TEXT("Homeward Wind"),TEXT("+{Percent}% Returning Fang travel speed."),2,3,{125,354},TEXT("Fang.Reach"),TEXT("ShadowFlight"),.12f,TEXT("ReturningFang"),1);
  Stance(TEXT("Barrage.Edge"),TEXT("Needlepoint"),TEXT("+{Percent}% Barrage direct damage per rank."),2,1,{242,481},TEXT("Shadow.Power"),TEXT("ShadowDamage"),.03f,TEXT("BarrageStance"));
  Stance(TEXT("Barrage.Tempo"),TEXT("Relentless Rhythm"),TEXT("+{Percent}% Barrage basic attack speed per rank."),2,2,{157,477},TEXT("Barrage.Edge"),TEXT("ShadowAttack"),.03f,TEXT("BarrageStance"));
  Stance(TEXT("Barrage.Twin"),TEXT("Thousand Needles"),TEXT("+{Magnitude} projectile per Barrage volley."),2,3,{73,471},TEXT("Barrage.Tempo"),TEXT("Projectile"),1,TEXT("BarrageStance"),1);
  Stance(TEXT("Shuriken.Edge"),TEXT("Turning Blade"),TEXT("+{Percent}% Great Shuriken direct damage per rank."),2,1,{291,633},TEXT("Shadow.Power"),TEXT("ShadowDamage"),.03f,TEXT("GreatShuriken"));
  Stance(TEXT("Shuriken.Size"),TEXT("Widening Circle"),TEXT("+{Percent}% Great Shuriken blade radius per rank."),2,2,{243,705},TEXT("Shuriken.Edge"),TEXT("ShurikenSize"),.04f,TEXT("GreatShuriken"));
  Stance(TEXT("Shuriken.Grind"),TEXT("Endless Wheel"),TEXT("+{Percent}% Great Shuriken contact and pulse frequency."),2,3,{200,780},TEXT("Shuriken.Size"),TEXT("ShurikenTempo"),.12f,TEXT("GreatShuriken"),1);
  Add(TEXT("Root.Area"),TEXT("Expanding Presence"),TEXT("+{Percent}% attack coverage per rank: melee/slash/wave width, Fang reach, Barrage kunai and pool size, or Shuriken blade size."),0,1,{720,310},TEXT("Root.Gather"),TEXT("Area"),.04f,5);
  Add(TEXT("Root.Currency"),TEXT("Soul Harvest"),TEXT("+{Percent}% Soul Embers and Soul Sigils earned per rank. Small bonuses accumulate across rewards."),0,2,{790,253},TEXT("Root.Area"),TEXT("Currency"),.05f,5);
  Add(TEXT("Ability.ComboUnlock"),TEXT("Soul Convergence"),TEXT("Unlock the combo burst. Build the shared meter through combat, then unleash your active hero's combo ability."),3,0,{450,350},TEXT(""),TEXT("ComboUnlock"),1,1);
  Add(TEXT("Ability.ComboCharge"),TEXT("Gathering Resonance"),TEXT("+{Percent}% combo meter gained per rank."),3,1,{500,255},TEXT("Ability.ComboUnlock"),TEXT("ComboCharge"),.05f);
  Add(TEXT("Ability.ComboPower"),TEXT("Resonant Release"),TEXT("+{Percent}% combo ability damage per rank."),3,2,{555,165},TEXT("Ability.ComboCharge"),TEXT("ComboPower"),.05f);
  for (const auto& Combo : ComboCatalog::Entries())
  {
   const FString Id = ComboCatalog::UnlockId(Combo.Id).ToString();
   const FString Effect = TEXT("ComboChoice.") + Combo.Id.ToString();
   Add(*Id,Combo.Name,Combo.Description,3,1,{0,0},TEXT("Ability.ComboUnlock"),*Effect,1,1);
   Result.Last().BaseCost = 6;
  }
  for (FMetaSkillNode& N:Result)
  {
   if (!N.RequiredStance.IsNone())
   {
    N.Page=EMetaSkillPage::Stances;
    if (N.Id.ToString().EndsWith(TEXT(".Edge"))) N.Prerequisites.Reset();
   }
   if (N.Effect==TEXT("Swap") || N.Effect==TEXT("Dash") || N.Effect==TEXT("Reroll") ||
       N.Effect==TEXT("Banish") || N.Effect.ToString().StartsWith(TEXT("Combo")))
   {
    N.Page=EMetaSkillPage::Abilities;
    if (N.Id==TEXT("Root.Dash") || N.Id==TEXT("Bond.Flow") || N.Id==TEXT("Bond.Reroll")) N.Prerequisites.Reset();
   }
   if (N.Id==TEXT("Bond.Unity")) N.Prerequisites={FName(TEXT("Root.Strength"))};
   if (N.Id==TEXT("Ability.ComboUnlock")) { N.BaseCost=10; N.Description=TEXT("Unlock joint combo attacks. Both heroes cast together, regardless of stance."); }
   if (N.Id==TEXT("Ability.Combo.BloodMoon")) { N.BaseCost=0; N.Description+=TEXT(" Included with Soul Convergence."); }
   if (N.Effect==TEXT("Attack") || N.Effect==TEXT("SteelAttack") || N.Effect==TEXT("ShadowAttack"))
    N.Description=TEXT("+{Percent}% attack cadence per rank: attack rate, Iaijutsu charge speed, Fang flight speed, or Shuriken hit frequency.");
   if(N.Effect==TEXT("Health"))N.SharedStat=ESharedPlayerStatType::MaxHealthMultiplier;
   if(N.Effect==TEXT("Pickup"))N.SharedStat=ESharedPlayerStatType::PickupRadiusMultiplier;
   if(N.Effect==TEXT("Move"))N.SharedStat=ESharedPlayerStatType::MoveSpeedMultiplier;
   if(N.Effect==TEXT("Attack"))N.SharedStat=ESharedPlayerStatType::AttackSpeedMultiplier;
   if(N.Effect==TEXT("Dash"))N.SharedStat=ESharedPlayerStatType::MaxDashCharges;
   if(N.Branch==1)N.Target=EUpgradeStatTarget::Samurai;
   if(N.Branch==2)N.Target=EUpgradeStatTarget::Ninja;
   if(N.Effect==TEXT("SteelArea"))N.CharacterStat=ECharacterStatType::AttackAreaMultiplier;
   if(N.Effect==TEXT("SteelAttack")||N.Effect==TEXT("ShadowAttack"))N.CharacterStat=ECharacterStatType::AttackSpeedMultiplier;
   if(N.Effect==TEXT("ShadowFlight"))N.CharacterStat=ECharacterStatType::ProjectileSpeedMultiplier;
   if(N.Effect==TEXT("Projectile"))N.CharacterStat=ECharacterStatType::ProjectileCountBonus;
   N.bPassiveEffect=N.Effect==TEXT("Swap")||N.Effect==TEXT("Bleed")||N.Effect==TEXT("Reroll")||N.Effect==TEXT("Banish")
       ||N.Effect==TEXT("Area")||N.Effect==TEXT("Currency")||N.Effect==TEXT("Attack")||N.Effect==TEXT("SteelAttack")||N.Effect==TEXT("ShadowAttack")
       ||N.Effect.ToString().StartsWith(TEXT("Combo"))||N.Effect==TEXT("IaijutsuCharge")||N.Effect==TEXT("FangRange")||N.Effect==TEXT("ShurikenSize")||N.Effect==TEXT("ShurikenTempo");
  }
  return Result;
 }();
 return Catalog;
}

const TArray<FMetaSkillNode>& MetaSkillTree::Nodes()
{
	const auto* Balance=UBloodshiftBalanceData::Get();
    if (Balance->Skills.IsEmpty()) return DefaultNodes();
    // Saved balance catalogs retain every authored value; append newly introduced
    // techniques by stable ID so existing assets need no destructive migration.
    static TArray<FMetaSkillNode> Combined;
    static const UBloodshiftBalanceData* CachedBalance = nullptr;
    static uint32 CachedHash = 0;
    uint32 Hash = GetTypeHash(Balance->Skills.Num());
    for (const auto& Node : Balance->Skills)
    {
        Hash = HashCombine(Hash,GetTypeHash(Node.Id)); Hash = HashCombine(Hash,GetTypeHash(Node.Name));
        Hash = HashCombine(Hash,GetTypeHash(Node.Description)); Hash = HashCombine(Hash,GetTypeHash(Node.BaseCost));
        Hash = HashCombine(Hash,GetTypeHash(Node.MaxRank)); Hash = HashCombine(Hash,GetTypeHash(Node.PerRank));
        Hash = HashCombine(Hash,GetTypeHash(Node.Effect)); Hash = HashCombine(Hash,GetTypeHash(Node.RequiredStance));
        Hash = HashCombine(Hash,GetTypeHash(Node.Branch)); Hash = HashCombine(Hash,GetTypeHash(Node.Tier));
        Hash = HashCombine(Hash,GetTypeHash(static_cast<uint8>(Node.Page)));
        Hash = HashCombine(Hash,GetTypeHash(static_cast<uint8>(Node.Target)));
        Hash = HashCombine(Hash,GetTypeHash(Node.Lane)); Hash = HashCombine(Hash,GetTypeHash(Node.MapPosition));
        Hash = HashCombine(Hash,GetTypeHash(Node.bPassiveEffect));
        Hash = HashCombine(Hash,GetTypeHash(static_cast<uint8>(Node.SharedStat)));
        Hash = HashCombine(Hash,GetTypeHash(static_cast<uint8>(Node.CharacterStat)));
        for (FName Parent : Node.Prerequisites) Hash = HashCombine(Hash,GetTypeHash(Parent));
    }
    if (CachedBalance == Balance && CachedHash == Hash) return Combined;
    CachedBalance = Balance; CachedHash = Hash;
    Combined = Balance->Skills;
    Combined.RemoveAll([](const FMetaSkillNode& N){return N.Effect.ToString().StartsWith(TEXT("ComboChoice."));});
    for (const auto& Node : DefaultNodes())
        if (Node.Effect.ToString().StartsWith(TEXT("ComboChoice.")) && !Combined.ContainsByPredicate([&Node](const FMetaSkillNode& Existing){return Existing.Id == Node.Id;})) Combined.Add(Node);
    for (auto& N : Combined) if (N.Id==TEXT("Ability.ComboUnlock")) N.Description=TEXT("Both heroes cast together. Includes Crimson Tempest; equip one shared recipe in Loadout.");
    return Combined;
}
FString MetaSkillTree::Description(const FMetaSkillNode& Node)
{
	return Node.Description.Replace(TEXT("{Percent}"),*FText::AsNumber(Node.PerRank*100.f).ToString())
		.Replace(TEXT("{Magnitude}"),*FText::AsNumber(Node.PerRank).ToString());
}

FString MetaSkillTree::StanceName(FName Stance)
{
 if(Stance==TEXT("BattleStance"))return TEXT("Blood");
 if(Stance==TEXT("Iaijutsu"))return TEXT("Iaijutsu");
 if(Stance==TEXT("BladeWave"))return TEXT("Crescent");
 if(Stance==TEXT("ReturningFang"))return TEXT("Returning Fang");
 if(Stance==TEXT("BarrageStance"))return TEXT("Barrage");
 if(Stance==TEXT("GreatShuriken"))return TEXT("Great Shuriken");
 return Stance.ToString();
}
FString MetaSkillTree::Scope(const FMetaSkillNode& Node)
{
 if(!Node.RequiredStance.IsNone())return StanceName(Node.RequiredStance)+TEXT(" stance only");
 if(Node.Target==EUpgradeStatTarget::Samurai)return TEXT("Samurai / all stances");
 if(Node.Target==EUpgradeStatTarget::Ninja)return TEXT("Ninja / all stances");
 return TEXT("Both heroes / all stances");
}
const FMetaSkillNode* MetaSkillTree::Find(FName Id)
{
	return Nodes().FindByPredicate([Id](const FMetaSkillNode& N) { return N.Id == Id; });
}
int32 MetaSkillTree::Rank(const UHeavensDivideMetaSaveGame& Save, FName Id)
{
	const FMetaSkillNode* N = Find(Id);
	return N ? FMath::Clamp(Save.SkillRanks.FindRef(Id), 0, N->MaxRank) : 0;
}
int32 MetaSkillTree::Cost(const UHeavensDivideMetaSaveGame& Save, FName Id)
{
	const FMetaSkillNode* N = Find(Id);
	if (!N || Rank(Save, Id) >= N->MaxRank) return 0;
	// Every permanent purchase costs three times its original catalog price.
	// Apply this to authored and fallback catalogs alike; recorded old spend stays exact.
	return static_cast<int32>(FMath::Clamp<int64>(int64(N->BaseCost) * (Rank(Save, Id) + 1) * 3, 0, MAX_int32));
}
FString MetaSkillTree::PurchaseBlock(const UHeavensDivideMetaSaveGame& Save, FName Id)
{
	const FMetaSkillNode* N = Find(Id);
	if (!N) return TEXT("Unknown skill.");
	if (Rank(Save, Id) >= N->MaxRank) return TEXT("Fully learned.");
	for (FName Parent : N->Prerequisites)
		if (Rank(Save, Parent) == 0) return Find(Parent)?FString::Printf(TEXT("Learn %s first."), *Find(Parent)->Name):TEXT("Invalid prerequisite skill ID.");
	if (Save.SoulEmbers < Cost(Save, Id)) return TEXT("Earn Soul Embers from combat, objectives and surviving runs.");
	return FString();
}
bool MetaSkillTree::Purchase(UHeavensDivideMetaSaveGame& Save, FName Id)
{
	if (!PurchaseBlock(Save, Id).IsEmpty()) return false;
	const int32 Price = Cost(Save, Id);
	Save.SoulEmbers -= Price;
	Save.SkillEmbersSpent = static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(Save.SkillEmbersSpent) + Price));
	Save.SkillRanks.Add(Id, Rank(Save, Id) + 1);
	return true;
}
int32 MetaSkillTree::Refund(UHeavensDivideMetaSaveGame& Save)
{
	const int32 Amount = FMath::Max(0, Save.SkillEmbersSpent);
	Save.SoulEmbers = static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(FMath::Max(0, Save.SoulEmbers)) + Amount));
	Save.SkillRanks.Reset(); Save.SkillEmbersSpent = 0;
	return Amount;
}
void MetaSkillTree::Sanitize(UHeavensDivideMetaSaveGame& Save)
{
	Save.SoulEmbers = FMath::Max(0, Save.SoulEmbers);
	Save.SkillEmbersSpent = FMath::Max(0, Save.SkillEmbersSpent);
	// The stance-tree redesign changes both effects and prerequisite paths.
	// Return the exact recorded spend once; other meta progression stays intact.
	if (Save.SaveVersion < 5) Refund(Save);
	// Refund retired Prepare passives once, keeping all surviving purchases.
	const TPair<FName, int32> Retired[] = {
		{TEXT("Bond.Memory"), 5}, {TEXT("Bond.Strike"), 10},
		{TEXT("Bond.Echo"), 15}, {TEXT("Bond.Reaction"), 15}};
	int32 RefundAmount = 0;
	for (const auto& Node : Retired)
	{
		const int32 Ranks = FMath::Clamp(Save.SkillRanks.FindRef(Node.Key), 0, 3);
		RefundAmount += Node.Value * Ranks * (Ranks + 1) / 2;
		Save.SkillRanks.Remove(Node.Key);
	}
	RefundAmount = FMath::Min(RefundAmount, Save.SkillEmbersSpent);
	Save.SkillEmbersSpent -= RefundAmount;
	Save.SoulEmbers = static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(Save.SoulEmbers) + RefundAmount));
	for (auto It = Save.SkillRanks.CreateIterator(); It; ++It)
	{
		const FMetaSkillNode* N = Find(It.Key());
		if (!N || It.Value() <= 0) It.RemoveCurrent();
		else It.Value() = FMath::Min(It.Value(), N->MaxRank);
	}
	// Catalog order is topological; invalid children cannot survive missing parents.
	for (const FMetaSkillNode& N : Nodes())
		for (FName Parent : N.Prerequisites)
			if (Rank(Save, Parent) == 0) { Save.SkillRanks.Remove(N.Id); break; }
}
float MetaSkillTree::Bonus(const UHeavensDivideMetaSaveGame& Save, FName Effect)
{
	float Value = 0;
	for (const FMetaSkillNode& N : Nodes()) if (N.Effect == Effect) Value += Rank(Save, N.Id) * N.PerRank;
	return Value;
}
int32 MetaSkillTree::RunReward(float Seconds, bool bVictory)
{
	if (!FMath::IsFinite(Seconds) || Seconds < 0) return 0;
	const auto* Balance=UBloodshiftBalanceData::Get();
	return FMath::FloorToInt(FMath::Min(Seconds, FMath::Max(0.f,Balance->RewardTimeCap)) / FMath::Max(1.f,Balance->SecondsPerEmber)) + (bVictory ? FMath::Max(0,Balance->VictoryEmbers) : 0);
}
