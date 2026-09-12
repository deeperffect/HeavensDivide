import json
from pathlib import Path
rows=json.loads(Path('Tools/build_family_catalog.json').read_text())
kinds=list(dict.fromkeys(r['kind'] for r in rows)); reactions=list(dict.fromkeys(r['synergy']['kind'] for r in rows))
s='#pragma once\n#include "CoreMinimal.h"\n// Generated from Tools/build_family_catalog.json by Tools/generate_build_catalog.py.\n'
s+='enum class EBuildPattern : uint8 { '+', '.join(kinds)+' };\n'
s+='enum class EBuildReaction : uint8 { '+', '.join(reactions)+' };\n'
s+='''struct FBuildFamilySpec
{
 const TCHAR* Id; const TCHAR* Owner; const TCHAR* Title; EBuildPattern Pattern;
 float Damage, Radius, Cooldown; int32 Count; float Interval;
 const TCHAR* Branches[3]; const TCHAR* Synergy;
 EBuildReaction Reaction; float ReactionFactor, ReactionRadius; int32 ReactionCount;
};
inline const FBuildFamilySpec BuildFamilies[] = {
'''
for r in rows:
 q=lambda x:'TEXT('+json.dumps(x)+')'
 def f(x):return str(float(x))+'f'
 y=r['synergy'];s+=' { '+', '.join([q(r['id']),q(r['owner']),q(r['name']),'EBuildPattern::'+r['kind'],f(r['damage']),f(r['radius']),f(r['cooldown']),str(r['count']),f(r['interval']),'{'+','.join(q(b['id']) for b in r['branches'])+'}',q(y['id']),'EBuildReaction::'+y['kind'],f(y['factor']),f(y['radius']),str(y['count'])])+' },\n'
s+='};\nstatic_assert(UE_ARRAY_COUNT(BuildFamilies)==20);\n'
Path('Source/HeavensDivide/BuildFamilyCatalog.h').write_text(s)
