"""Rebuild the local asset catalog and verify the delivered PNG files."""
from pathlib import Path
import hashlib
import html
import json
import struct
import zlib

ROOT = Path(__file__).resolve().parent.parent
manifest_path = ROOT / 'AssetManifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
manifest['artDirection'] = 'Exaggerated chunky forms, broad visible hand-painted brushwork, simplified diffuse materials.'
manifest['styleReference'] = '01_Buildings/B01_Main_Shrine_Hall.png'

def inspect_png(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'Not a PNG: {path}')
    pos, width, height, ended = 8, None, None, False
    while pos + 12 <= len(data):
        size = struct.unpack('>I', data[pos:pos+4])[0]
        kind = data[pos+4:pos+8]
        body = data[pos+8:pos+8+size]
        crc = data[pos+8+size:pos+12+size]
        if len(body) != size or len(crc) != 4:
            raise ValueError(f'Truncated PNG: {path}')
        if zlib.crc32(kind + body) & 0xffffffff != struct.unpack('>I', crc)[0]:
            raise ValueError(f'PNG CRC mismatch: {path}')
        if kind == b'IHDR':
            width, height = struct.unpack('>II', body[:8])
        pos += size + 12
        if kind == b'IEND':
            ended = True
            break
    if not ended or not width or not height:
        raise ValueError(f'Incomplete PNG: {path}')
    return {'width': width, 'height': height, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

complete = []
for asset in manifest['assets']:
    prompt_path = ROOT / asset['promptFile']
    if prompt_path.exists():
        asset['prompt'] = prompt_path.read_text(encoding='utf-8-sig')
    path = ROOT / asset['file']
    if path.exists():
        asset.update(inspect_png(path))
        asset['status'] = 'saved'
        complete.append(asset)
    else:
        asset['status'] = 'pending'
manifest['status'] = 'complete' if len(complete) == len(manifest['assets']) else 'in_progress'
manifest['savedCount'] = len(complete)
manifest['assetCount'] = len(manifest['assets'])
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')

categories = list(dict.fromkeys(a['folder'] for a in manifest['assets']))
def label(folder):
    return folder.split('_', 1)[1].replace('_', ' ')
def esc(value):
    return html.escape(str(value), quote=True)
sections = []
for folder in categories:
    cards = []
    for asset in (a for a in manifest['assets'] if a['folder'] == folder):
        name = asset['name'].replace('_', ' ')
        picture = (f'<a href="{esc(asset["file"])}"><img loading="lazy" src="{esc(asset["file"])}" alt="{esc(name)}"></a>'
                   if asset['status'] == 'saved' else '<div class="pending">Pending</div>')
        cards.append(f'<article data-search="{esc((asset["id"] + " " + name + " " + label(folder)).lower())}">{picture}<div class="copy"><small>{esc(asset["id"])}</small><h3>{esc(name)}</h3><p>{esc(asset["use"])}</p><p class="scale">Suggested size: {esc(asset["scale"])}</p><a class="prompt" href="{esc(asset["promptFile"])}">Generation prompt</a></div></article>')
    sections.append(f'<section data-category="{esc(folder)}"><h2>{esc(label(folder))}</h2><div class="grid">{"".join(cards)}</div></section>')
options = ''.join(f'<option value="{esc(c)}">{esc(label(c))}</option>' for c in categories)
page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Old Village - Model References</title>
<style>*{box-sizing:border-box}body{margin:0;background:#f4efe6;color:#282b31;font-family:system-ui,sans-serif}header,main{max-width:1400px;margin:auto;padding:32px}header{padding-bottom:12px}h1{font-size:36px;margin:8px 0}h2{font-size:24px;margin:34px 0 16px}h3{font-size:18px;margin:6px 0}p{line-height:1.5}a{color:#934b36}nav{display:flex;gap:12px;flex-wrap:wrap;margin:24px 0}input,select{padding:12px 16px;border:1px solid #c7bcaa;border-radius:8px;background:#fffcf7;font:inherit}input{flex:1;min-width:210px}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:20px}article{background:#fffcf7;border:1px solid #e2d9ca;border-radius:12px;overflow:hidden}img{display:block;width:100%;aspect-ratio:1;object-fit:contain;background:#fffcf7}.copy{padding:18px}small{font-weight:700;letter-spacing:.1em;color:#906c49}.copy p{font-size:14px;margin:8px 0}.scale,.prompt{font-size:12px;color:#69665e}.summary{max-width:850px}.refs{display:flex;gap:20px;flex-wrap:wrap}.pending{aspect-ratio:1;display:grid;place-items:center;color:#777}[hidden]{display:none!important}footer{padding:30px 0;color:#69665e;font-size:13px}</style>
<header><small>HEAVENS DIVIDE / BLOODSHIFT</small><h1>Old Village Model References</h1><p class="summary">COUNT separate asset images with exaggerated shapes and visible hand-painted brushwork. Each PNG shows one asset. Suggested sizes are starting points for modeling, not measured dimensions.</p><div class="refs"><a href="00_Reference/OldVillage_Environment.png">Environment layout</a><a href="01_Buildings/B01_Main_Shrine_Hall.png">Painted style reference</a><a href="AssetList.txt">Asset checklist</a><a href="README.txt">Assembly notes</a></div><nav><input id="search" placeholder="Find an asset..." aria-label="Find an asset"><select id="category" aria-label="Asset category"><option value="">All categories</option>OPTIONS</select></nav><p id="count"></p></header><main>SECTIONS<footer>Created with built-in image_gen. Original PNGs and exact prompts are preserved in this folder.</footer></main>
<script>const search=document.querySelector('#search'),category=document.querySelector('#category');function filter(){let shown=0;document.querySelectorAll('section').forEach(section=>{let local=0;section.querySelectorAll('article').forEach(card=>{const ok=(!category.value||section.dataset.category===category.value)&&card.dataset.search.includes(search.value.toLowerCase());card.hidden=!ok;if(ok){local++;shown++}});section.hidden=!local});document.querySelector('#count').textContent=shown+' assets shown'}search.addEventListener('input',filter);category.addEventListener('change',filter);filter();</script></html>'''
page = page.replace('COUNT', str(len(complete))).replace('OPTIONS', options).replace('SECTIONS', ''.join(sections))
(ROOT / 'index.html').write_text(page, encoding='utf-8')
lines = ['OLD VILLAGE - MODEL REFERENCE CHECKLIST', '', 'Suggested sizes are modeling starting points, not measured dimensions.', '']
for folder in categories:
    lines.extend([label(folder).upper(), ''])
    for a in (x for x in manifest['assets'] if x['folder'] == folder):
        lines.extend([f'[{"x" if a["status"] == "saved" else " "}] {a["id"]} {a["name"].replace("_", " ")}', f'    Image: {a["file"]}', f'    Suggested size: {a["scale"]}', f'    Use: {a["use"]}', ''])
(ROOT / 'AssetList.txt').write_text('\n'.join(lines), encoding='utf-8')
report = {'saved': len(complete), 'expected': len(manifest['assets']), 'missing': [a['id'] for a in manifest['assets'] if a['status'] != 'saved'], 'pngChecks': 'signature, dimensions, chunk CRCs, end marker', 'uniqueHashes': len({a['sha256'] for a in complete})}
(ROOT / '00_Reference' / 'Verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report))
