"""Import original WAVs and build real one-shot MetaSound graphs with random pitch."""
import unreal as u,json
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();BASE='/Game/HeavensDivide/Audio';rows=json.loads((ROOT/'Tools/combat_audio_manifest.json').read_text())
at=u.AssetToolsHelpers.get_asset_tools();bs=u.get_engine_subsystem(u.MetaSoundBuilderSubsystem);ed=u.get_editor_subsystem(u.MetaSoundEditorSubsystem)
OK=u.MetaSoundBuilderResult.SUCCEEDED

def good(result):assert result==OK,str(result)
def value(result):good(result[-1]);return result[0]
def save(a):assert u.EditorAssetLibrary.save_loaded_asset(a,False)
def create(name,folder,cls,factory):
 path=BASE+'/'+folder+'/'+name
 return u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else at.create_asset(name,BASE+'/'+folder,cls,factory)
limits={'Weapon':(6,.035),'Impact':(8,.025),'Proc':(6,.06),'Ability':(4,.08),'Pulse':(4,.07),'Enemy':(6,.06),'Warning':(4,.1),'Death':(6,.05),'Major':(3,.12),'Movement':(4,.06),'Player':(2,.1),'UI':(3,.12),'Pickup':(4,.035),'CombatGlobal':(20,0.)}
concurrency={}
for group,(count,gap) in limits.items():
 a=create('SC_'+group,'Mix',u.SoundConcurrency,u.SoundConcurrencyFactory());p=a.get_editor_property('concurrency')
 p.set_editor_property('max_count',count);p.set_editor_property('retrigger_time',gap);p.set_editor_property('limit_to_owner',False);p.set_editor_property('resolution_rule',u.MaxConcurrentResolutionRule.STOP_QUIETEST);p.set_editor_property('voice_steal_release_time',.03)
 a.set_editor_property('concurrency',p);save(a);concurrency[group]=a
atten=create('ATT_Combat','Mix',u.SoundAttenuation,u.SoundAttenuationFactory());p=atten.get_editor_property('attenuation')
p.set_editor_property('attenuate',True);p.set_editor_property('spatialize',True);p.set_editor_property('attenuation_shape_extents',u.Vector(1600,0,0));p.set_editor_property('falloff_distance',3600.);atten.set_editor_property('attenuation',p);save(atten)
report=[]
for row in rows:
 name=row['event'];path=BASE+'/MetaSounds/MS_'+name
 if u.EditorAssetLibrary.does_asset_exist(path):
  a=u.load_asset(path);a.set_editor_property('concurrency_set',{concurrency[row['group']]} | ({concurrency['CombatGlobal']} if row['group']!='UI' else set()));save(a)
  report.append({'event':name,'existing':True});continue
 task=u.AssetImportTask();task.filename=str(ROOT/row['wav']);task.destination_path=BASE+'/Waves';task.destination_name='W_'+name;task.automated=True;task.save=True;task.replace_existing=False;at.import_asset_tasks([task])
 wave=u.load_asset(BASE+'/Waves/W_'+name);assert wave
 wave.set_editor_property('volume',1.);save(wave)
 b,onplay,finished,outputs,res=bs.create_source_builder('Build_'+name,u.MetaSoundOutputAudioFormat.MONO,True);good(res)
 def node(n,variant=''):
  c=u.MetasoundFrontendClassName();c.set_editor_property('namespace','UE');c.set_editor_property('name',n);c.set_editor_property('variant',variant)
  return value(b.add_node_by_class_name(c))
 def inp(n,k):return value(b.find_node_input_by_name(n,k))
 def out(n,k):return value(b.find_node_output_by_name(n,k))
 def connect(a,z):good(b.connect_nodes(a,z))
 def scalar(k,v):return value(b.add_graph_input_node(k,'Float',bs.create_float_meta_sound_literal(v)[0]))
 player=node('Wave Player','Mono');random=node('RandomFloat')
 wave_input=value(b.add_graph_input_node('Wave','WaveAsset',bs.create_object_meta_sound_literal(wave)))
 connect(wave_input,inp(player,'Wave Asset'))
 connect(scalar('PitchMin',row['pitch_min']),inp(random,'Min'));connect(scalar('PitchMax',row['pitch_max']),inp(random,'Max'))
 good(b.set_node_input_default(inp(random,'Seed'),bs.create_int_meta_sound_literal(-1)[0]))
 connect(onplay,inp(random,'Next'));connect(out(random,'On Next'),inp(player,'Play'));connect(out(random,'Value'),inp(player,'Pitch Shift'))
 connect(out(player,'Out Mono'),outputs[0]);connect(out(player,'On Finished'),finished)
 ed.set_node_location(b,random,u.Vector2D(-350,-150));ed.set_node_location(b,player,u.Vector2D(0,0))
 a=value(ed.build_to_asset(b,'HeavensDivide - original procedural audio','MS_'+name,BASE+'/MetaSounds'))
 a=u.load_asset(path);assert isinstance(a,u.MetaSoundSource),str(type(a))
 a.set_editor_property('volume',row['volume']);a.set_editor_property('concurrency_set',{concurrency[row['group']]} | ({concurrency['CombatGlobal']} if row['group']!='UI' else set()))
 if row['group']!='UI':a.set_editor_property('attenuation_settings',atten)
 save(a);report.append({'event':name,'path':a.get_path_name(),'pitch':[row['pitch_min'],row['pitch_max']]})
 u.log('COMBAT_METASOUND_CREATED '+name)
(ROOT/'Saved/CombatAudio/metasounds.json').write_text(json.dumps(report,indent=2));u.log('COMBAT_METASOUNDS_READY '+str(len(report)))

