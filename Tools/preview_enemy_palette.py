import bpy,json,sys,math
from pathlib import Path
from mathutils import Vector
ROOT=Path("C:/Users/deepe/OneDrive/Documents/Unreal Projects/HeavensDivide");OUT=ROOT/'Saved/BearPatchwork'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=1;scene.render.resolution_x=720;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='Standard';scene.view_settings.look='None';scene.view_settings.exposure=0;scene.view_settings.gamma=1
world=bpy.data.worlds.new('Charcoal');scene.world=world;world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.032,.037,.035,1)
camdata=bpy.data.cameras.new('Camera');camera=bpy.data.objects.new('Camera',camdata);scene.collection.objects.link(camera);scene.camera=camera;camdata.type='ORTHO'

def palette_nodes(nodes,links,source,fabric):
 def math(op,a,b=None):
  n=nodes.new('ShaderNodeMath');n.operation=op
  for i,x in enumerate([a] if b is None else [a,b]):
   if isinstance(x,(int,float)):n.inputs[i].default_value=x
   else:links.new(x,n.inputs[i])
  return n.outputs[0]
 def add(a,b):return math('ADD',a,b)
 def sub(a,b):return math('SUBTRACT',a,b)
 def mul(a,b):return math('MULTIPLY',a,b)
 def div(a,b):return math('DIVIDE',a,b)
 def maximum(a,b):return math('MAXIMUM',a,b)
 def minimum(a,b):return math('MINIMUM',a,b)
 def power(a,b):return math('POWER',a,b)
 def clamp(x,a=0,b=1):return minimum(b,maximum(a,x))
 def mix(a,b,t):return add(mul(a,sub(1,t)),mul(b,t))
 def smooth(a,b,x):
  t=clamp(div(sub(x,a),b-a));return mul(mul(t,t),sub(3,mul(2,t)))
 def split(inp):
  n=nodes.new('ShaderNodeSeparateColor');n.mode='RGB';links.new(inp,n.inputs[0]);return [power(maximum(n.outputs[i],.000001),1/2.2) for i in range(3)]
 c=split(source);d=split(fabric);r,g,b=c;v=maximum(r,maximum(g,b));sat=div(sub(v,minimum(r,minimum(g,b))),maximum(v,.00001))
 geometry=nodes.new('ShaderNodeNewGeometry');splitpos=nodes.new('ShaderNodeSeparateXYZ');links.new(geometry.outputs['Position'],splitpos.inputs[0]);x=math('ABSOLUTE',splitpos.outputs[0]);y=mul(splitpos.outputs[1],-1);z=splitpos.outputs[2]
 shoulders=mul(mul(smooth(.12,.17,x),smooth(.50,.54,z)),sub(1,smooth(.75,.79,z)))
 muzzle=mul(mul(mul(sub(1,smooth(.092,.112,x)),smooth(.61,.635,z)),sub(1,smooth(.775,.8,z))),smooth(.10,.13,y))
 warmHue=mul(smooth(.07,.16,sub(d[0],d[1])),smooth(.035,.09,sub(d[1],d[2])));warm=mul(maximum(shoulders,muzzle),warmHue)
 mauve=mul(mul(smooth(.07,.16,sub(d[0],d[1])),sub(1,smooth(.02,.07,sub(d[1],d[2])))),sub(1,warm));green=maximum(0,sub(sub(1,mauve),warm));pale=smooth(.28,.48,v)
 detail=mix(1,clamp(div(add(add(mul(d[0],.2126),mul(d[1],.7152)),mul(d[2],.0722)),maximum(v,.05)),.85,1.15),.18)
 protect=maximum(sub(1,smooth(.07,.16,v)),mul(smooth(.64,.85,v),sub(1,smooth(.25,.55,sat))))
 combine=nodes.new('ShaderNodeCombineColor');combine.mode='RGB'
 for i,(teal,rust,warm0,warm1) in enumerate(zip([.5,.93,1],[1,.58,.68],[1,.62,.32],[1,.88,.66])):
  out=mul(v,add(add(mul(green,1.45*teal),mul(mauve,1.35*rust)),mul(mul(warm,1.58),mix(warm0,warm1,pale))))
  out=power(clamp(mix(mul(out,detail),c[i],protect)),2.2);links.new(out,combine.inputs[i])
 return combine.outputs[0]

