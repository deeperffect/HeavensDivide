import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
OUT=ROOT/'Saved/NinjaV6Replacement'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(OUT/'CurrentNinja_Rig.fbx'))
report=[]
for o in bpy.context.scene.objects:
    d={'name':o.name,'type':o.type,'matrix':[list(r) for r in o.matrix_world]}
    if o.type=='MESH':
        vs=[o.matrix_world@v.co for v in o.data.vertices]
        d.update({'bounds':[[min(v[i] for v in vs) for i in range(3)],[max(v[i] for v in vs) for i in range(3)]],'vertices':len(vs),'groups':[g.name for g in o.vertex_groups]})
        d['regions']={}
        for name in ['Head','Spine2','Spine','LeftHand','RightHand','LeftFoot','RightFoot']:
            g=o.vertex_groups.get(name)
            pts=[o.matrix_world@v.co for v in o.data.vertices if g and any(w.group==g.index and w.weight>.7 for w in v.groups)]
            if pts:d['regions'][name]={'min':[min(p[i] for p in pts) for i in range(3)],'max':[max(p[i] for p in pts) for i in range(3)]}
    if o.type=='ARMATURE':
        d['bones']=[{'name':b.name,'head':list(o.matrix_world@b.head_local),'tail':list(o.matrix_world@b.tail_local),'parent':b.parent.name if b.parent else None} for b in o.data.bones]
    report.append(d)
(OUT/'BlenderRig.json').write_text(json.dumps(report,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CurrentNinja_Rig.blend'))
print('RIG_IMPORT_PASS',json.dumps(report)[:2200],flush=True)
