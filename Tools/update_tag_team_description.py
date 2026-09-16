"""Bring the saved Tag Team card in line with its current attack behavior."""
import unreal
bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
pool = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
card = next(card for card in pool if str(card.get_editor_property('upgrade_id')) == 'TagTeam')
card.set_editor_property('description', unreal.Text(
    'Every 5 qualifying basic attacks, your partner assists. Ninja uses her equipped attack stance and upgrades. '
    'Samurai slashes up to 12 enemies and pushes them back. Bleed requires Bleeding Edge; Poison requires Venomous Kunai. '
    'With Marked Blade, Samurai also marks survivors. Assist damage scales with the assisting character.'))
assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
unreal.log('TAG_TEAM_DESCRIPTION_UPDATED ' + card.get_path_name())
