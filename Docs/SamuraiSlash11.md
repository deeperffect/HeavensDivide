# Samurai autoattack slash

The current burst uses `NS_Slash_10` through the same area-aware notify.
`Tools/configure_samurai_slash10.py` changes only the Niagara System assignment;
all scaling, offsets, attachment and timing are preserved. The previous montage
is backed up in `Saved/Backups/SamuraiSlash10`.

In `AM_AutoAttackSamurai`, select the **SpawnSamuraiSlashNiagara** notify
(around 0.189 seconds). Its Niagara section exposes:

- **Location Offset**: local translation from the character mesh or selected socket.
- **Rotation Offset**: local rotation adjustment.
- **Scale**: base size before Area bonuses.
- **Area Bonus Scale Multiplier**: 1 matches Area; 2 doubles only the bonus
  (1.5x Area becomes 2x VFX size).
- **Skeleton Socket Name**: optional attachment socket; None uses the mesh origin.

Keep **Attach To Character Mesh** enabled for this burst. The component scale is
set before activation and inherited by the local-space slash emitters. Do not also
add `User.Scale` to Area Scaled Float Parameters: that would apply Area twice.
Offsets remain authored distances as Area changes. The separate sword trail has
its own notify and tuning.

`Tools/configure_samurai_slash11.py` migrates the standard Niagara notify while
preserving event timing, track, offsets, rotation and base scale. A pre-migration
montage backup is kept in `Saved/Backups/SamuraiSlash11`.
