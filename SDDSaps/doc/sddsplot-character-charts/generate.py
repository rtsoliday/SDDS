"""Generate reference images from the repository's Hershey font geometry.

Run with make -C SDDSaps/doc character-charts. No compilation or application
execution is needed. Glyphs are individually fitted for reference readability.
"""
from pathlib import Path
import re
import string
import html
import zipfile
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
source = (ROOT / 'SDDSaps/sddsplots/hershey.font').read_text()
fonts = {}
for name, body in re.findall(r'unsigned char (\w+)\[\] = \{(.*?)\};', source, re.S):
  data = bytes(int(h, 16) for h in re.findall(r'0x([0-9a-fA-F]+)', body)).decode()
  glyphs = {}
  for i, line in enumerate(data.splitlines()):
    paths, path = [], []
    for j in range(int(line[5:8]) - 1):
      pair = line[10 + 2*j:12 + 2*j]
      if pair == ' R':
        if path: paths.append(path)
        path = []
      elif len(pair) == 2:
        path.append((ord(pair[0])-82, ord(pair[1])-82))
    if path: paths.append(path)
    glyphs[chr(i+32)] = paths
  fonts[name] = glyphs

def array(name):
  body = re.search(r'short '+name+r'\[\w+\] = \{(.*?)\};', source, re.S)[1]
  body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
  return [int(v) for v in re.findall(r'-?\d+', body)]

ix, iy, starts = array('ix'), array('iy'), array('istart')
def legacy(n):
  paths, path = [], []
  for i in range(starts[n-1], len(ix)):
    if ix[i] == 127:
      if path: paths.append(path)
      path = []
      if iy[i] == 127: break
    else:
      path.append((ix[i], iy[i]))
  return paths

standard = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,/()-+=*'
greek = [1,2,7,4,5,21,3,22,9,9,10,11,12,13,15,16,8,17,18,19,20,22,24,14,23,6]
remap = dict(zip('!#$%^&_{}[]<>?|', 'EFDY6W-PQMNLGHB'))
fixed = {':':70, ';':71, '@':211, '"':212, "'":219, '`':219}
switches = {'cyrillic':'c', 'mathlow':'m', 'greek':'k', 'symbolic':'y', 'rowmans':'1', 'rowmand':'2', 'rowmant':'3'}
chars = string.ascii_uppercase + string.ascii_lowercase + string.digits + '.,/()-+=*' + '!#$%^&_{}[]<>?|:;@"\'`'
assert len(set(chars)) == len(chars)

def shape(font, ch, mode=''):
  if ch in fixed: return legacy(fixed[ch]), True
  if ch in remap: return legacy(60 + standard.index(remap[ch])+1), True
  upper = ch.upper()
  ic = standard.index(upper)+1
  lower = 105 if ch.islower() else 0
  if mode == 's': return legacy(60+ic+lower), True
  if mode == 'g' and ch.isalpha(): return legacy(36+greek[ic-1]+lower), True
  return fonts[font].get(ch, []), False

def label_font(names, size):
  for name in names:
    try:
      return ImageFont.truetype(name, size)
    except OSError:
      pass
  raise RuntimeError('Install DejaVu Sans and DejaVu Sans Mono for chart labels')

