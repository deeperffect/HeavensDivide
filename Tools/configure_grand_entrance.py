"""Create Grand Entrance, import its illustration and update the saved upgrade pool.

Use -ValidateGrandEntrance to verify saved assets without modifying them.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
job=json.loads((root/'Art/UpgradeCards/GrandEntrance.json').read_text())
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate='-ValidateGrandEntrance' in unreal.SystemLibrary.get_command_line()
values={'SamuraiRadius':600.,'NinjaBonusProjectiles':8.,'NinjaFanAngle':100.}
description='After swapping, your next normal attack is enhanced: Samurai unleashes a 360-degree slash with 600 cm base radius and full attack damage to every target; Ninja fires 8 extra projectiles in a 100-degree fan. Samurai radius scales with attack area. Assists and abilities do not spend the enhancement. Does not stack.'

if not validate:
    backup=root/'Saved/Backups/GrandEntrance'/datetime.now().strftime('%Y%m%d_%H%M%S')
    for path in [controller,job['asset'],job['texture']]:
        relative=Path('Content')/(path.removeprefix('/Game/')+'.uasset')
        if (root/relative).exists():
            (backup/relative).parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(root/relative,backup/relative)
    asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
    texture=unreal.load_asset(job['texture']) if unreal.EditorAssetLibrary.does_asset_exist(job['texture']) else None
    if not texture:
        task=unreal.AssetImportTask()
        task.filename=str(root/job['file'])
        task.destination_path,task.destination_name=job['texture'].rsplit('/',1)
        task.automated=True;task.replace_existing=False;task.save=True
        asset_tools.import_asset_tasks([task])
        texture=unreal.load_asset(job['texture'])
    assert isinstance(texture,unreal.Texture2D)
    for key,value in dict(srgb=True,lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,max_texture_size=512).items():
        texture.set_editor_property(key,value)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture,False)
    card=unreal.load_asset(job['asset']) if unreal.EditorAssetLibrary.does_asset_exist(job['asset']) else None
    if not card:
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
        folder,name=job['asset'].rsplit('/',1)
        card=asset_tools.create_asset(name,folder,unreal.UpgradeDefinition,factory)
    for key,value in dict(upgrade_id='GrandEntrance',display_name='Grand Entrance',description=description,category=unreal.UpgradeCategory.SYNERGY,investment_owner=unreal.UpgradeInvestmentOwner.NONE,role=unreal.UpgradeRole.SPECIAL,build_family_id='None',requires_meta_unlock=False,unlocked_by_default=True,max_level=1,rarity=unreal.UpgradeRarity.EPIC,prerequisite_upgrade_ids=[],prerequisite_requirements=[],stat_modifiers=[],special_effects=[],card_artwork=texture,icon=texture).items():
        card.set_editor_property(key,value)
    current=dict(card.get_editor_property('balance_parameters'))
    existing={str(k) for k in current}
    for key,value in values.items():
        if key not in existing:current[key]=value
    card.set_editor_property('balance_parameters',current)
    for field in ['bHasRuntimeBalance','bHasRuntimePresentation']:
        unreal.SystemLibrary.execute_console_command(None,'setnopec '+card.get_path_name()+' '+field+' True')
    assert unreal.EditorAssetLibrary.save_loaded_asset(card,False)
    bp=unreal.load_asset(controller)
    component=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
    pool=[a for a in component.get_editor_property('upgrade_pool') if a and str(a.get_editor_property('upgrade_id'))!='GrandEntrance']
    pool.append(card);component.set_editor_property('upgrade_pool',pool)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)

card=unreal.load_asset(job['asset']);texture=unreal.load_asset(job['texture'])
assert card and texture
assert str(card.get_editor_property('upgrade_id'))=='GrandEntrance'
assert card.get_editor_property('category')==unreal.UpgradeCategory.SYNERGY
assert card.get_editor_property('max_level')==1
assert not card.get_editor_property('requires_meta_unlock')
assert card.get_editor_property('card_artwork')==texture and card.get_editor_property('icon')==texture
assert texture.get_editor_property('max_texture_size')==512
assert set(values).issubset(str(k) for k in card.get_editor_property('balance_parameters'))
assert card.get_editor_property('has_runtime_balance')
bp=unreal.load_asset(controller)
pool=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
ids=[str(a.get_editor_property('upgrade_id')) for a in pool if a]
assert len(ids)==len(pool)==len(set(ids)) and ids.count('GrandEntrance')==1
unreal.log('GRAND_ENTRANCE_ASSETS_OK: saved synergy, runtime tuning, artwork and '+str(len(pool))+' unique pool entries verified')
