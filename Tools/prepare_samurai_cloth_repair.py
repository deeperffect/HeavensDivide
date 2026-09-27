from pathlib import Path
import hashlib,json,re,shutil,subprocess
root=Path(__file__).resolve().parents[1]
out=root/'Saved/Backups/SamuraiClothRepair20260928';out.mkdir(parents=True,exist_ok=True)
revision='706865346964f9b41f12ad34029f0ebfa155abcc'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
protected=list((root/'Content/Assets/PlayerCharacters/Ninja').rglob('*.uasset'))
protected += [root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.uasset',root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_CharacterBase.uasset']
manifest={'protected':{str(p.relative_to(root)):sha(p) for p in protected},'backups':[],'restored_animation_assets':[]}
scopes=['Content/Assets/PlayerCharacters/Samurai','Content/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai']
files=[p for scope in scopes for p in (root/scope).rglob('*.uasset')]
files += [root/'Content/HeavensDivide/Blueprints/PlayerCharacters/ABP_Samurai.uasset',root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.uasset']
for p in files:
    dest=out/p.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True)
    assert not dest.exists(),'Backup exists; inspect before retrying'
    shutil.copy2(p,dest);manifest['backups'].append(str(p.relative_to(root)))
targets=[p for p in files if '/RetargetedAnimations/' in p.as_posix() or '/Montages/Samurai/' in p.as_posix() or p.name in ['ABP_Samurai.uasset','SamuraiCharacterV4_Skeleton.uasset','SamuraiCharacterV4_PhysicsAsset.uasset']]
for p in targets:
    relative=p.relative_to(root).as_posix()
    pointer=subprocess.check_output(['git','show',revision+':'+relative],cwd=root)
    oid=re.search(rb'oid sha256:([0-9a-f]{64})',pointer)[1].decode()
    source=root/'.git/lfs/objects'/oid[:2]/oid[2:4]/oid
    assert sha(source)==oid
    if sha(p)!=oid:
        shutil.copyfile(source,p);manifest['restored_animation_assets'].append(relative)
(out/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('Backed up',len(files),'Samurai assets; repaired',len(manifest['restored_animation_assets']),'animation/skeleton references; new mesh retained')
