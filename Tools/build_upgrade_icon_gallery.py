"""Build an offline, player-facing gallery of currently available upgrades."""
import html
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / 'Art/UpgradeIcons'
manifest = json.loads((CATALOG / 'manifest.json').read_text(encoding='utf-8'))
roster = json.loads((ROOT / 'Tools/samurai_stance_expansion.json').read_text(encoding='utf-8'))
disabled = set(roster['disabled_ids'])
names = {row['id']: row['name'] for row in manifest['cards']}
cards = []
for row in manifest['cards']:
    if not row['in_pool'] or row['id'] in disabled:
        continue
    category = {'CURSED': 'Blood Pacts', 'GLOBAL': 'Shared'}.get(row['category'], row['category'].title())
    search = ' '.join([row['name'], row['description'], *[names[uid] for uid in row['requirements'] if uid in names]]).lower()
    source = Path(row['file']).relative_to('Art/UpgradeIcons').as_posix()
    cards.append(f'''<article data-category="{html.escape(category)}" data-search="{html.escape(search, quote=True)}">
      <a href="{html.escape(source)}" target="_blank" rel="noopener"><img src="{html.escape(source)}" alt="{html.escape(row['name'])} icon" loading="lazy" width="240" height="240"></a>
      <div class="copy"><small>{html.escape(category)}</small><h2>{html.escape(row['name'])}</h2>
      <p>{html.escape(row['description'])}</p></div>
    </article>''')
assert len(cards) == roster['enabled_count'], 'Gallery must match the current available roster'
options = ''.join(f'<option>{html.escape(name)}</option>' for name in ['Samurai', 'Ninja', 'Synergy', 'Shared', 'Blood Pacts'])
page = '''<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BLOODSHIFT | Upgrades</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#0a181e;color:#eee8da;font:16px/1.5 system-ui,sans-serif}
header,main{max-width:1440px;margin:auto;padding:28px}header{padding-bottom:12px}h1{margin:0;font-size:32px;letter-spacing:.04em}header p{color:#acb9bd;max-width:800px}
.filters{display:flex;gap:12px;flex-wrap:wrap;align-items:center}label{display:flex;gap:8px;align-items:center}input,select{font:inherit;background:#142a32;color:#fff;border:1px solid #41606b;border-radius:8px;padding:10px}input{width:min(420px,65vw)}#count{color:#c2cece}
main{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:20px;padding-top:12px}article{background:#102229;border:1px solid #29434d;border-radius:12px;overflow:hidden}article[hidden]{display:none}img{display:block;width:100%;height:auto;aspect-ratio:1;object-fit:contain}.copy{padding:18px;padding-top:8px}small{color:#d9b370;text-transform:uppercase;letter-spacing:.1em;font-size:11px}h2{font-size:18px;margin:6px 0}p{font-size:14px;color:#b6c5c9}details{font-size:12px;overflow-wrap:anywhere;color:#9dafb5}summary{cursor:pointer}a:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #eac977;outline-offset:3px}
</style>
<header><h1>BLOODSHIFT</h1><p>Explore stance upgrades, shared bonuses, and Blood Pacts.</p>
<div class="filters"><label>Search <input id="search" type="search" placeholder="Name, effect, or prerequisite…"></label><label>Category <select id="category"><option value="">All categories</option>OPTIONS</select></label><span id="count" aria-live="polite"></span></div></header>
<main>CARDS</main>
<script>
const search=document.querySelector('#search'),category=document.querySelector('#category'),cards=[...document.querySelectorAll('article')];
function filter(){const terms=search.value.trim().toLowerCase().split(/\s+/).filter(Boolean);let count=0;for(const card of cards){const visible=(!category.value||category.value===card.dataset.category)&&terms.every(term=>card.dataset.search.includes(term));card.hidden=!visible;if(visible)count++}document.querySelector('#count').textContent=`${count} / ${cards.length} upgrades`}
search.addEventListener('input',filter);category.addEventListener('change',filter);filter();
</script></html>'''.replace('OPTIONS', options).replace('CARDS', '\n'.join(cards))
(CATALOG / 'index.html').write_text(page, encoding='utf-8')
print(f'Upgrade gallery built: {len(cards)} cards')
