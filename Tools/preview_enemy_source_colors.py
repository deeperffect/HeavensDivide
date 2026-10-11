import bpy,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide');OUT=ROOT/'Saved/EnemyModelReplacement'
for name in ['AshSeer','OgreWarden','HexSniper']:
    bpy.ops.wm.open_mainfile(filepath=str(OUT/(name+'_Source.blend')))
    scene=bpy.context.scene
    for o in scene.objects:
        if o.type=='LIGHT':o.data.energy*=.07
    scene.view_settings.view_transform='AgX'
    obj=next(o for o in scene.objects if o.type=='MESH')
    if name=='HexSniper':obj.matrix_world=Matrix.Rotation(-math.atan2(obj.matrix_world[1][0],obj.matrix_world[0][0]),4,'Z')@obj.matrix_world
    scene.camera.location=(0,-3,.5);scene.camera.rotation_euler=(Vector((0,0,.5))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/(name+'_ColorFront.png'));bpy.ops.render.render(write_still=True)
    scene.camera.location=(3,0,.5);scene.camera.rotation_euler=(Vector((0,0,.5))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/(name+'_ColorSide.png'));bpy.ops.render.render(write_still=True)
