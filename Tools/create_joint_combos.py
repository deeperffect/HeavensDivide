"""Replace three stable combo definitions with synchronized joint attacks; preserve original VFX."""
import json
import shutil
from pathlib import Path
import unreal as u

root=Path(u.Paths.project_dir())
backup=root/'Saved/JointCombos/AssetBackups'
backup.mkdir(parents=True,exist_ok=True)
tools=u.AssetToolsHelpers.get_asset_tools()
def component(hero,kind):
    cls=u.load_class(None,'/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+hero+'.BP_'+hero+'_C')
    return u.get_default_object(cls).get_component_by_class(kind)
sam=component('Samurai',u.AutoAttackComponent)
nin=component('Ninja',u.AutoAttackComponent)
build=component('Ninja',u.NinjaBuildComponent)
def copy_fx(name,source):
    path='/Game/HeavensDivide/VFX/Combos/Joint/'+name
    fx=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.EditorAssetLibrary.duplicate_asset(source,path)
    assert isinstance(fx,u.NiagaraSystem),source
    u.SwapVFXSetupLibrary.prepare_portal_local_space(fx)
    assert u.SwapVFXSetupLibrary.prepare_pickup_burst_scale(fx)
    assert u.SwapVFXSetupLibrary.configure_joint_combo_burst(fx)
    fx.set_editor_property('determinism',True);fx.set_editor_property('random_seed',95331+len(name))
    assert u.EditorAssetLibrary.save_loaded_asset(fx,False)
    return fx
v='/Game/Assets/VFX/'
charge=copy_fx('NS_JointChargeLightning',v+'VerticalBeamsV1/Particles/NiagaraSystems/NS_VerticalBeam18_Electric')
crimson=copy_fx('NS_CrimsonTempest',v+'CrossSlashesV1/Particles/NiagaraSystems/NS_CrossSlash08')
eclipse=copy_fx('NS_EclipseSeverance',v+'EzrealSlashesV1/Particles/NiagaraSystems/NS_EzrealSlash08')
lunar=copy_fx('NS_LunarInfusion','/Game/HeavensDivide/VFX/Stances/NS_FangSlash_360')
needles=copy_fx('NS_JointLightning',v+'VerticalBeamsV1/Particles/NiagaraSystems/NS_VerticalBeam18_Electric')
flash=copy_fx('NS_JointVolley','/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedFlash')
rows=[
 ('BloodMoon','Crimson Tempest','CRIMSON_TEMPEST','A Blood slash and arcing kunai volley. Blood upgrades apply to the slash; Barrage upgrades apply to the volley.','Blood + Barrage',crimson,flash,'attack_montage','attack_montage',280,1400,1.8,.65,9,4,3,.3,u.Rotator(),u.LinearColor(3,.12,.2,1)),
 ('HeavensDivide','Eclipse Severance','ECLIPSE_SEVERANCE','Ninja repeatedly throws lightning-fast Fangs into the charging Iaijutsu lanes, then Samurai dashes through the slash. Matching Iaijutsu and Fang upgrades empower their own half.','Iaijutsu + Returning Fang',eclipse,needles,'iaijutsu_montage','fang_montage',140,1500,1.8,1.25,1,4,3,.4,u.Rotator(pitch=0,yaw=90,roll=0),u.LinearColor(.3,1.2,3,1)),
 ('Moonwell','Lunar Tempest','LUNAR_TEMPEST','Ninja throws a Great Shuriken. Samurai charges it with a Crescent strike; it fires four Crescent slashes in a cross that rotates with the shuriken. Matching Shuriken and Crescent upgrades empower their own half.','Crescent + Great Shuriken',lunar,flash,'crescent_montage','attack_montage',150,1600,2.2,.65,4,2.5,4,.8,u.Rotator(),u.LinearColor(.5,.35,3,1)),
]
result=[]
for identity,name,kind,desc,hint,fx,secondary,sam_m,nin_m,radius,reach,duration,windup,count,sp,np,scale,rotation,color in rows:
    disk=root/'Content/HeavensDivide/Abilities/Combos'/('DA_Combo_'+identity+'.uasset')
    if disk.exists() and not (backup/disk.name).exists():shutil.copy2(disk,backup/disk.name)
    path='/Game/HeavensDivide/Abilities/Combos/DA_Combo_'+identity
    asset=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('DA_Combo_'+identity,'/Game/HeavensDivide/Abilities/Combos',u.ComboAbilityDefinition,u.DataAssetFactory())
    assert isinstance(asset,u.ComboAbilityDefinition),path
    sm=sam.get_editor_property(sam_m) or sam.get_editor_property('attack_montage')
    nm=nin.get_editor_property(nin_m) or nin.get_editor_property('attack_montage')
    assert sm and nm, 'Both heroes need compatible saved montages'
    properties=dict(ability_id=identity,display_name=name,description=desc,stance_hint=hint,character=u.UpgradeStatTarget.SHARED_PLAYER,
      technique=getattr(u.ComboTechnique,kind),radius=radius,range=reach,duration=duration,windup=windup,count=count,samurai_power=sp,ninja_power=np,area_multiplier=1.6,wave_interval=.65,fang_speed_multiplier=4,release_recovery=.42,fang_throw_interval=.14,cinematic_world_speed=.1,
      vfx=fx,secondary_vfx=secondary,charge_vfx=charge,vfx_scale=scale,vfx_rotation=rotation,color=color,vfx_seconds=.6 if identity=='HeavensDivide' else .45 if identity=='Moonwell' else .55,
      samurai_montage=sm,ninja_montage=nin.get_editor_property('attack_montage') if identity=='HeavensDivide' else nm,ninja_alternate_montage=nin.get_editor_property('alternate_attack_montage'),
      secondary_vfx_scale=.18 if identity=='HeavensDivide' else .45,secondary_vfx_seconds=.12 if identity=='HeavensDivide' else .3,secondary_vfx_color=u.LinearColor(.3,.5,3,1) if identity=='HeavensDivide' else u.LinearColor(.15,2,.7,1),weapon_mesh=build.get_editor_property('shuriken_mesh'),
      release_sound=u.load_asset('/Game/HeavensDivide/Audio/MetaSounds/'+({'BloodMoon':'MS_SamuraiSwing','HeavensDivide':'MS_SamuraiAbility','Moonwell':'MS_ShurikenThrow'}[identity])))
    for key,value in properties.items():asset.set_editor_property(key,value)
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
    result.append(dict(id=identity,name=name,asset=path,samurai_montage=sm.get_path_name(),ninja_montage=nm.get_path_name(),vfx=fx.get_path_name()))
(root/'Saved/JointCombos/Assets.json').write_text(json.dumps(result,indent=2))
print('Joint combos saved: '+str(len(result)))
