"""Restore only the Sept 28 Samurai character edits from the Sept 27 commit."""
from pathlib import Path
from datetime import datetime
import hashlib, json, re, shutil, subprocess, sys

root=Path(__file__).resolve().parents[1]
revision='706865346964f9b41f12ad34029f0ebfa155abcc'
scope=['Content/Assets/PlayerCharacters/Samurai',
       'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.uasset',
       'Content/HeavensDivide/Blueprints/PlayerCharacters/ABP_Samurai.uasset',
       'Content/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai']
def git(*args): return subprocess.check_output(['git',*args],cwd=root)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def baseline(path):
    pointer=git('show',revision+':'+path)
    match=re.search(rb'oid sha256:([0-9a-f]{64})',pointer)
    assert match, path
    oid=match[1].decode()
    obj=root/'.git/lfs/objects'/oid[:2]/oid[2:4]/oid
    assert obj.exists() and sha(obj)==oid, ('Baseline LFS object missing/corrupt',path)
    return obj,oid
paths=git('ls-tree','-r','--name-only',revision,'--',*scope).decode().splitlines()
plan=[]
for path in paths:
    current=root/path
    is_recent=current.exists() and datetime.fromtimestamp(current.stat().st_mtime)>=datetime(2026,9,28)
    missing_physics=path.endswith('/SamuraiCharacterV4_PhysicsAsset.uasset') and not current.exists()
    if not (is_recent or missing_physics): continue
    obj,oid=baseline(path)
    before=sha(current) if current.exists() else None
    if before!=oid: plan.append({'path':path,'before':before,'restored':oid})
out=root/'Saved/Backups/SamuraiRevert20260928'
out.mkdir(parents=True,exist_ok=True)
if '--apply' not in sys.argv:
    print(json.dumps(plan,indent=2))
    for path in [scope[1],scope[2],'Content/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai.uasset','Content/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_DoubleCutSamurai.uasset']:
        obj,_=baseline(path)
        refs=sorted(set(s.decode() for s in re.findall(rb'/Game/[A-Za-z0-9_./-]+',obj.read_bytes())))
        print(path, json.dumps([s for s in refs if any(t in s for t in ['SamuraiCharacter','NS_Slash','Skeleton'])]))
    sys.exit(0)
protected=list((root/'Content/Assets/PlayerCharacters/Ninja').rglob('*.uasset'))
protected += [root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_CharacterBase.uasset',root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.uasset']
protected_hashes={str(p.relative_to(root)):sha(p) for p in protected if p.exists()}
# Back up every file before any restoration, including the unused imported V5 assets.
for item in plan:
    path=item['path'];current=root/path;target=out/path
    if current.exists():
        assert not target.exists(), 'Backup already exists; inspect before retrying'
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(current,target)
for current in (root/scope[0]).glob('SamuraiCharacterV5*.uasset'):
    target=out/current.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists(): shutil.copy2(current,target)
manifest={'revision':revision,'restored_files':plan,'protected_hashes':protected_hashes}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2))
for item in plan:
    obj,oid=baseline(item['path']);target=root/item['path']
    shutil.copyfile(obj,target)
    assert sha(target)==oid
for path,value in protected_hashes.items(): assert sha(root/path)==value,path
print('SAMURAI_RESTORE_PASS',len(plan),'files;',len(protected_hashes),'Ninja/shared assets unchanged')
