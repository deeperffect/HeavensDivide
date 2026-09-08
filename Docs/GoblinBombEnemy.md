# Goblin Bomb

`BP_EnemyGoblinBomb` inherits from `AGoblinBombEnemy`, which reuses the Tank/Floating Skeleton circle telegraph and damage calculation. Its attached Bomb static mesh is preserved.

The enemy chases normally, stops when the player enters Attack Range, fills the circle during its charge, then damages players still inside the circle, spawns its explosion and dies through the existing health/death flow. Rewards and enemy tracking use that same flow. Detonation removes the body and attached bomb immediately; the world-space Niagara effect continues independently.

## Editor setup

Open `BP_EnemyGoblinBomb` and select **Class Defaults**. Search for these properties:

| Property | Purpose / configured value |
| --- | --- |
| Explosion Niagara System | Assign your explosion Niagara System here; currently unassigned. Use a non-looping system that finishes so its pooled component can be released. |
| Explosion Spawn Offset | World-space offset from the actor location; adjust Z for the explosion's height. |
| Attack Range | Starts charging within 250 Unreal units. |
| Attack AoE Radius | Circle radius: 600 units, copied from Floating Skeleton. |
| Telegraph Windup Duration | Charge time: 1 second. |
| Attack Telegraph Material | Existing circle decal material copied from Floating Skeleton. |
| Attack Damage | Preserved at 10 before difficulty scaling and Slam Damage Multiplier. |
| Charge Wobble Degrees / Frequency | Visual mesh wobble during charging; defaults to 7 degrees at 7 cycles/second. Set degrees to zero to disable. |
| Bomb Flash Color | Red by default. |
| Bomb Flash Start / End Frequency | Blinking accelerates from 2 to 8 flashes/second across the charge. |
| Bomb Mesh Name | `Bomb`; matches the static mesh component name or a component tag. |
| Bomb Flash Material | Defaults to the opaque `M_BombChargeFlash` material, using `FlashColor`. No bomb material edits needed. |

Compile and save after assigning your Niagara system. No attack montage or animation notify is needed. Without an assigned Niagara system, damage and cleanup still occur, but no explosion is visible.

Leaving the circle avoids damage but does not cancel the fuse. Killing the goblin before it detonates cancels the charge and uses the standard EnemyDeathComponent presentation instead. Gameplay suspension also cancels the charge. Contact damage is disabled for this enemy.

While charging, movement stays stopped and the visible mesh wobbles without rotating the collision capsule or decal. Walking animation pauses during charging. The Bomb alternates between an opaque red material and its original materials, including on Nanite meshes. A 30 Hz timer runs only during charging; death, suspension and destruction clear it and restore the original mesh rotation, animation pause state and bomb materials.

The flash material is created/assigned by `Tools/configure_bomb_charge_flash.py`. The saved material passed the rendered on/off check in `Tools/verify_bomb_flash_render.py` (2,362 red pixels on, zero off). That headless capture uses the conventional mesh path; check the complete animated charge in PIE for final visual tuning.

## Implementation and validation

- Added `GoblinBombEnemy.h/.cpp` and its attack automation test.
- Made the shared melee attack entry point and Tank contact-damage policy overridable.
- Reparented and configured only `BP_EnemyGoblinBomb`; Floating Skeleton retains its existing behavior.
- `Tools/configure_goblin_bomb.py` performs the Blueprint migration and verifies the Bomb mesh and relative transform survive reparenting. The original asset backup is under `Saved/Backups/GoblinBombAttack`.
- UE Editor Development build passed. `HeavensDivide.Enemies.GoblinBomb` and `HeavensDivide.EnemyDeath.Lifecycle` passed, covering fuse timing, one-time damage, escaping the circle, early death, suspension and existing death cleanup. Explosion appearance needs an assigned asset and a play-in-editor check.