report={}
for label in ['Bear','Crawler']:
 datafile=OUT/(label+'_Geometry.json')
 if not datafile.exists():continue
 data=json.loads(datafile.read_text());verts=[];uvs=[];faces=[]
 for t in data:
  start=len(verts);verts.extend([(p[0]*.01,-p[1]*.01,p[2]*.01) for p in t['p']]);uvs.extend([(p[0],1-p[1]) for p in t['uv']]);faces.append((start,start+2,start+1))
 mesh=bpy.data.meshes.new(label);mesh.from_pydata(verts,[],faces);mesh.update();layer=mesh.uv_layers.new(name='UVMap')
 for loop in mesh.loops:layer.data[loop.index].uv=uvs[loop.vertex_index]
 obj=bpy.data.objects.new(label,mesh);scene.collection.objects.link(obj)
 mat=bpy.data.materials.new(label+'_Preview');mat.use_nodes=True;nodes=mat.node_tree.nodes;nodes.clear();tex=nodes.new('ShaderNodeTexImage');tex.interpolation='Linear';shader=nodes.new('ShaderNodeEmission');output=nodes.new('ShaderNodeOutputMaterial');mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Color']);mat.node_tree.links.new(shader.outputs[0],output.inputs[0]);mesh.materials.append(mat)
 low=Vector(tuple(min(p[i] for p in verts) for i in range(3)));high=Vector(tuple(max(p[i] for p in verts) for i in range(3)));center=(low+high)*.5;size=max(high.z-low.z,(high.x-low.x)*1.25)*1.12;camdata.ortho_scale=size
 paths=[('Patchwork',OUT/'Bear_Original_BaseColor.png')] if label=='Bear' else [('Subtle',OUT/'Crawler_Enemy_Goblin_base.png')]
 report[label]={'triangles':len(data),'bounds':[list(low),list(high)]}
 for variant,p in paths:
  tex.image=bpy.data.images.load(str(p),check_existing=True)
  if label=='Bear':
   fabric=nodes.new('ShaderNodeTexImage');fabric.image=bpy.data.images.load(str(ROOT/'Art/Enemies/Bear/Patchwork/T_Bear_Patchwork_Refined_2K.png'));color=palette_nodes(nodes,mat.node_tree.links,tex.outputs['Color'],fabric.outputs['Color']);mat.node_tree.links.new(color,shader.inputs['Color'])
  else:
   links=mat.node_tree.links;sep=nodes.new('ShaderNodeSeparateColor');links.new(tex.outputs['Color'],sep.inputs[0]);weighted=[]
   for i,w in enumerate([.3,.59,.11]):
    m=nodes.new('ShaderNodeMath');m.operation='MULTIPLY';links.new(sep.outputs[i],m.inputs[0]);m.inputs[1].default_value=w;weighted.append(m.outputs[0])
   a=nodes.new('ShaderNodeMath');a.operation='ADD';links.new(weighted[0],a.inputs[0]);links.new(weighted[1],a.inputs[1]);b=nodes.new('ShaderNodeMath');b.operation='ADD';links.new(a.outputs[0],b.inputs[0]);links.new(weighted[2],b.inputs[1]);gray=nodes.new('ShaderNodeMath');gray.operation='MULTIPLY';gray.inputs[1].default_value=.2;links.new(b.outputs[0],gray.inputs[0])
   v=nodes.new('ShaderNodeVectorMath');v.operation='SCALE';v.inputs['Scale'].default_value=1.2;links.new(tex.outputs['Color'],v.inputs[0]);sub=nodes.new('ShaderNodeVectorMath');sub.operation='SUBTRACT';links.new(v.outputs[0],sub.inputs[0]);links.new(gray.outputs[0],sub.inputs[1]);bright=nodes.new('ShaderNodeVectorMath');bright.operation='SCALE';bright.inputs['Scale'].default_value=1.04;links.new(sub.outputs[0],bright.inputs[0]);links.new(bright.outputs[0],shader.inputs['Color'])

  for view,direction in [('Front',(0,-1,.03)),('Back',(0,1,.03)),('ThreeQuarter',(.6,-1,.08))]:
   camera.location=center+Vector(direction)*size*3;camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(OUT/(label+'_'+variant+'_'+view+'.png'));bpy.ops.render.render(write_still=True)
 obj.hide_render=True
(OUT/'BlenderPreview.json').write_text(json.dumps(report,indent=2))
print('ENEMY_MATERIAL_PREVIEW_COMPLETE',flush=True)
