import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
OUT=ROOT/'Saved/EnemyModelReplacement'
report={}
for name in ['AshSeer','OgreWarden','HexSniper']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(OUT/(name+'_Donor.fbx')))
    objs=list(bpy.context.scene.objects)
    report[name]=[]
    for o in objs:
        d={'name':o.name,'type':o.type,'matrix':[list(r) for r in o.matrix_world]}
        if o.type=='ARMATURE':
            d['bones']=[{'name':b.name,'parent':b.parent.name if b.parent else None,'head':list(o.matrix_world@b.head_local),'tail':list(o.matrix_world@b.tail_local)} for b in o.data.bones]
        if o.type=='MESH':
            points=[o.matrix_world@v.co for v in o.data.vertices]
            d['bounds']=[[min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)]]
            d['vertices']=len(points)
        report[name].append(d)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'_Donor.blend')))
(OUT/'DonorBlender.json').write_text(json.dumps(report,indent=2))
print('DONOR_BLENDER_COMPLETE',flush=True)
