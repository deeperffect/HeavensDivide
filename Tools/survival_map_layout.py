"""Reworked survival maps: authored routes and sites, in Unreal centimetres."""
import math

MAPS = {
 'MoonlitForest': dict(title='Moonlit Forest', seed=1801, size=(17000,17000), start=(-2600,-3000), gate=(2300,5200),
   goals=[(-2600,-3000),(-4900,600),(-2900,4400),(2300,4600),(4900,500),(3000,-4200),(0,0),(-5900,-3500)],
   paths=[[(-5900,-3500),(-2600,-3000),(3000,-4200),(4900,500),(2300,4600),(-2900,4400),(-4900,600),(-5900,-3500)],
          [(-2600,-3000),(0,0),(2300,4600)],[(-4900,600),(0,0),(4900,500)]],
   sun=2.4, sky=1.65, exposure=.55, temperature=10000, yaw=-35,
   horizon=(.035,.07,.15), zenith=(.009,.017,.065), ground=(.14,.28,.22), path=(.52,.55,.53), fog=(.06,.12,.20),
   views=[(-2600,-3000),(-3400,3900),(4200,700)]),
 'SunkenSanctuary': dict(title='Sunken Sanctuary', seed=1802, size=(18000,16000), start=(-5700,-3000), gate=(6000,5200),
   goals=[(-5700,-3000),(-5700,2200),(-2200,5100),(1600,1200),(5500,-3000),(6000,5200),(0,-5400),(0,3600)],
   paths=[[(-5700,-3000),(-5700,2200),(-2200,5100),(0,3600),(1600,1200),(5500,1200),(6000,5200)],
          [(-5700,-3000),(0,-5400),(5500,-3000),(5500,1200)],
          [(-5700,-3000),(-2500,-3000),(1600,1200)],[(1600,1200),(0,3600)],[(0,3600),(6000,5200)]],
   sun=4.2,sky=1.35,exposure=.1,temperature=6200,yaw=-25,
   horizon=(.31,.48,.50),zenith=(.10,.27,.40),ground=(.29,.39,.28),path=(.68,.66,.52),fog=(.25,.43,.45),
   views=[(-5700,-3000),(1500,1200),(5700,5100)]),
 'EmberQuarry': dict(title='Ember Quarry', seed=1803, size=(18000,20000), start=(-3200,-6000), gate=(1000,7400),
   goals=[(-3200,-6000),(2800,-5800),(5100,-1200),(3100,4600),(1000,7400),(-4200,4000),(-5300,-800),(0,0)],
   paths=[[(-3200,-6000),(2800,-5800),(5100,-1200),(3100,4600),(1000,7400),(-4200,4000),(-5300,-800),(-3200,-6000)],
          [(-3200,-6000),(-1600,-2600),(0,0),(3100,4600)],[(-5300,-800),(0,0),(5100,-1200)]],
   sun=5.,sky=1.15,exposure=.05,temperature=5100,yaw=-115,
   horizon=(.61,.39,.27),zenith=(.24,.26,.37),ground=(.42,.23,.13),path=(.83,.55,.29),fog=(.46,.30,.22),
   views=[(-3200,-6000),(4300,-1400),(1000,6100)])}

# Asymmetric reservoir, reflecting court and a separate garden basin.
# Exact 200 cm grid edges match generated terrain and watertight collision.
POOLS=[(-2000,800,1600,2200),(2200,-3200,1800,1000),(5200,3100,1200,1100)]

def boundary(name,count=64):
 if name=='MoonlitForest':
  return [(math.cos(t)*(7650+450*math.sin(t*3)+230*math.cos(t*5)),math.sin(t)*(7650+450*math.sin(t*3)+230*math.cos(t*5))) for t in [i*math.tau/count for i in range(count)]]
 if name=='SunkenSanctuary':return [(-9000,-5800),(-7400,-8000),(6400,-8000),(9000,-5100),(9000,6000),(7000,8000),(-7100,8000),(-9000,5600)]
 return [(-6100,-10000),(3600,-10000),(7300,-7000),(8800,-1800),(7100,4200),(4300,9000),(800,10000),(-4900,8000),(-8000,2800),(-8700,-2400)]

def inside_polygon(x,y,p):
 inside=False
 for (ax,ay),(bx,by) in zip(p,p[1:]+p[:1]):
  if (ay>y)!=(by>y) and x<(bx-ax)*(y-ay)/(by-ay)+ax:inside=not inside
 return inside

def playable(name,x,y,margin=0):
 if not inside_polygon(x,y,boundary(name)):return False
 if name=='SunkenSanctuary' and any(abs(x-cx)<sx+margin and abs(y-cy)<sy+margin for cx,cy,sx,sy in POOLS):return False
 return True

def segment_distance(p,a,b):
 dx=b[0]-a[0];dy=b[1]-a[1]
 t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/max(1,dx*dx+dy*dy)))
 return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)

def route_distance(name,x,y):
 return min(segment_distance((x,y),a,b) for path in MAPS[name]['paths'] for a,b in zip(path,path[1:]))

def reserved(name,x,y,radius=0):
 cfg=MAPS[name]
 return route_distance(name,x,y)<520+radius or any(math.hypot(x-a,y-b)<800+radius for a,b in cfg['goals']) or math.dist((x,y),cfg['gate'])<650+radius

def edge_distance(name,x,y):
 p=boundary(name)
 return min(segment_distance((x,y),a,b) for a,b in zip(p,p[1:]+p[:1]))

def backdrop_height(name,x,y):
 if inside_polygon(x,y,boundary(name)):return -500
 d=edge_distance(name,x,y)
 lift=min(3000,max(0,d-200)*.32)
 return -100+lift*(.65+.22*math.sin(x*.0004)+.13*math.cos(y*.0006))
