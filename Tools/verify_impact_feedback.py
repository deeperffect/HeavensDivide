"""Read-only saved-asset checks; run in a fresh Unreal Python commandlet."""
import unreal

root = '/Game/HeavensDivide/Blueprints/PlayerCharacters/'
samurai = unreal.load_asset(root + 'BP_Samurai')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = actors.spawn_actor_from_class(samurai.generated_class(), unreal.Vector())
attack = actor.get_component_by_class(unreal.AutoAttackComponent)
feedback = attack.get_editor_property('impact_feedback')
assert feedback.hit_niagara_system, 'Samurai VFX was not saved'
assert feedback.hit_sound or attack.get_editor_property('impact_sound'), 'Samurai impact audio missing'
assert feedback.enable_camera_shake and feedback.camera_shake_class
unreal.log('SAMURAI_IMPACT: ' + str(feedback))
actors.destroy_actor(actor)
ninja = unreal.load_asset(root + 'BP_NinjaProjectile')
feedback = unreal.get_default_object(ninja.generated_class()).get_editor_property('impact_feedback')
assert feedback.hit_niagara_system and feedback.hit_sound
assert not feedback.enable_camera_shake
unreal.log('NINJA_IMPACT: ' + str(feedback))
shake = unreal.load_asset('/Game/HeavensDivide/Blueprints/Feedback/BP_CS_SamuraiImpact')
assert shake
unreal.BlueprintEditorLibrary.compile_blueprint(samurai)
unreal.BlueprintEditorLibrary.compile_blueprint(ninja)
unreal.BlueprintEditorLibrary.compile_blueprint(shake)
unreal.log('IMPACT_FEEDBACK_VERIFY_OK')
