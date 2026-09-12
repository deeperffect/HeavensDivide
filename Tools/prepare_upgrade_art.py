"""Build a resumable per-card image-generation manifest from the design catalog."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rows = json.loads((root / 'Tools/build_family_catalog.json').read_text())
out = root / 'Art/UpgradeCards'
jobs = []
subjects = {
 'SteelTempest': 'A complete horizontal ring of razor wind around a planted samurai.',
 'Heavenfall': 'A gigantic spectral sword plunging vertically from storm clouds onto a crowd below. Show the sky-to-ground impact, no horizontal melee swing.',
 'NightThread': 'Fine violet supernatural threads zigzag between three shadow enemies, binding them into a triangular web.',
 'VenomGarden': 'A sprawling bed of poisonous spectral flowers and thorny vines blooms in violet mist around enemy feet.',
 'BladeWave': 'A detached crescent of cutting energy flies forward from a katana across the battlefield.',
 'CrescentReaper': 'One enormous crescent-shaped spectral blade flies diagonally through a line of shadow enemies.',
 'IronOrbit': 'Three clearly separate metal swords orbit a small samurai silhouette like satellites, with thin circular trails.',
 'FaultLine': 'Jagged cracked ground erupts in a straight sequence of red energy geysers beneath a crowd.',
 'WarBanner': 'A tall tattered Japanese war banner planted firmly in earth radiates power through shadow soldiers. Blank cloth, no writing.',
 'ThousandCuts': 'One looming shadow enemy caught in a dense lattice of short rapid crimson sword cuts.',
 'SpiritLance': 'A long narrow spectral spear pierces through several aligned enemies in one straight thrust.',
 'BloodMoon': 'A huge crimson moon hangs over a hollow expanding ground-level red tide encircling a crowd.',
 'LotusMines': 'Three small lotus-shaped metal traps on the ground unfold sharp petals into violet explosions beneath approaching enemy feet.',
 'ShadowShuriken': 'A large four-point steel shuriken ricochets between shadow enemies along an angular violet trail.',
 'SmokeLattice': 'Layered violet smoke forms a ground-level lattice of intersecting bands, with enemy silhouettes recoiling at its edges.',
 'ThunderWire': 'A taut thin wire stretched between two anchors cuts through enemies, crackling with violet electrical sparks.',
 'PhantomAmbush': 'Two translucent masked ninja phantoms lunge inward from opposing sides at a central enemy.',
 'CaltropTrail': 'Close foreground of sharp four-point metal caltrops scattered on a winding violet trail behind a retreating ninja.',
 'CrimsonNeedle': 'A long slender steel throwing needle with violet trails pierces a crimson-marked shadow target; precise single-point impact.',
 'RavenSwarm': 'Black ravens with violet feather highlights spiral around a struggling shadow enemy, wings and ink scattering.',
}
for r in rows:
    cards = [(r['id'], r['name'], 'Starter', r['description'])]
    cards += [(b['id'], b['name'], 'Branch', b['description']) for b in r['branches']]
    for s in r['scales']:
        uid = 'WideArc' if s == 'WideArc' else r['id'] + s
        visual = {'Power': 'Emphasize a concentrated, forceful impact and shattered enemy armor; close-up composition.',
                  'Area': 'Emphasize enormous reach across a wider crowd; elevated wide view with expanding brushstroke arcs.',
                  'WideArc': 'Emphasize an exceptionally wide sweeping crescent cutting across a crowd.',
                  'Haste': 'Emphasize rapid motion with rhythmic repeated afterimages and speed strokes.'}[s]
        cards.append((uid, r['name'] + ': ' + s, 'Scaling', visual))
    y = r['synergy']
    cards.append((y['id'], y['name'], 'Synergy', y['description']))
    for uid, name, role, desc in cards:
        synergy = role == 'Synergy'
        palette = 'crimson and violet interacting, muted parchment-gray background and deep black ink' if synergy else ('crimson red, black and small pale-red highlights' if r['owner'] == 'Samurai' else 'violet purple, black and small silver-lavender highlights')
        ref = 'Synergy_Hemotoxin' if synergy else ('Samurai_BladeWave' if r['owner'] == 'Samurai' else 'Ninja_VenomousKunai')
        prompt = (f'Use case: stylized-concept. Generate ONE new square game upgrade illustration for {name}. '
                  f'Input image is STYLE REFERENCE ONLY. Match its dark Japanese fantasy ink illustration, textured brushwork, intricate black silhouettes, explosive ink splatter and dramatic high contrast. Palette: {palette}. '
                  f'REQUIRED NEW SUBJECT: {subjects[r["id"]]} Do not copy the reference image composition or its specific weapon/pose. '
                  f'Ability family: {r["name"]}. Family action: {r["description"]} '
                  f'This specific {role.lower()} depicts: {desc} '
                  'Translate the mechanic into a clear visual scene, never written instructions or statistics. Make this card a distinct composition with one readable focal action, legible at thumbnail size. '
                  'Use armored samurai, masked ninja, weapons or shadow enemy silhouettes only as appropriate to the action. '
                  'For poison use violet ink and mist; for Bleed use crimson ink. Full-bleed square illustration, dark vignette. No text, letters, numbers, UI, frame, watermark, photorealism or 3D rendering.')
        folder = 'Synergy' if synergy else r['owner']
        prefix = 'DA_BuildSynergy_' if synergy else 'DA_Upgrade_' + r['owner']
        jobs.append(dict(id=uid, name=name, family=r['id'], owner=r['owner'], role=role,
                         asset='/Game/HeavensDivide/Upgrades/' + folder + '/' + prefix + uid,
                         texture='/Game/HeavensDivide/Blueprints/UI/BuildFamilyArt/T_' + uid,
                         file='Art/UpgradeCards/Generated/' + uid + '.png',
                         reference=str(out / 'References' / (ref + '.png')), prompt=prompt))
assert len(jobs) == 160 and len({j['id'] for j in jobs}) == 160
(out / 'manifest.json').write_text(json.dumps(jobs, indent=2), encoding='utf-8')
print('Prepared 160 artwork jobs')
