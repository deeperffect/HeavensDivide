# Samurai memory-trial lane indicator

The missing `M_SamuraiLaneIndicator` material is restored and assigned through
`MI_SamuraiTrialIndicator` on `BP_SamuraiTechniqueTrial`.

The flat lane planes show a nearly black rectangle with an animated
crimson ink edge. Red advances along the lane as `FillAmount` goes from 0 to 1.
The shader is procedural and needs no textures. It is a surface material for
the lane meshes, not a Deferred Decal material.

**FillColor** is inherited from the assigned material or material instance; the
trial changes only **FillAmount** and **LaneAspect** at runtime. The wavy crimson
border has its own fixed shader color. Straight red strike debug rectangles are
disabled by default; they can be enabled explicitly under **Samurai Trial > Debug >
Draw Strike Damage Debug Boxes**.
The indicator uses a flat plane rather than a thin cube, preventing the material
from drawing an extra straight perimeter on vertical side faces.

The two unsafe lanes fill during each memory preview over **Preview Display
Duration**, under **Samurai Trial > Memory**. Indicators stay hidden during
execution, preserving the memory challenge. The native trial now enables actor
ticking so the existing fill animation advances. Lane aspect ratio comes from
each lane component's scale so the border stays proportional.

In the Blueprint viewport, select **LeftLane**, **CenterLane**, or **RightLane**
and adjust its **Transform > Location / Rotation / Scale**. Each lane has a
colored wireframe box (cyan, yellow, purple) that follows these edits even while
the indicator mesh is hidden. Scale X changes width and Scale Y changes length;
keep the existing thin Z scale for the floor indicator. The boxes match the
100-unit local cube used for the safe-area check. They are editor-only helpers,
hidden during play and excluded from cooked content. Edit the lane itself, not
its locked `LaneBounds` child. Strike VFX remain separately positioned components.

`Tools/create_samurai_lane_indicator.py` recreates the material and repairs the
material-instance parent and Blueprint assignment. Existing assets are backed
up once under `Saved/Backups/SamuraiLaneIndicator` before modification.
`Tools/verify_samurai_lane_indicator.py` checks the saved Blueprint's tick flags
and renders empty, half-filled and full states under `Saved/SamuraiLaneIndicator`.
Run both with Unreal's Python commandlet; rendering verification requires
`-AllowCommandletRendering`.
