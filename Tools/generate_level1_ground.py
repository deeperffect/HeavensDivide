"""Bake authored world-space ground weights (R grass, G dirt, B stone).
Uses only Python's standard library. Re-run after editing level1_ground_layout.json.
The output is a data mask; original painted surface textures are untouched.
"""
from pathlib import Path
import array,json,math,struct,zlib
ROOT=Path(__file__).resolve().parents[1]
cfg=json.loads((ROOT/'Tools/level1_ground_layout.json').read_text())
N=cfg['resolution']; ox,oy=cfg['world_min']; sx,sy=cfg['world_size']; step=sx/N
D=array.array('f',[0])*(N*N); S=array.array('f',[0])*(N*N); I=array.array('f',[0])*(N*N)
def smooth(a,b,x):
 t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
def noise(x,y):
 ix=math.floor(x);iy=math.floor(y);fx=x-ix;fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy)
 def h(a,b):
  v=math.sin(a*127.1+b*311.7)*43758.5453;return v-math.floor(v)
 return (h(ix,iy)*(1-fx)+h(ix+1,iy)*fx)*(1-fy)+(h(ix,iy+1)*(1-fx)+h(ix+1,iy+1)*fx)*fy
def bounds(x0,y0,x1,y1):return range(max(0,int((x0-ox)/step)),min(N,int((x1-ox)/step)+1)),range(max(0,int((y0-oy)/step)),min(N,int((y1-oy)/step)+1))
def stroke(a,b,width,stone):
 ax,ay=a;bx,by=b;dx=bx-ax;dy=by-ay;dd=dx*dx+dy*dy;rad=width*.5
 xs,ys=bounds(min(ax,bx)-rad-110,min(ay,by)-rad-110,max(ax,bx)+rad+110,max(ay,by)+rad+110)
 for j in ys:
  y=oy+(j+.5)*step
  for i in xs:
   x=ox+(i+.5)*step;t=max(0,min(1,((x-ax)*dx+(y-ay)*dy)/dd));dist=math.hypot(x-ax-t*dx,y-ay-t*dy)
   wobble=(noise(x/130,y/130)-.5)*65+(noise(x/490+9,y/490)-.5)*80
   weight=1-smooth(rad*.75,rad+75,dist+wobble);idx=j*N+i
   D[idx]=max(D[idx],weight);S[idx]=max(S[idx],weight*stone)
def spline(points):
 result=[]
 for k in range(len(points)-1):
  p0=points[max(0,k-1)];p1=points[k];p2=points[k+1];p3=points[min(len(points)-1,k+2)]
  count=max(3,int(math.dist(p1,p2)/110))
  for j in range(count):
   t=j/count;result.append(tuple(.5*((2*p1[d])+(-p0[d]+p2[d])*t+(2*p0[d]-5*p1[d]+4*p2[d]-p3[d])*t*t+(-p0[d]+3*p1[d]-3*p2[d]+p3[d])*t*t*t) for d in range(2)))
 return result+[points[-1]]
for route in cfg['paths']:
 pts=spline(route['points'])
 for a,b in zip(pts,pts[1:]):stroke(a,b,route['width'],route['stone'])
def ellipse(item,kind):
 cx,cy=item['center'];rx,ry=item['radius'];xs,ys=bounds(cx-rx*1.4,cy-ry*1.4,cx+rx*1.4,cy+ry*1.4)
 for j in ys:
  y=oy+(j+.5)*step
  for i in xs:
   x=ox+(i+.5)*step;dist=math.hypot((x-cx)/rx,(y-cy)/ry)+(noise(x/165+7,y/165+3)-.5)*.2+(noise(x/410,y/410)-.5)*.14
   w=1-smooth(.72,1.15,dist);idx=j*N+i
   if kind=='clearing':D[idx]=max(D[idx],w*.96);S[idx]=max(S[idx],w*item['stone'])
   elif kind=='rock':S[idx]=max(S[idx],w*.86);D[idx]=max(D[idx],w*.75)
   else:I[idx]=max(I[idx],w*.96)
for item in cfg['clearings']:ellipse(item,'clearing')
for item in cfg['rock_patches']:ellipse(item,'rock')
for item in cfg['grass_islands']:ellipse(item,'grass')
raw=bytearray();preview=bytearray();totals=[0.,0.,0.]
for j in range(N):
 raw.append(0);preview.append(0);y=oy+(j+.5)*step
 for i in range(N):
  idx=j*N+i;x=ox+(i+.5)*step;n=noise(x/260+14,y/260-23);fine=noise(x/82,y/82)
  # Low-wear ground grows grass; small irregular bare spots cluster off the route.
  bare=smooth(.63,.87,n)*.42;d=max(D[idx],bare);s=S[idx]
  # Broken flagstone in forecourts, with earth joints and grass recolonization.
  s*=.78+.22*smooth(.22,.66,fine);d=max(d,s)
  g=max((1-d)*(1-s),I[idx]);s*=1-I[idx];d=max(0,1-g-s)
  w=[g,d,s];w=[round(v/sum(w)*255) for v in w];w[1]=255-w[0]-w[2]
  raw.extend(w)
  for k,v in enumerate(w):totals[k]+=v/255
  colors=[(83,104,53),(148,116,75),(116,124,117)]
  preview.extend(round(sum(w[k]*colors[k][c]/255 for k in range(3))) for c in range(3))
def png(filename,data):
 def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
 filename.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',N,N,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b''))
out=ROOT/'Art/EnvironmentPolish';out.mkdir(parents=True,exist_ok=True)
png(out/'Lvl1_GroundWeights.png',raw);png(out/'Lvl1_LayoutPreview.png',preview)
report={'resolution':N,'world_min':cfg['world_min'],'world_size':cfg['world_size'],'weights':dict(zip(['grass','dirt','stone'],[v/(N*N) for v in totals])),'routes':[p['name'] for p in cfg['paths']]}
(out/'LayoutReport.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
