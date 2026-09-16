"""Make the assigned healing burst respect its Niagara component scale."""
import unreal

bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_HealingPickup')
cdo = unreal.get_default_object(bp.generated_class())
system = cdo.get_editor_property('pickup_burst_fx')
assert system, 'No Pickup Burst FX is assigned'

# Local space scales particle positions, but sprite quads retain their authored
# size. Apply Owner Scale explicitly scales the rendered particle attributes.
assert unreal.SwapVFXSetupLibrary.prepare_pickup_burst_scale(system)

# The burst stays at the collection point, so local simulation preserves its
# motion while allowing the component transform to scale all of its particles.
changed = unreal.SwapVFXSetupLibrary.prepare_portal_local_space(system)
unreal.log('HEAL_BURST_SCALE ' + system.get_path_name()
           + ': converted ' + str(changed) + ' world-space emitters to local space')
assert unreal.EditorAssetLibrary.save_loaded_asset(system, False)
assert unreal.SwapVFXSetupLibrary.prepare_portal_local_space(system) == 0
unreal.log('HEAL_BURST_SCALE verified all emitters use local space')
