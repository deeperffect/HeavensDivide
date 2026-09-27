# Double Cut circular follow-up

Double Cut's follow-up centers its melee sphere on the Samurai instead of using
the normal forward offset. Its radius is still `AttackRadius * AttackAreaMultiplier`.
Damage, proc cadence, secondary-target damage, pushback and status effects use
the existing combat rules. Normal attacks retain their forward hitbox.

`AM_DoubleCutSamurai` uses the autoattack's `NS_Slash_10` burst and scale settings,
with **Radial Copies = 4** on its SpawnSamuraiSlashNiagara notify. Four identical
arcs, spaced 90 degrees apart, form the circular effect at the same size.
The notify is mesh-attached at the character center and starts on the damage
trace frame. Its vertical offset comes from the autoattack notify. This replaces
the saved unassigned weapon-tip Niagara state; other montage events are kept.

Setup: `Tools/configure_double_cut_360.py`; original montage backup:
`Saved/Backups/DoubleCut360`. Test: `HeavensDivide.Combat.DoubleCut360`.
