Bloodshift enemy model replacements

Sources: the user's Desktop/Enemies/AshSeer, OgreWarden and HexSniper FBX
models and base-color textures. Original desktop files are unchanged.

Editable assets
Each named folder contains Bloodshift_<name>_Rigged.blend, SK_<name>.fbx,
the base-color image, and three FBX clips: Idle, Walk and Cast.
The Blender files include the fitted skeleton, skin weights, packed texture
and all three actions. Meshes face -X and measure approximately 1 m tall.
The game component rotates yaw 180 degrees and supplies the gameplay scale.

Unreal assets
/Game/Assets/EnemyCharacters/AshSeer
/Game/Assets/EnemyCharacters/OgreWarden
/Game/Assets/EnemyCharacters/HexSniper
Each contains the mesh, skeleton, material, animation Blueprint, textures,
three animation sequences and an attack montage. Three mesh LODs, a single
material section per LOD and at most four skin influences keep these suitable
for crowds. The supplied base-color texture is capped at 2048 pixels.
Materials support the existing DissolveAmount death effect.

The replacements are assigned only to BP_AshSeer, BP_OgreWarden and
BP_HexSniper under /Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical.
Existing combat tuning, collision capsules, spawners and attack indicators
are retained. Cast animations match the default ability windups; later
balance changes to windup may warrant retiming the corresponding montage.

Re-export/import workflow
1. Edit the named Blender files and their Idle/Walk/Cast actions.
2. Run Tools/export_final_enemy_fbx.py in Blender background mode. This
   restores transforms between exports to avoid Blender FBX unit changes.
3. Run Tools/import_desktop_enemies.py with Unreal's Python commandlet.
   It also bakes the FBX armature basis into the imported root animation
   tracks, generates LODs, and sets up the materials/animation Blueprints.
   Direct animation reimport alone bypasses this required root correction.
4. Run Tools/verify_desktop_enemy_render.py with AllowCommandletRendering.
5. Run Tools/activate_desktop_enemy_models.py to assign the validated assets.

Validation reports and rendered poses are in Saved/EnemyModelReplacement.
Pre-replacement tactical Blueprint backups are under its Backup folder.
