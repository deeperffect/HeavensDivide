"""Assign editable impact defaults without replacing already-authored feedback assets."""
import os
import shutil
import unreal

ROOT = '/Game/HeavensDivide/Blueprints/PlayerCharacters/'
backup = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'ImpactFeedback')
os.makedirs(backup, exist_ok=True)

def load_bp(name):
    bp = unreal.load_asset(ROOT + name)
    assert bp, name
    filename = os.path.join(unreal.Paths.project_content_dir(), 'HeavensDivide', 'Blueprints', 'PlayerCharacters', name + '.uasset')
    target = os.path.join(backup, name + '.uasset')
    if not os.path.exists(target):
        shutil.copy2(filename, target)
    return bp

hit = unreal.load_asset('/Game/Assets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_Hit_Basic_Once')
assert hit
shake_path = '/Game/HeavensDivide/Blueprints/Feedback/BP_CS_SamuraiImpact'
shake = unreal.load_asset(shake_path) if unreal.EditorAssetLibrary.does_asset_exist(shake_path) else None
if not shake:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.SamuraiImpactCameraShake)
    shake = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_CS_SamuraiImpact', '/Game/HeavensDivide/Blueprints/Feedback', unreal.Blueprint, factory)
    assert shake
    unreal.BlueprintEditorLibrary.compile_blueprint(shake)
    assert unreal.EditorAssetLibrary.save_loaded_asset(shake, False)
samurai = load_bp('BP_Samurai')
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
attack = None
for handle in subsystem.k2_gather_subobject_data_for_blueprint(samurai):
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, samurai)
    if isinstance(obj, unreal.AutoAttackComponent):
        attack = obj
        break
assert attack, 'Samurai AutoAttack component template not found'
feedback = attack.get_editor_property('impact_feedback')
if not feedback.hit_niagara_system:
    feedback.hit_niagara_system = hit
    feedback.niagara_scale = unreal.Vector(0.65, 0.65, 0.65)
# Preserve the existing once-per-swing audio path if a sound was already assigned.
if not feedback.hit_sound and not attack.get_editor_property('impact_sound'):
    feedback.hit_sound = unreal.load_asset('/Game/Assets/Sounds/Samurai/MS_Samurai_Impact')
feedback.enable_camera_shake = True
feedback.camera_shake_class = shake.generated_class()
attack.set_editor_property('impact_feedback', feedback)
unreal.BlueprintEditorLibrary.compile_blueprint(samurai)
assert unreal.EditorAssetLibrary.save_loaded_asset(samurai, False)

ninja = load_bp('BP_NinjaProjectile')
defaults = unreal.get_default_object(ninja.generated_class())
feedback = defaults.get_editor_property('impact_feedback')
if not feedback.hit_niagara_system:
    feedback.hit_niagara_system = hit
    feedback.niagara_scale = unreal.Vector(0.3, 0.3, 0.3)
if not feedback.hit_sound:
    feedback.hit_sound = unreal.load_asset('/Game/Assets/Sounds/Ninja/MS_Ninja_Impact')
feedback.enable_camera_shake = False
defaults.set_editor_property('impact_feedback', feedback)
assert unreal.EditorAssetLibrary.save_loaded_asset(ninja, False)
unreal.log('IMPACT_FEEDBACK_SETUP_OK')
