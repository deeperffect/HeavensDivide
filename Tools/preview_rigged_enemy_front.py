import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide');OUT=ROOT/'Saved/EnemyModelReplacement'
for name in ['AshSeer','OgreWarden','HexSniper']:
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Art/Enemies'/name/('Bloodshift_'+name+'_Rigged.blend')))
    arm=bpy.data.objects['Armature'];scene=bpy.context.scene
    for label in ['Idle','Walk','Cast']:
        arm.animation_data.action=bpy.data.actions[name+'_'+label]
        scene.frame_set(int(arm.animation_data.action.frame_range[1]*.4))
        center=Vector((0,0,.50));camera=scene.camera;camera.location=(.15,-3,.65);camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(OUT/(name+'_'+label+'_FrontAnimated.png'));bpy.ops.render.render(write_still=True)
