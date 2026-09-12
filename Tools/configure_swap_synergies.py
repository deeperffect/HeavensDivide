import unreal, shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
def update(path,description):
 a=unreal.load_asset(path)
 assert a,path
 rel=Path(path.removeprefix('/Game/')+'.uasset')
 backup=root/'Saved/Backups/SwapSynergies'/rel
 if not backup.exists():
  backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/'Content'/rel,backup)
 a.set_editor_property('description',description)
 assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
families=[
 ('Samurai','SteelTempest','Steel Tempest','Every 4 seconds, release a golden ring around Samurai for 24 base damage in a 260 cm radius.','RazorHalo','Razor Halo','Steel Tempest echoes after 0.3 seconds for 65% damage.','Area'),
 ('Samurai','Heavenfall','Heavenfall','Every 4 seconds, target the densest nearby crowd. After 0.5 seconds, strike a 280 cm radius for 42 base damage and apply Bleed. With Marked Blade, mark survivors.','Starfall','Starfall','Heavenfall strikes up to 3 separate nearby groups, prioritizing enemies outside earlier strike areas.','Deathblow'),
 ('Ninja','NightThread','Night Thread','Every 3.5 seconds, a shadow thread strikes up to 3 nearby enemies for 25 base damage each, jumping up to 360 cm between targets. Prioritizes bleeding enemies and applies Poison to them.','BlackWeb','Black Web','Night Thread leaves a small delayed burst at each hit position for 50% damage.','KunaisBounce'),
 ('Ninja','VenomGarden','Venom Garden','Plant a 340 cm field in a nearby crowd: 8 pulses of 4 base damage, 1.1 seconds apart. Poisons unpoisoned enemies. Samurai melee hits inside detonate the field for 150% remaining damage in a larger area and apply Bleed. 9 second recharge; one field at a time.','WitheringGarden','Withering Garden','Venom Garden lasts for 11 pulses, then bursts for triple pulse damage with 25% more radius. Samurai detonation includes this final burst in its remaining-damage payout.','VenomousKunai')]

for owner,family,title,desc,evo,evo_title,evo_desc,art in families:
 for uid,text in [(family,desc),(evo,evo_desc)]:
  update('/Game/HeavensDivide/Upgrades/'+owner+'/DA_Upgrade_'+owner+uid,text)
update('/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_TagTeam','Every 5 qualifying basic attacks, your partner assists. Ninja hits up to 5 enemies in a fan and applies Poison. Samurai slashes up to 12 enemies, applies Bleed and pushes them back; with Marked Blade, also marks survivors. Assist damage scales with the assisting character. Assists do not detonate Venom Garden.')
unreal.log('SWAP_SYNERGY_ASSETS_PASS')
