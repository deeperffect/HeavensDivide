"""Audit named combat parameters and prepare an additive asset migration (no balance resets)."""
from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1]
values={};sources={};conflicts=[]
def add(uid,key,value,source):
 value=float(value.rstrip('fF')) if isinstance(value,str) else float(value)
 existing=values.setdefault(uid,{})
 if key in existing and existing[key]!=value:conflicts.append([uid,key,existing[key],value,source])
 else:existing[key]=value
 sources.setdefault(uid,{}).setdefault(key,[]).append(source)
num=r'(-?(?:\d+(?:\.\d*)?|\.\d+)[fF]?)'
for p in sorted((root/'Source/HeavensDivide').glob('*')):
 if p.suffix not in ('.h','.cpp') or any(x in p.name for x in ['Tests','Commandlet']):continue
 s=p.read_text(encoding='utf-8-sig')
 for m in re.finditer(r'(?:Tune|Value)\([^;\n]*?TEXT\("(\w+)"\),\s*TEXT\("(\w+)"\),\s*'+num+r'\)',s):
  add(m[1],m[2],m[3],p.name)
 for m in re.finditer(r'(?:Scaling|Rank)\([^,;\n]+,\s*TEXT\("(\w+)"\),\s*'+num+r'\)',s):
  add(m[1],'PerRank',m[2],p.name)
 for m in re.finditer(r'Chance\([^,;\n]+,\s*TEXT\("(\w+)"\),\s*TEXT\("(\w+)"\),\s*'+num+r',\s*'+num+r'\)',s):
  add(m[1],'Chance',m[3],p.name);add(m[2],'PerRank',m[4],p.name)
for filename in ['upgrade_tuning_defaults.json','samurai_build_upgrades.json','ninja_build_upgrades.json','barrage_build_upgrades.json','fang_build_upgrades.json','shuriken_build_upgrades.json']:
 data=json.loads((root/'Tools'/filename).read_text(encoding='utf-8-sig'))
 if isinstance(data,list):data={d['id']:d.get('balance',{}) for d in data}
 for uid,params in data.items():
  for key,value in params.items():
   if uid not in values or key not in values[uid]:add(uid,key,value,filename)
for uid,params in {'BloodRush':{'Duration':3,'MoveSpeedBonus':.2},'IaijutsuDash':{'DashDistanceBonus':1}}.items():
 for key,value in params.items():add(uid,key,value,'Named local card lookup')
# The coordinated pass is newer than historical source fallbacks and seed files.
balance_path=root/'Tools/combat_balance_v1.json'
if balance_path.exists():
 for uid,change in json.loads(balance_path.read_text()).get('upgrades',{}).items():
  for key,value in change.get('parameters',{}).items():
   values.setdefault(uid,{})[key]=value
   sources.setdefault(uid,{}).setdefault(key,[]).append('combat_balance_v1.json')
(root/'Tools/balance_parameter_catalog.json').write_text(json.dumps({'values':values,'sources':sources,'conflicting_fallbacks':conflicts},indent=2)+'\n')
print(f'{len(values)} definitions; {sum(map(len,values.values()))} named parameters; {len(conflicts)} fallback conflicts (saved assets always win).')
