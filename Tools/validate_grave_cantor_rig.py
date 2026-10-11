"""Check the complete authored motion range, skin weights and locomotion loops."""
import bpy, json
from pathlib import Path
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
OUT=ROOT/'Saved/GraveCantorReplacement'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Art/Enemies/GraveCantor/Bloodshift_GraveCantor_Rigged.blend'))
arm=bpy.data.objects['Armature'];obj=bpy.data.objects['SK_GraveCantor'];scene=bpy.context.scene
assert all(v.groups and len(v.groups)<=4 and abs(sum(g.weight for g in v.groups)-1)<.0001 for v in obj.data.vertices)
report={'weighted_vertices':len(obj.data.vertices),'max_influences':4,'clips':{}}
def vertices():
    evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
    points=[v.co.copy() for v in mesh.vertices];evaluated.to_mesh_clear();return points
for label in ['Idle','Walk','Cast']:
    action=bpy.data.actions['GraveCantor_'+label];arm.animation_data.action=action
    start,end=action.frame_range;bad=[]
    for fraction in [0,.15,.3,.45,.6,.75,.9,1]:
        scene.frame_set(round(start+(end-start)*fraction));points=vertices()
        for edge in obj.data.edges:
            a,b=edge.vertices
            rest=(obj.data.vertices[a].co-obj.data.vertices[b].co).length
            posed=(points[a]-points[b]).length
            if posed>.08 and posed/max(rest,.001)>4:bad.append((scene.frame_current,edge.index,posed))
    assert not bad,(label,'Excessive mesh stretching',bad[:10])
    entry={'samples':8,'excessive_stretch_edges':0,'seconds':(end-start)/scene.render.fps}
    if label!='Cast':
        scene.frame_set(round(start));first=vertices();scene.frame_set(round(end));last=vertices()
        seam=max((a-b).length for a,b in zip(first,last))
        assert seam<.00001,(label,'Loop seam',seam)
        entry['max_loop_seam_meters']=seam
    report['clips'][label]=entry
(OUT/'RigValidation.json').write_text(json.dumps(report,indent=2))
print('GRAVE_CANTOR_RIG_VALIDATED',json.dumps(report),flush=True)