sans = ['DejaVuSans.ttf', '/usr/share/fonts/msttcore/corbel.ttf']
mono = ['DejaVuSansMono.ttf', '/usr/share/fonts/msttcore/cour.ttf']
titlefont = label_font(['DejaVuSans-Bold.ttf', '/usr/share/fonts/msttcore/corbelb.ttf'], 42)
textfont = label_font(sans, 23)
codefont = label_font(mono, 21)
smallfont = label_font(sans, 19)
entries = []
def make_chart(name, font, mode=''):
  prefix = mode or switches.get(font, '')
  rows = (len(chars)+9)//10
  im = Image.new('RGB', (1800, 230+rows*150+110), '#f3f6fa')
  d = ImageDraw.Draw(im)
  d.rectangle((0,0,1800,12), fill='#166f85')
  d.text((40,30), 'sddsplot  /  '+name, font=titlefont, fill='#172b41')
  selection = ('Inline: $'+prefix+' ... '+('$e' if mode=='s' else '$r')) if prefix else 'Select with -font='+font
  d.text((40,94), selection, font=textfont, fill='#166f85')
  d.text((40,133), 'Each label is literal title/label text. Preserve $ characters in your shell or script.', font=textfont, fill='#34465a')
  d.text((40,171), 'Glyphs fitted individually; sizes are not typographic scale. Amber dot: legacy glyph mapping.', font=smallfont, fill='#536579')
  for i, ch in enumerate(chars):
    x, y = 40+(i%10)*172, 230+(i//10)*150
    d.rounded_rectangle((x,y,x+160,y+138), radius=10, fill='white', outline='#d9e2ec')
    paths, old = shape(font, ch, mode)
    points = [p for path in paths for p in path]
    if points:
      xmin,xmax = min(p[0] for p in points),max(p[0] for p in points)
      ymin,ymax = min(p[1] for p in points),max(p[1] for p in points)
      scale = min(92/max(xmax-xmin,1),66/max(ymax-ymin,1),3.4)
      for path in paths:
        pts = [(x+80+(a-(xmin+xmax)/2)*scale, y+51+(b-(ymin+ymax)/2)*scale) for a,b in path]
        if len(pts)>1: d.line(pts, fill='#182d45', width=2)
    else:
      d.text((x+80,y+47), 'blank', anchor='mm', font=smallfont, fill='#8794a4')
    literal = '$$' if ch=='$' else ch
    code = ('$'+prefix+literal+('$e' if mode=='s' else '$r')) if prefix else literal
    d.text((x+80,y+110), code, anchor='mm', font=codefont, fill='#166f85')
    if old: d.ellipse((x+145,y+9,x+151,y+15), fill='#bf791a')
  bottom = 230+rows*150+10
  d.text((40,bottom), 'Space is blank. Literal ~ and backslash are unsupported. $y3$r draws an arc, not a tilde.', font=textfont, fill='#34465a')
  d.text((40,bottom+36), 'Source: SDDSaps/sddsplots/{hershey.font, psymbol.c, graphics.c}  |  10 September 2026', font=smallfont, fill='#536579')
  file = name.replace(' ', '_')+'.png'
  im.save(OUT/file)
  entries.append((name,file,selection))

for font in fonts: make_chart(font,font)
make_chart('original_Greek', 'rowmans', 'g')
make_chart('original_special', 'rowmans', 's')
intro = '''<h1>sddsplot character charts</h1><p>32 named fonts and the original Greek and special-symbol sets. Click a chart to open its full-resolution PNG.</p><p>Codes are title/label text, not complete shell commands. Preserve dollar signs; escape commas and quotes as required by the sddsplot option parser and your calling language. A space renders blank. Literal tilde and backslash are unsupported. Glyphs are fitted individually for legibility, not shown at their relative typographic sizes.</p><p>For inline switches, $r restores your configured default font; $e ends the original special-symbol mode. Amber dots mark legacy glyph mappings, which can override the selected font. Charts assume normal text mode on entry.</p>'''
page = '<!doctype html><meta charset="utf-8"><title>sddsplot character charts</title><style>body{font:18px system-ui;max-width:1400px;margin:40px auto;padding:0 24px;background:#f3f6fa;color:#172b41}nav{display:flex;flex-wrap:wrap;gap:12px}a{color:#166f85}section{margin:48px 0}img{width:100%;border:1px solid #d9e2ec}code{font-size:16px}</style>'+intro
page += '<nav>'+''.join(f'<a href="#{n}">{n}</a>' for n,_,_ in entries)+'</nav>'
for name,file,selection in entries:
  page += f'<section id="{name}"><h2>{name}</h2><p><code>{html.escape(selection)}</code></p><a href="{file}"><img loading="lazy" src="{file}" alt="Character chart for {name}"></a></section>'
(OUT/'index.html').write_text(page)
with zipfile.ZipFile(OUT/'sddsplot-character-charts.zip','w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(OUT.iterdir()):
    if f.suffix in ('.png','.html','.md','.py'): z.write(f,f.name)
print(f'Generated {len(entries)} charts in {OUT}')
