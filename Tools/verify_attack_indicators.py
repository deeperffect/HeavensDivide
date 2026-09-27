"""Render empty/half/full warnings and audit saved indicator assignments."""
from pathlib import Path
import json
import unreal as u
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
out=Path(u.Paths.project_saved_dir(),'AttackIndicators').resolve();out.mkdir(parents=True,exist_ok=True)
report={}
setup=json.loads((out/'setup.json').read_text())
for path in setup['blueprints']:
 bp=u.load_asset(path);cdo=u.get_default_object(bp.generated_class())
 if isinstance(cdo,u.TankMeleeEnemyBase):
  key='CircleDecal' if cdo.get_editor_property('attack_shape')==u.TankSlamAttackShape.CIRCLE else 'Rectangle'
  assert cdo.get_editor_property('attack_telegraph_material').get_path_name()==setup['materials'][key],path
 elif isinstance(cdo,u.FinalBossBase):
  assert cdo.get_editor_property('rectangle_telegraph_material').get_path_name()==setup['materials']['Rectangle'],path
  assert cdo.get_editor_property('circle_telegraph_material').get_path_name()==setup['materials']['CircleDecal'],path
for shape in ['Rectangle','Circle']:
 mat=u.load_asset('/Game/HeavensDivide/Materials/M_AttackIndicator'+shape)
 u.MaterialEditingLibrary.recompile_material(mat)
 mid=u.MaterialLibrary.create_dynamic_material_instance(world,mat)
 mid.set_scalar_parameter_value('LaneAspect',1)
 target=u.RenderingLibrary.create_render_target2d(world,256,256,u.TextureRenderTargetFormat.RTF_RGBA8)
 counts=[]
 for name,amount in [('empty',0.),('half',.5),('full',1.)]:
  mid.set_scalar_parameter_value('FillAmount',amount)
  u.RenderingLibrary.clear_render_target2d(world,target,u.LinearColor(.08,.10,.14,1))
  u.RenderingLibrary.draw_material_to_render_target(world,target,mid)
  pixels=u.RenderingLibrary.read_render_target(world,target,False)
  counts.append(sum(p.r>70 and p.r>p.g*2 and p.r>p.b*2 for p in pixels))
  u.RenderingLibrary.export_render_target(world,target,str(out),shape+'_'+name+'.png')
  if name=='empty': assert pixels[128*256+128].r<10, 'Empty warning must have a black center'
 assert counts[0]>100 and counts[1]>counts[0]+3000 and counts[2]>counts[1]+10000, (shape,counts)
 report[shape]=counts
registry=u.AssetRegistryHelpers.get_asset_registry();registry.wait_for_completion()
report['legacy_references']={}
for path in ['/Game/Assets/EnemyCharacters/M_AttackTelegraphBox','/Game/Assets/EnemyCharacters/M_AttackTelegraphCircle']:
 report['legacy_references'][path]=list(map(str,u.EditorAssetLibrary.find_package_referencers_for_asset(path,False)))
skeleton=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyFloatingSkeleton')
if skeleton:
 cdo=u.get_default_object(skeleton.generated_class());report['floating_skeleton_class']=cdo.get_class().get_path_name()
 for field in ['attack_telegraph_material','attack_shape']:
  try:report['floating_skeleton_'+field]=str(cdo.get_editor_property(field))
  except Exception:pass
(out/'verification.json').write_text(json.dumps(report,indent=2))
u.log('ATTACK_INDICATOR_RENDER_PASS '+json.dumps(report))
