#include "MetaSkillTree.h"
#include "HeavensDivideMetaSaveGame.h"

const TArray<FMetaSkillNode>& MetaSkillTree::Nodes()
{
	static const TArray<FMetaSkillNode> Catalog = []
	{
		TArray<FMetaSkillNode> Result;
		auto Add = [&Result](const TCHAR* Id, const TCHAR* Name, const TCHAR* Text, int32 Branch, int32 Tier, int32 Lane,
			const TCHAR* Parent, const TCHAR* Effect, float Value, int32 Ranks = 3)
		{
			FMetaSkillNode N;
			N.Id = Id; N.Name = Name; N.Description = Text; N.Branch = Branch; N.Tier = Tier; N.Lane = Lane;
			N.MaxRank = Ranks; N.BaseCost = 5 + Tier * 5; N.Effect = Effect; N.PerRank = Value;
			if (*Parent) N.Prerequisites.Add(FName(Parent));
			Result.Add(N);
		};
		Add(TEXT("Root.Vitality"), TEXT("Living Flame"), TEXT("+4% maximum health per rank."), 0,0,0,TEXT(""),TEXT("Health"),.04f);
		Add(TEXT("Root.Gather"), TEXT("Ember Reach"), TEXT("+8% XP pickup radius per rank."), 0,0,1,TEXT(""),TEXT("Pickup"),.08f);
		Add(TEXT("Root.Strength"), TEXT("Inner Fire"), TEXT("+3% damage for both characters per rank."), 0,1,0,TEXT("Root.Vitality"),TEXT("Damage"),.03f);
		Add(TEXT("Root.Step"), TEXT("Pilgrim's Step"), TEXT("+2% movement speed per rank."), 0,1,1,TEXT("Root.Gather"),TEXT("Move"),.02f);
		Add(TEXT("Root.Tempo"), TEXT("Steady Breath"), TEXT("+2% basic attack speed for both characters per rank."), 0,2,0,TEXT("Root.Strength"),TEXT("Attack"),.02f);
		Add(TEXT("Root.Endurance"), TEXT("Deep Reserves"), TEXT("+5% maximum health per rank."), 0,2,1,TEXT("Root.Step"),TEXT("Health"),.05f);
		Add(TEXT("Root.Dash"), TEXT("Second Wind"), TEXT("+1 maximum dash charge."), 0,3,0,TEXT("Root.Tempo"),TEXT("Dash"),1,1);
		Add(TEXT("Root.Horizon"), TEXT("Far Horizon"), TEXT("+20% XP pickup radius."), 0,3,1,TEXT("Root.Endurance"),TEXT("Pickup"),.2f,1);
		Add(TEXT("Steel.Edge"), TEXT("Tempered Edge"), TEXT("+4% Samurai direct damage per rank."), 1,0,0,TEXT("Root.Vitality"),TEXT("SteelDamage"),.04f);
		Add(TEXT("Steel.Blood"), TEXT("Crimson Oath"), TEXT("+5% Bleed damage per rank. Does not grant Bleed."), 1,0,1,TEXT("Root.Vitality"),TEXT("Bleed"),.05f);
		Add(TEXT("Steel.Reach"), TEXT("Sweeping Steel"), TEXT("+5% Samurai basic melee area scale per rank."), 1,1,0,TEXT("Steel.Edge"),TEXT("SteelArea"),.05f);
		Add(TEXT("Steel.Tempo"), TEXT("Draw and Cut"), TEXT("+3% Samurai basic attack speed per rank."), 1,1,1,TEXT("Steel.Blood"),TEXT("SteelAttack"),.03f);
		Add(TEXT("Steel.Power"), TEXT("Mountain Splitter"), TEXT("+4% Samurai direct damage per rank."), 1,2,0,TEXT("Steel.Reach"),TEXT("SteelDamage"),.04f);
		Add(TEXT("Steel.Wounds"), TEXT("Unclosing Wounds"), TEXT("+5% Bleed damage per rank."), 1,2,1,TEXT("Steel.Tempo"),TEXT("Bleed"),.05f);
		Add(TEXT("Steel.Horizon"), TEXT("Heaven's Arc"), TEXT("+15% Samurai basic melee area scale."), 1,3,0,TEXT("Steel.Power"),TEXT("SteelArea"),.15f,1);
		Add(TEXT("Steel.Resolve"), TEXT("Perfect Form"), TEXT("+8% Samurai basic attack speed."), 1,3,1,TEXT("Steel.Wounds"),TEXT("SteelAttack"),.08f,1);
		Add(TEXT("Shadow.Edge"), TEXT("Hidden Edge"), TEXT("+4% Ninja direct damage per rank."), 2,0,0,TEXT("Root.Gather"),TEXT("ShadowDamage"),.04f);
		Add(TEXT("Shadow.Venom"), TEXT("Jade Venom"), TEXT("+5% Poison damage per rank. Does not grant Poison."), 2,0,1,TEXT("Root.Gather"),TEXT("Poison"),.05f);
		Add(TEXT("Shadow.Flight"), TEXT("Silent Flight"), TEXT("+6% Ninja basic projectile speed per rank."), 2,1,0,TEXT("Shadow.Edge"),TEXT("ShadowFlight"),.06f);
		Add(TEXT("Shadow.Tempo"), TEXT("Flickering Hands"), TEXT("+3% Ninja basic attack speed per rank."), 2,1,1,TEXT("Shadow.Venom"),TEXT("ShadowAttack"),.03f);
		Add(TEXT("Shadow.Power"), TEXT("Assassin's Patience"), TEXT("+4% Ninja direct damage per rank."), 2,2,0,TEXT("Shadow.Flight"),TEXT("ShadowDamage"),.04f);
		Add(TEXT("Shadow.Toxin"), TEXT("Black Lotus"), TEXT("+5% Poison damage per rank."), 2,2,1,TEXT("Shadow.Tempo"),TEXT("Poison"),.05f);
		Add(TEXT("Shadow.Pierce"), TEXT("Through the Veil"), TEXT("Ninja basic projectiles pierce one additional enemy."), 2,3,0,TEXT("Shadow.Power"),TEXT("Pierce"),1,1);
		Add(TEXT("Shadow.Twin"), TEXT("Twin Fangs"), TEXT("+1 Ninja basic projectile."), 2,3,1,TEXT("Shadow.Toxin"),TEXT("Projectile"),1,1);
		Add(TEXT("Bond.Flow"), TEXT("Crossing Souls"), TEXT("+5% swap recharge speed per rank."), 3,0,0,TEXT("Root.Strength"),TEXT("Swap"),.05f);
		Add(TEXT("Bond.Memory"), TEXT("Lingering Intent"), TEXT("Family synergy preparation lasts +0.5 seconds per rank."), 3,0,1,TEXT("Root.Step"),TEXT("Preparation"),.5f);
		Add(TEXT("Bond.Strike"), TEXT("Answered Challenge"), TEXT("+5% family partner-reaction damage per rank."), 3,1,0,TEXT("Bond.Flow"),TEXT("Reaction"),.05f);
		Add(TEXT("Bond.Relay"), TEXT("Seamless Relay"), TEXT("+5% swap recharge speed per rank."), 3,1,1,TEXT("Bond.Memory"),TEXT("Swap"),.05f);
		Add(TEXT("Bond.Echo"), TEXT("Unbroken Promise"), TEXT("Family synergy preparation lasts +0.5 seconds per rank."), 3,2,0,TEXT("Bond.Strike"),TEXT("Preparation"),.5f);
		Add(TEXT("Bond.Reaction"), TEXT("Converging Blades"), TEXT("+5% family partner-reaction damage per rank."), 3,2,1,TEXT("Bond.Relay"),TEXT("Reaction"),.05f);
		Add(TEXT("Bond.Unity"), TEXT("Two Souls, One Will"), TEXT("+8% damage for both characters."), 3,3,0,TEXT("Bond.Echo"),TEXT("Damage"),.08f,1);
		Add(TEXT("Bond.Rhythm"), TEXT("Heaven Undivided"), TEXT("+8% basic attack speed for both characters."), 3,3,1,TEXT("Bond.Reaction"),TEXT("Attack"),.08f,1);
		Result[30].Prerequisites.Append({FName(TEXT("Steel.Power")), FName(TEXT("Shadow.Power"))});
		Result[31].Prerequisites.Append({FName(TEXT("Steel.Wounds")), FName(TEXT("Shadow.Toxin"))});
		for (FMetaSkillNode& N : Result)
		{
			if (N.Effect == TEXT("Health")) N.SharedStat = ESharedPlayerStatType::MaxHealthMultiplier;
			if (N.Effect == TEXT("Pickup")) N.SharedStat = ESharedPlayerStatType::PickupRadiusMultiplier;
			if (N.Effect == TEXT("Move")) N.SharedStat = ESharedPlayerStatType::MoveSpeedMultiplier;
			if (N.Effect == TEXT("Attack")) N.SharedStat = ESharedPlayerStatType::AttackSpeedMultiplier;
			if (N.Effect == TEXT("Dash")) N.SharedStat = ESharedPlayerStatType::MaxDashCharges;
			if (N.Effect.ToString().StartsWith(TEXT("Steel"))) N.Target = EUpgradeStatTarget::Samurai;
			if (N.Effect.ToString().StartsWith(TEXT("Shadow")) || N.Effect == TEXT("Pierce") || N.Effect == TEXT("Projectile")) N.Target = EUpgradeStatTarget::Ninja;
			if (N.Effect == TEXT("SteelArea")) N.CharacterStat = ECharacterStatType::AttackAreaMultiplier;
			if (N.Effect == TEXT("SteelAttack") || N.Effect == TEXT("ShadowAttack")) N.CharacterStat = ECharacterStatType::AttackSpeedMultiplier;
			if (N.Effect == TEXT("ShadowFlight")) N.CharacterStat = ECharacterStatType::ProjectileSpeedMultiplier;
			if (N.Effect == TEXT("Pierce")) N.CharacterStat = ECharacterStatType::ProjectilePierceBonus;
			if (N.Effect == TEXT("Projectile")) N.CharacterStat = ECharacterStatType::ProjectileCountBonus;
		}
		return Result;
	}();
	return Catalog;
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
	return N && Rank(Save, Id) < N->MaxRank ? N->BaseCost * (Rank(Save, Id) + 1) : 0;
}
FString MetaSkillTree::PurchaseBlock(const UHeavensDivideMetaSaveGame& Save, FName Id)
{
	const FMetaSkillNode* N = Find(Id);
	if (!N) return TEXT("Unknown skill.");
	if (Rank(Save, Id) >= N->MaxRank) return TEXT("Fully learned.");
	for (FName Parent : N->Prerequisites)
		if (Rank(Save, Parent) == 0) return FString::Printf(TEXT("Learn %s first."), *Find(Parent)->Name);
	if (Save.SoulEmbers < Cost(Save, Id)) return TEXT("Earn more Soul Embers by completing runs.");
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
	return FMath::FloorToInt(FMath::Min(Seconds, 3600.f) / 30.f) + (bVictory ? 20 : 0);
}
