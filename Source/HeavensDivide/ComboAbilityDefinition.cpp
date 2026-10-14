#include "ComboAbilityDefinition.h"

const TArray<ComboCatalog::FEntry>& ComboCatalog::Entries()
{
    // Stable IDs preserve purchases of the three replaced techniques.
    static const TArray<FEntry> Catalog = {
        {TEXT("BloodMoon"),TEXT("Crimson Tempest"),true,TEXT("Samurai's Blood slash and Ninja's arcing kunai volley. Matching Blood and Barrage upgrades empower their own half."),TEXT("Blood + Barrage")},
        {TEXT("HeavensDivide"),TEXT("Eclipse Severance"),true,TEXT("Ninja throws lightning-fast Fangs into the charging Iaijutsu lanes, then Samurai dashes through the slash. Matching Iaijutsu and Fang upgrades empower their own half."),TEXT("Iaijutsu + Returning Fang")},
        {TEXT("Moonwell"),TEXT("Lunar Tempest"),true,TEXT("Samurai infuses a giant shuriken with Crescent energy. It fires four Crescent slashes in a cross that rotates with the shuriken. Matching Shuriken and Crescent upgrades empower their own half."),TEXT("Crescent + Great Shuriken")}
    };
    return Catalog;
}
const ComboCatalog::FEntry* ComboCatalog::Find(FName Id)
{ return Entries().FindByPredicate([Id](const FEntry& Entry){return Entry.Id == Id;}); }
FName ComboCatalog::UnlockId(FName Id)
{ return Find(Id) ? FName(*FString::Printf(TEXT("Ability.Combo.%s"), *Id.ToString())) : NAME_None; }
UComboAbilityDefinition* ComboCatalog::Definition(FName Id)
{
    if (!Find(Id)) return nullptr;
    return LoadObject<UComboAbilityDefinition>(nullptr, *FString::Printf(TEXT("/Game/HeavensDivide/Abilities/Combos/DA_Combo_%s.DA_Combo_%s"), *Id.ToString(), *Id.ToString()));
}
