import bpy, json
from pathlib import Path
from mathutils import Vector
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
ART=ROOT/'Art/Characters/Ninja/NinjaV6_Textured'
OUT=ROOT/'Saved/NinjaV6Replacement'
bpy.ops.wm.open_mainfile(filepath=str(ART/'Bloodshift_NinjaV6_Textured.blend'))
obj=bpy.data.objects['NinjaV6_ReferenceTextured']
groups=json.loads((OUT/'V6_Components.json').read_text())
ids=set(groups[15]['vertex_indices']+groups[24]['vertex_indices'])
assert len(ids)==253
body=obj.data.materials[0]
body.name='M_NinjaV6_Body'
scarf=body.copy();scarf.name='M_NinjaV6_ScarfRibbons'
obj.data.materials.append(scarf)
counts=[0,0]
for p in obj.data.polygons:
    p.material_index=int(all(i in ids for i in p.vertices))
    assert p.material_index or not any(i in ids for i in p.vertices)
    counts[p.material_index]+=1
obj['ScarfRibbons']='Material slot 1: only the two trailing neck scarf ribbons. Neck wrap remains in Body.'
(OUT/'ScarfSeparation.json').write_text(json.dumps({'vertices':253,'components':[15,24],'polygons_per_slot':counts,'materials':[m.name for m in obj.data.materials]},indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Bloodshift_NinjaV6_ScarfSeparated.blend'))
# Temporary diagnostic colors; never saved into the deliverable.
for mat,color in [(body,(.16,.16,.16,1)),(scarf,(.0,.8,1,1))]:
    nodes=mat.node_tree.nodes;nodes.clear()
    o=nodes.new('ShaderNodeOutputMaterial');s=nodes.new('ShaderNodeEmission')
    s.inputs['Color'].default_value=color
    mat.node_tree.links.new(s.outputs[0],o.inputs['Surface'])
scene=bpy.context.scene;scene.render.resolution_x=768;scene.render.resolution_y=1024;scene.cycles.samples=8
camera=scene.camera;target=Vector((0,0,.5))
camera.location=(1.8,3,.55);camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.ortho_scale=1.12
scene.render.filepath=str(OUT/'ScarfSlot_Diagnostic.png')
bpy.ops.render.render(write_still=True)
print('SCARF_SEPARATION_PASS',counts,flush=True)
