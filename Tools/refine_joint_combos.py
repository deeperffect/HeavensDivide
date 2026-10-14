"""Tune the three combos and share character-colored smoke, preserving attack VFX edits."""
import json
import shutil
from pathlib import Path
import unreal as u

root=Path(u.Paths.project_dir())
out=root/'Saved/JointComboRefinement'
backup=out/'AssetBackups'
backup.mkdir(parents=True,exist_ok=True)

def preserve(path):
    disk=root/'Content'/Path(path.removeprefix('/Game/')+'.uasset')
    target=backup/disk.name
    if disk.exists() and not target.exists(): shutil.copy2(disk,target)

def smoke(name,color):
    path='/Game/HeavensDivide/VFX/Swap/'+name
    if u.EditorAssetLibrary.does_asset_exist(path): return u.load_asset(path)
    fx=u.EditorAssetLibrary.duplicate_asset('/Game/HeavensDivide/VFX/Combos/Joint/NS_JointHelperSmokeBurst',path)
    assert fx
    assert u.SwapVFXSetupLibrary.tint_smoke_system(fx,u.LinearColor(*color,1))
    assert u.EditorAssetLibrary.save_loaded_asset(fx,False)
    return fx

red=smoke('NS_SamuraiSmoke',(.4,.025,.045))
purple=smoke('NS_NinjaSmoke',(.23,.03,.45))
rows={}
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
for name,fx in [('BP_Samurai',red),('BP_Ninja',purple)]:
    path='/Game/HeavensDivide/Blueprints/PlayerCharacters/'+name
    preserve(path)
    bp=u.load_asset(path)
    objects=[]
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data=u.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data,bp)
        if obj: objects.append(obj)
    swap=next(x for x in objects if isinstance(x,u.SwapPresentationComponent))
    swap.set_editor_property('character_smoke_vfx',fx)
    swap.set_editor_property('character_smoke_scale',.125)
    swap.set_editor_property('character_smoke_seconds',.65)
    if name=='BP_Samurai':
        swap.set_editor_property('smoke_on_swap_arrival',True)
        # Scale the complete visual hierarchy around the planted mesh root.
        # Set an absolute target so rerunning never compounds the scale.
        visual=next(x for x in objects if isinstance(x,u.SceneComponent) and x.get_name()=='VisualRoot')
        mesh=next(x for x in objects if isinstance(x,u.SkeletalMeshComponent) and x.get_name()=='CharacterMesh0')
        old=visual.get_editor_property('relative_scale3d')
        loc=visual.get_editor_property('relative_location')
        foot=mesh.get_editor_property('relative_location')
        target=1.22
        visual.set_editor_property('relative_location',u.Vector(loc.x+foot.x*(old.x-target),loc.y+foot.y*(old.y-target),loc.z+foot.z*(old.z-target)))
        visual.set_editor_property('relative_scale3d',u.Vector(target,target,target))
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    c=u.get_default_object(bp.generated_class())
    rows[name]=dict(smoke=str(c.get_component_by_class(u.SwapPresentationComponent).get_editor_property('character_smoke_vfx')),scale=.125)
for identity in ('BloodMoon','HeavensDivide','Moonwell'):
    path='/Game/HeavensDivide/Abilities/Combos/DA_Combo_'+identity
    preserve(path)
    asset=u.load_asset(path)
    # Migrate only our old generic helper smoke. Custom artist overrides survive.
    for field in ('helper_arrival_vfx','helper_departure_vfx'):
        current=asset.get_editor_property(field)
        if current and current.get_path_name().startswith('/Game/HeavensDivide/VFX/Combos/Joint/NS_JointHelperSmokeBurst.'):
            asset.set_editor_property(field,None)
    asset.set_editor_property('helper_smoke_scale',.125)
    if identity=='HeavensDivide': asset.set_editor_property('windup',1.25)
    if identity=='Moonwell':
        asset.set_editor_property('wave_interval',.65)
        asset.set_editor_property('shuriken_size_multiplier',1.5)
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
    rows[identity]={k:str(asset.get_editor_property(k)) for k in ('windup','wave_interval','shuriken_size_multiplier','helper_smoke_scale','vfx','secondary_vfx')}
(out/'Assets.json').write_text(json.dumps(rows,indent=2))
print('Combo refinement: pacing, scale and character smoke saved.')
