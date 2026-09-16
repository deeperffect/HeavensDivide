"""Read-only check for Prepare upgrades/descriptions in the current saved pool."""
import unreal
bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
pool = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
retired = {'StormConductor', 'FallenConstellation', 'ThreadSever', 'PlagueHarvest',
           'VenomEdge', 'OrbitRelay', 'RallyingShadows', 'EclipseHarvest',
           'PhantomHandoff', 'CrimsonVerdict', 'CarrionFeast'}
for card in pool:
    assert card
    uid = str(card.get_editor_property('upgrade_id'))
    assert uid not in retired, uid
    description = str(card.get_editor_property('description'))
    if 'prepar' in description.lower():
        unreal.log_warning('PREPARE_DESCRIPTION ' + card.get_path_name() + ': ' + description)
    if uid == 'TagTeam':
        unreal.log('TAG_TEAM_DESCRIPTION ' + description)
unreal.log('PREPARE_POOL_AUDIT complete: ' + str(len(pool)) + ' retained upgrades')
