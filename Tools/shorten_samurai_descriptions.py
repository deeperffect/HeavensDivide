"""Edit only Samurai card text; derive numerical text from saved tuning.

-AuditSamuraiText dumps current text/tuning without edits.
"""
import json
import shutil
from pathlib import Path
from datetime import datetime
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
pool=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
cards=[a for a in pool if a and a.get_path_name().startswith('/Game/HeavensDivide/Upgrades/Samurai/')]
def snapshot(a):
    return {k:str(a.get_editor_property(k)) for k in ['balance_parameters','stat_modifiers','rarity_magnitudes','max_level','rarity','uses_rolled_rarity','prerequisite_upgrade_ids','prerequisite_requirements','exclusivity_group','special_effects','card_artwork','icon','category','investment_owner','build_family_id','role']}
audit=[]
for a in cards:
    audit.append(dict(id=str(a.get_editor_property('upgrade_id')),description=str(a.get_editor_property('description')),format=str(a.get_editor_property('rolled_description_format')),**snapshot(a)))
out=root/'Saved/SamuraiCardTextAudit.json';out.write_text(json.dumps(audit,indent=2))
if '-AuditSamuraiText' in unreal.SystemLibrary.get_command_line():
    unreal.log('SAMURAI_TEXT_AUDIT: '+str(out))
else:
    by_id={str(a.get_editor_property('upgrade_id')):a for a in cards}
    def tune(uid,key,default):
        return {str(k):float(v) for k,v in by_id[uid].get_editor_property('balance_parameters').items()}.get(key,default)
    def n(value):return f'{value:.2f}'.rstrip('0').rstrip('.')
    text={
        'DoubleCut':'Every 3rd melee attack hits twice.',
        'BleedingEdge':f'Melee and Blade Wave hits apply Bleed. Each stack gains {n(tune("BleedingEdge","HitDamagePerTick",.1)*100)}% of hit damage per tick.',
        'BladeWave':f'Melee attacks launch a wave dealing {n(tune("BladeWave","WaveDamageMultiplier",.65)*100)}% attack damage.',
        'ReturningBlade':'Blade Waves return, hitting enemies again.',
        'CrossingBlades':f'Every {n(tune("CrossingBlades","AttackFrequency",3))} attacks, launch {n(tune("CrossingBlades","WaveCount",3))} crossing waves.',
        'SplinterWave':f'Each wave\'s first hit bursts for {n(tune("SplinterWave","SplinterDamageMultiplier",.3)*100)}% wave damage and applies Bleed nearby.',
        'BloodTransfer':f'On death, share {n(tune("BloodTransfer","TransferFraction",.5)*100)}% remaining Bleed damage among up to {n(tune("BloodTransfer","Targets",5))} nearby enemies.',
        'Bloodletting':'+1 Bleed stack per melee or Blade Wave hit, per rank.',
        'OverkillBurst':'Melee and Blade Wave kills explode for excess damage. Explosions cannot chain.',
        'WaveMultishot':'+1 Blade Wave per attack, per rank.',
    }
    formats={
        'SamuraiArea':'+{Percent}% Samurai attack area.',
        'DeepCuts':'+{Percent}% Bleed damage.',
        'WideArc':'+{Percent}% Blade Wave width and damage.',
        'BladeWavePower':'+{Percent}% Blade Wave damage.',
        'BladeWaveHaste':'+{Percent}% Blade Wave speed.',
        'LingeringWounds':'+{Percent}% Bleed duration.',
        'SamuraiTempo':'+{Percent}% Samurai attack speed.',
        'BurstRadius':'+{Percent}% explosion radius.',
    }
    for uid in ['BloodStance','ExecutionStance','WaveStance','SamuraiHeavyBlade']:
        labels={unreal.CharacterStatType.ATTACK_AREA_MULTIPLIER:'area',unreal.CharacterStatType.ATTACK_SPEED_MULTIPLIER:'attack speed',unreal.CharacterStatType.DAMAGE_MULTIPLIER:'damage'}
        parts=[]
        for m in by_id[uid].get_editor_property('stat_modifiers'):
            value=float(m.get_editor_property('value_per_level'))
            if m.get_editor_property('operation')==unreal.StatModifierOperation.MULTIPLY:value-=1
            parts.append(('+' if value>=0 else '-')+n(abs(value)*100)+'% '+labels[m.get_editor_property('character_stat')])
        text[uid]=', '.join(parts)+'.'+(' One stance per run.' if uid.endswith('Stance') else '')
    for uid,fmt in formats.items():
        a=by_id[uid];rarity=a.get_editor_property('rarity')
        values=list(a.get_editor_property('rarity_magnitudes'))
        match=next((v for v in values if v.get_editor_property('rarity')==rarity),values[0])
        text[uid]=fmt.replace('{Percent}',n(float(match.get_editor_property('magnitude'))*100))
    assert set(text)==set(by_id)
    backup=root/'Saved/Backups/SamuraiCardText'/datetime.now().strftime('%Y%m%d_%H%M%S')
    for uid,a in by_id.items():
        before=snapshot(a)
        relative=Path('Content')/(a.get_path_name().removeprefix('/Game/').split('.')[0]+'.uasset')
        (backup/relative).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/relative,backup/relative)
        a.set_editor_property('description',text[uid])
        a.set_editor_property('rolled_description_format',formats.get(uid,''))
        assert snapshot(a)==before,uid
        assert unreal.EditorAssetLibrary.save_loaded_asset(a,False),uid
        assert str(a.get_editor_property('description'))==text[uid]
    (root/'Saved/SamuraiCardTextResult.json').write_text(json.dumps(text,indent=2))
    unreal.log('SAMURAI_TEXT_OK: 22 concise descriptions; saved tuning, modifiers, rarity, prerequisites and artwork preserved')
