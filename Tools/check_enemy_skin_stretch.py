import bpy,json,os
from pathlib import Path
from mathutils import Matrix
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide');report={}
for name in os.environ.get('BLOODSHIFT_ENEMY_NAMES','AshSeer,OgreWarden,HexSniper').split(','):
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Art/Enemies'/name/('Bloodshift_'+name+'_Rigged.blend')))
    arm=bpy.data.objects['Armature'];obj=bpy.data.objects['SK_'+name];arm.animation_data.action=bpy.data.actions[name+'_Cast'];bpy.context.scene.frame_set(round(arm.animation_data.action.frame_range[1]*.65))
    ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=ev.to_mesh();bad=[]
    for e in obj.data.edges:
        a,b=e.vertices;before=(obj.data.vertices[a].co-obj.data.vertices[b].co).length;after=(mesh.vertices[a].co-mesh.vertices[b].co).length
        if after>.08 and after/max(before,.001)>4:
            bad.append({'ratio':after/max(before,.001),'after':after,'v':[{'id':i,'rest':list(obj.data.vertices[i].co),'weights':[(obj.vertex_groups[g.group].name,g.weight) for g in obj.data.vertices[i].groups]} for i in [a,b]]})
    report[name]={'bad_edges':len(bad),'worst':sorted(bad,key=lambda b:-b['ratio'])[:15]};ev.to_mesh_clear()
(ROOT/'Saved'/os.environ.get('BLOODSHIFT_ENEMY_WORKSPACE','EnemyModelReplacement')/'SkinStretch.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report),flush=True)
