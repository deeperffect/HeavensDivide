# Upgrade balance and VFX editing

Automatic ability families are retired. Edit the retained Blade Wave starter and branch assets under `/Game/HeavensDivide/Upgrades/Samurai`, or Grand Entrance under `/Game/HeavensDivide/Upgrades/Synergy`. Runtime Balance controls their parameters and Runtime VFX controls presentation. `Tools/upgrade_tuning_defaults.json` contains only retained upgrade defaults.

Blade Wave frequency follows normal melee attacks. Its Velocity card changes projectile travel speed. Grand Entrance exposes `SamuraiRadius`, `NinjaBonusProjectiles`, and `NinjaFanAngle`.

Prepare duration, bonus multiplier and spread radius remain on the SurvivorAbility component. See [Build families](BuildFamilies.md) for behavior and [Upgrade artwork](UpgradeArtwork.md) for illustrations.

Samurai stances use three stat modifiers with the Multiply operation (1.35 = +35%; 0.65 = -35%). Their exclusivity group is SamuraiStance. Bleeding Edge exposes HitDamagePerTick, Blood Transfer exposes TransferFraction/Radius/Targets, Overkill Burst exposes Radius/DamageMultiplier, and Wave Volley exposes AnglePerWave. See `Tools/samurai_build_upgrades.json` for initial values.
