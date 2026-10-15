"""Authored districts and clustered scenery for revision two of the three maps."""
import math,random
import unreal as u
from survival_map_layout import MAPS,POOLS,boundary,playable,reserved,edge_distance,backdrop_height,inside_polygon,route_distance

J='/Game/Assets/Environment/JapaneseShrine/Meshes/'
R='/Game/Assets/Environment/Ancient_Ruins/Meshes/'
G='/Game/Assets/Environment/GreenCave/Meshes/'
V='/Game/Assets/Environment/Stylized_Village/Assets/'
O='/Game/HeavensDivide/Environment/OldVillage/Meshes/'
used=set();occupied=[];batches={};b=None;rockmat=None;detail_instance_count=0

def configure(api):
 global b,used,occupied,batches,rockmat,detail_instance_count
 b=api;used=set();occupied=[];batches={};rockmat=None;detail_instance_count=0

def p(path,label,x,y,h=None,s=1,yaw=0,z=0,solid=True,box=False,folder='30_Districts',mat=None):
 used.add(path)
 a=b.prop(path,label,x,y,height=h,scale=s,yaw=yaw,z=z,solid=solid,collision_box=box,folder=folder)
 if mat:
  if folder not in ['95_Backdrop','60_Boundary']:mat=b.fade_material(mat,b.fade_cache)
  for i in range(a.static_mesh_component.get_num_materials()):a.static_mesh_component.set_material(i,mat)
 if folder in ['95_Backdrop','60_Boundary']:
  a.set_editor_property('tags',['SurvivalMapBackdrop']);a.static_mesh_component.set_collision_profile_name('NoCollision');a.static_mesh_component.set_cast_shadow(False)
 elif z==0:a.set_editor_property('tags',['SurvivalGrounded'])
 return a

def site(x,y,r=800):occupied.append((x,y,r))

def free(x,y,r=100):
 return playable(b.name,x,y,r) and edge_distance(b.name,x,y)>r+180 and not reserved(b.name,x,y,r) and all(math.hypot(x-a,y-c)>r+d for a,c,d in occupied)

def batch(path,label,x,y,h,z=0,folder='20_Planting'):
 used.add(path);key=(path,label,folder)
 if key not in batches:batches[key]=([],[])
 batches[key][0].append((x,y,z));batches[key][1].append(h)

def low_plants(x,y,r=430,count=15,kind='forest'):
 paths=([G+'SM_Fern_a',J+'SM_PlantainLily01a',G+'SM_SmallMushrooms_b',G+'SM_Clover_Cluster'] if kind=='forest' else
        [R+'SM_Leafy_Foliage_1',R+'SM_Leafy_Foliage_3',R+'SM_Blue_Flower',R+'SM_Grass'] if kind=='ruin' else
        [G+'SM_SmallCrystals',G+'SM_FlatStone_a',G+'SM_FlatStone_b'])
 for j in range(count):
  t=b.rng.uniform(0,math.tau);d=b.rng.uniform(r*.25,r);px=x+math.cos(t)*d;py=y+math.sin(t)*d
  if not playable(b.name,px,py,60) or reserved(b.name,px,py,-150):continue
  batch(paths[j%len(paths)],'Grouped '+kind+' groundcover',px,py,b.rng.uniform(28,65))

def bush(x,y,h=135,kind='forest'):
 path=V+'Foliage/Bushes/SM_Bush_01' if kind=='forest' else R+'SM_Bush'
 p(path,'Shrub bed',x,y,h,box=True,folder='20_Planting/Shrubs')

def leafy_tree(x,y,h=1450,variant=0,folder='20_Planting/Trees'):
 path=V+'Foliage/Trees/'+['Tree_Medium_Green','Tree_Large_Green','Tree_Small_Green'][variant%3]
 m=b.copied_mesh(path,True);used.add(path)
 s=h/(m.get_bounds().box_extent.z*2)
 a=b.mesh_actor(m,'Broadleaf canopy',x,y,0,s,b.rng.uniform(0,360),folder,False)
 a.static_mesh_component.set_cull_distance(16000)
 b.proxy('Solid tree trunk',x,y,140 if variant%3!=2 else 95,140 if variant%3!=2 else 95,500,round=True)
 return a

def lamp(x,y,forest=True):
 p(J+'SM_LampStone01a' if forest else G+'SM_StandingLantern','Lantern at rest stop',x,y,200 if forest else 260,box=True)

def accent(x,y,color=(125,190,255),intensity=180,radius=850):
 a=b.actor(u.PointLight,'Rest stop light',x,y,230,folder='90_Lighting/Local accents')
 b.put(a.get_component_by_class(u.PointLightComponent),mobility=u.ComponentMobility.MOVABLE,intensity=float(intensity),attenuation_radius=float(radius),cast_shadows=False,light_color=u.Color(r=color[0],g=color[1],b=color[2],a=255))

def storage(x,y,yaw=0,folder='30_Districts/Storage'):
 # Large obstacles form one intentional supply stack. Tiny tools sit on it.
 for dx,dy,path,h in [(0,0,'SM_Box_a',120),(135,0,'SM_Box_b',105),(10,140,'SM_Barrel_a',125),(145,145,'SM_Bag_b',65)]:
  p(G+path,'Supply stack',x+dx,y+dy,h,yaw=yaw,box=True,folder=folder)
 p(G+'SM_Box_c','Top crate',x,y,95,z=120,yaw=yaw,box=True,folder=folder)
 p(G+'SM_ToolBox','Tools on crate',x+135,y,40,z=105,yaw=yaw,solid=False,folder=folder)

def rock(x,y,h=450,kind='forest',yaw=0,z=0,solid=True,folder='20_Landforms'):
 path=(R+'SM_Rock_Large_1' if kind=='ruin' else G+'SM_StoneArray_a' if kind=='quarry' else J+'SM_Rock_Ground_01a')
 return p(path,'Moss boulder' if kind!='quarry' else 'Layered sandstone',x,y,h,yaw=yaw,z=z,solid=solid,box=solid and kind!='quarry',folder=folder,mat=rockmat if kind=='quarry' else None)

def perimeter():
 points=boundary(b.name)
 for i,(a,c) in enumerate(zip(points,points[1:]+points[:1])):
  length=math.dist(a,c);x=(a[0]+c[0])/2;y=(a[1]+c[1])/2
  b.proxy('Sealed play boundary %02d'%i,x,y,length+220,150,1800,math.degrees(math.atan2(c[1]-a[1],c[0]-a[0])),z=-100)
  # Vary outside scenery so the perimeter reads as a landscape, not a rock fence.
  count=max(1,round(length/680))
  for j in range(count):
   t=(j+.5)/count;px=a[0]+(c[0]-a[0])*t;py=a[1]+(c[1]-a[1])*t
   mag=math.hypot(px,py);px*=1+300/mag;py*=1+300/mag
   if b.name=='MoonlitForest':
    batch(V+'Foliage/Trees/'+('Tree_Large_Green' if j%3 else 'Tree_Medium_Green'),'Woodland perimeter',px*1.035,py*1.035,b.rng.uniform(760,1120),z=-45,folder='95_Backdrop')
    if j%2:rock(px,py,650,yaw=b.rng.uniform(0,360),z=-60,solid=False,folder='60_Boundary')
   elif b.name=='SunkenSanctuary':
    rock(px,py,b.rng.uniform(500,900),'ruin',b.rng.uniform(0,360),-120,False,'60_Boundary')
    if j%2==0:batch(V+'Foliage/Trees/Tree_Large_Green','Outer garden canopy',px+200,py+200,b.rng.uniform(1800,2400),z=-50,folder='95_Backdrop')
   else:
    from polish_survival_obstacles import fit_boundary_cliff
    original_height=b.rng.uniform(2400,3400)
    a=p(G+'SM_LargeWall_b','Canyon escarpment',px*1.025,py*1.025,original_height,yaw=math.degrees(math.atan2(py,px))+90,z=-180,solid=False,folder='60_Boundary',mat=rockmat)
    fit_boundary_cliff(a,min(800,original_height*.26))

def forest():
 # Houses are assembled as recognizable pockets off the three braided trails.
 for x,y,yaw,model,label in [(-4000,-4400,28,'ThatchedCottage','Ranger cottage'),(-3900,3150,-20,'SmallShrine','Moss shrine'),(3600,2800,145,'ThatchedShelter','Pilgrims rest'),(1800,-6000,8,'RaisedStorehouse','Woodland supply hut')]:
  site(x,y,850);folder='30_Districts/'+label
  p(O+'Buildings/SM_OV_'+model,label,x,y,s=1.,yaw=yaw,box=model!='ThatchedShelter',folder=folder)
  p(J+'SM_Cart01','Parked supply wagon',x+650,y-150,180,yaw=yaw+30,box=True,folder=folder)
  p(J+'SM_Well01a','Rest stop well',x-550,y-250,300,box=True,folder=folder)
  p(J+'SM_Bench_01a','Resting bench',x+100,y-620,95,yaw=yaw,box=True,folder=folder)
  storage(x+480,y+400,yaw,folder)
  for dx,dy in [(-650,450),(450,700),(-800,50)]:leafy_tree(x+dx,y+dy,1250,variant=1);bush(x+dx+150,y+dy-120)
  low_plants(x,y,1050,45)
  lamp(x+480,y-420);accent(x+480,y-420,(255,185,100),150)
 # Landmark shrine at the upper lobe; wide approach remains clear.
 x,y=-800,6100;site(x,y,1050)
 p(O+'Buildings/SM_OV_MainShrine','Old moon temple',x,y,s=.8,yaw=180,box=True,folder='10_Landmarks/Moon temple')
 for dx in [-900,900]:
  p(O+'Landmarks/SM_OV_GuardianStatue','Paired moon guardians',x+dx,y-700,250,box=True,folder='10_Landmarks/Moon temple')
  lamp(x+dx,y-950);bush(x+dx,y+50)
 # Natural boulder gardens subdivide the centre instead of a uniform tree scatter.
 grove_centres=[(-900,-1800),(2100,-1900),(-2600,1100),(1100,2300),(-6000,3000),(5700,-2300),(4500,-5400)]
 for i,(x,y) in enumerate(grove_centres):
  if not free(x,y,300):continue
  site(x,y,650);rock(x,y,360,yaw=i*57)
  for j,(dx,dy) in enumerate([(-380,-120),(340,240),(-170,480)]):leafy_tree(x+dx,y+dy,1450+j*100,(i+j)%3)
  for dx,dy in [(-420,300),(400,-250),(100,440)]:bush(x+dx,y+dy,b.rng.uniform(100,145))
  low_plants(x,y,850,45)
 # Bounded, varied infill preserves routes/objective clearings and avoids clutter.
 placed=[]
 for _ in range(4500):
  x=b.rng.uniform(-7300,7300);y=b.rng.uniform(-7300,7300)
  if not free(x,y,170) or any(math.hypot(x-a,y-c)<720 for a,c in placed):continue
  placed.append((x,y));leafy_tree(x,y,b.rng.uniform(1250,1900),len(placed)%3)
  if len(placed)%3==0:rock(x+180,y+170,230,yaw=b.rng.uniform(0,360))
  if len(placed)%2==0:bush(x-180,y-180,110)
  low_plants(x,y,420,12)
  if len(placed)>=105:break
 # Little wayside shrines at bends, paired with seats and planting.
 for x,y in [(-6100,-1800),(-4700,4500),(5600,1100),(1200,-3600)]:
  if not playable(b.name,x,y):continue
  p(J+'SM_ShrineLittle01a','Wayside spirit shrine',x,y,180,box=True)
  p(J+'SM_StatueJizo01a','Shrine guardian',x+150,y-100,75,box=True)
  low_plants(x,y,330,18)

def wall_run(x,y,length,yaw=0,height=450,label='Cloister wall'):
 n=max(1,round(length/450));rad=math.radians(yaw)
 for j in range(n):
  offset=(j-(n-1)/2)*length/n
  px=x+math.cos(rad)*offset;py=y+math.sin(rad)*offset
  p(R+('SM_Wall_Variant' if j%4 else 'SM_Wall_Quarter'),label,px,py,s=(length/n/300,4,height/300 if j%4 else height/180),yaw=yaw,folder='30_Districts/Architecture')
  if j%2==0:
   p(R+'SM_Ivy_1','Ivy on masonry',px,py-55,190,z=60,yaw=yaw,solid=False,folder='30_Districts/Architecture/Ivy')
  if j%3==0:low_plants(px,py-140,250,8,'ruin')

def tower(x,y,scale=1.,broken=False):
 folder='10_Landmarks/Ruined bell towers';site(x,y,700*scale)
 p(R+'SM_Tower_Base','Tower foundation',x,y,s=scale,box=True,folder=folder)
 # Central solid core guarantees the empty shell does not become a nav pocket.
 b.proxy('Tower solid core',x,y,580*scale,580*scale,1000*scale)
 for dx,dy,yaw in [(-260,-260,0),(260,-260,90),(260,260,180),(-260,260,270)]:
  p(R+'SM_Tower_Corner','Tower masonry',x+dx*scale,y+dy*scale,s=scale,yaw=yaw,z=180*scale,folder=folder)
 if not broken:p(R+'SM_Tower_Topper','Tower crown',x,y,s=scale,z=470*scale,box=True,folder=folder)
 else:
  p(R+'SM_Brick_Rubble_Corner','Collapsed tower crown',x,y,s=scale*2,z=440*scale,solid=False,folder=folder)
 bush(x+460*scale,y-290*scale,180,'ruin');low_plants(x,y,850*scale,45,'ruin')

def sanctuary(water):
 global detail_instance_count
 for i,(x,y,sx,sy) in enumerate(POOLS):
  b.proxy('Flooded basin collision',x,y,sx*2+80,sy*2+80,250,z=-30)
  # Solid dressed coping uses existing stone kit, exact width; no primitive rim.
  for cx,cy,length,yaw in [(x-sx,y,sy*2,90),(x+sx,y,sy*2,90),(x,y-sy,sx*2,0),(x,y+sy,sx*2,0)]:
   for j in range(max(1,round(length/400))):
    n=max(1,round(length/400));d=(j-(n-1)/2)*length/n;t=math.radians(yaw)
    p(R+'SM_Trim_Full','Reservoir dressed stone edge',cx+math.cos(t)*d,cy+math.sin(t)*d,s=(1.34,2,1.5),yaw=yaw,solid=False,folder='20_Water/Stone coping')
  for j in range(32):
   px=x+b.rng.uniform(-sx+180,sx-180);py=y+b.rng.uniform(-sy+180,sy-180)
   path=R+('SM_Lily_Pad_w_Flower' if j%4==0 else 'SM_Lily_Pad')
   height=b.rng.uniform(8,13) if j%4==0 else b.rng.uniform(1.3,2.4)
   batch(path,'Reservoir lilies',px,py,height,z=-40,folder='20_Water/Lilies')
  for dx,dy in [(-sx-150,-sy-150),(sx+150,sy+150)]:
   if playable(b.name,x+dx,y+dy):bush(x+dx,y+dy,170,'ruin');low_plants(x+dx,y+dy,450,18,'ruin')
 # Staggered court walls have broad gaps to the next court, with roofed arcades.
 for x,y,length,yaw in [(-7000,-1000,2600,90),(-4500,-5200,2600,0),(-6200,4500,1800,90),(-4200,6400,2900,0),(1500,6400,2000,0),(7200,1200,1600,90),(3200,-6300,2200,0),(-700,-6200,1000,0),(1000,-1200,900,0)]:wall_run(x,y,length,yaw)
 for x,y,s,broken in [(-7100,-5200,1.25,False),(-7000,5700,1.25,True),(7200,5700,1.45,False),(7200,-5100,1.15,True)]:tower(x,y,s,broken)
 # Two columned halls, beside rather than across the run routes.
 for x,y,yaw in [(-4700,1000,90),(1000,5500,0)]:
  folder='30_Districts/Roofed arcades'
  for j in range(5):
   px=x+(j-2)*430*(1 if yaw==0 else 0);py=y+(j-2)*430*(1 if yaw else 0)
   for side in [-1,1]:
    cx=px+(side*300 if yaw else 0);cy=py+(side*300 if not yaw else 0)
    p(R+'SM_Corner_Pillar','Cloister column',cx,cy,s=(3.18,3.18,2.25),yaw=yaw,box=True,folder=folder)
   if j!=3:p(R+'SM_Roof_Large','Cloister roof fragment',px,py,s=(1.25,1.8,2),yaw=yaw,z=680,solid=False,folder=folder)
  low_plants(x,y,1000,40,'ruin')
 # Shrine exedras and a fountain plinth provide memorable silhouettes.
 for x,y,yaw in [(-2500,-4700,180),(2500,3300,75),(-400,6700,0)]:
  site(x,y,700);p(R+'SM_Curved_Wall','Broken circular shrine',x,y,s=1.4,yaw=yaw,folder='10_Landmarks/Old shrines')
  plinth=p(R+'SM_Obelisk_Pedestal','Waystone plinth',x,y-150,s=1.6,folder='10_Landmarks/Old shrines')
  c,e=plinth.get_actor_bounds(False);p(R+'SM_Obelisk','Weathered waystone',x,y-150,380,z=c.z+e.z,box=True,folder='10_Landmarks/Old shrines')
  for dx in [-500,500]:bush(x+dx,y,145,'ruin')
 # Overgrowth and collapsed stone stay with walls, not across the paths.
 placed=[]
 for _ in range(2500):
  x=b.rng.uniform(-8100,8100);y=b.rng.uniform(-7000,7000)
  if not free(x,y,260) or any(math.hypot(x-a,y-c)<850 for a,c in placed):continue
  placed.append((x,y));kind=len(placed)%4
  if kind==0:leafy_tree(x,y,b.rng.uniform(1000,1600),len(placed)%3)
  elif kind==1:rock(x,y,b.rng.uniform(200,380),'ruin',b.rng.uniform(0,360))
  else:p(R+'SM_Brick_Rubble_Wall','Collapsed cloister masonry',x,y,s=(2.5,3.,1.5),yaw=b.rng.uniform(0,360),box=True,folder='30_Districts/Collapsed gardens')
  bush(x+180,y+120,120,'ruin');low_plants(x,y,600,22,'ruin')
  if len(placed)>=65:break
 for x,y in [(-6200,-4100),(6600,-3700),(-5600,5200)]:
  storage(x,y,folder='30_Districts/Archaeologists camp')
  p(R+'SM_Large_Vase','Urn beside camp crates',x+280,y,65,solid=False,folder='30_Districts/Archaeologists camp')
 from survival_ruin_details import add_details
 details=add_details(b)
 used.update(R+model for model in details['source_models'])
 detail_instance_count=details['inset_tiles']

def rail_path(points):
 # Fit each piece end-to-end from its measured local Y bounds, including pivot.
 mesh=b.copied_mesh(G+'SM_StraightRail_b');used.add(G+'SM_StraightRail_b')
 for a,c in zip(points,points[1:]):
  length=math.dist(a,c);n=max(1,round(length/680));yaw=math.degrees(math.atan2(c[1]-a[1],c[0]-a[0]))-90
  for j in range(n):
   t=(j+.5)/n;x=a[0]+(c[0]-a[0])*t;y=a[1]+(c[1]-a[1])*t
   p(G+'SM_StraightRail_b','Inset mine rails',x,y,s=(1.,length/n/708.5,.6),yaw=yaw,z=.3,solid=False,folder='20_Haul routes/Inset rails')

def quarry():
 global rockmat
 from polish_survival_obstacles import prepare_sandstone
 prepare_sandstone(b.copied_mesh(G+'SM_StoneArray_a'))
 rockmat=b.custom_material('PaintedSandstone','float3 t=Texture2DSample(Dirt,DirtSampler,float2(P.x+P.y*.31,P.z)/1200).rgb;float band=.82+.12*sin(P.z*.008+sin(P.x*.0012));return t*band;',True)
 # The long interior ridges separate winding haul loops and broad working courts.
 for x,y,dx,dy,n in [(-1700,3000,420,220,4),(2000,-2200,300,470,4),(-4700,-3300,-130,330,3)]:
  for j in range(n):
   px=x+(j-(n-1)/2)*dx;py=y+(j-(n-1)/2)*dy
   site(px,py,500);rock(px,py,650+(j%3)*180,'quarry',j*55,folder='10_Landmarks/Sandstone ridges')
   low_plants(px,py,800,22,'quarry')
  for j in range(3):p(G+'SM_SingleCrystal_Red_a','Ore in rock seam',x+300+j*80,y+270,130+j*25,box=True,folder='10_Landmarks/Ore')
 # Full work yards: a mine face, shelter, rail spur, cart, tools and supply stacks.
 for i,(x,y,yaw) in enumerate([(-5700,-6300,25),(6000,-3800,-70),(-5500,2500,90),(2800,7000,180)]):
  folder='30_Districts/'+['South loading depot','East tool yard','West mine camp','Upper ore works'][i];site(x,y,1150)
  distance=math.hypot(x,y)
  rock(x+x/distance*1000,y+y/distance*1000,1100,'quarry',yaw,folder=folder+'/Mine face')
  p(G+'SM_Entrance','Timbered mine entrance',x,y+200,700,yaw=yaw,folder=folder)
  p(O+'Buildings/SM_OV_OpenWorkshop','Miners shelter',x-750,y-100,s=.85,yaw=yaw,folder=folder)
  p(G+'SM_Cart','Parked ore wagon',x+600,y-500,200,yaw=yaw,box=True,folder=folder)
  p(G+'SM_Trolley_c','Ore trolley',x+200,y-550,140,yaw=yaw,box=True,folder=folder)
  storage(x+750,y+150,yaw,folder);storage(x-700,y-700,yaw,folder)
  table=p(G+'SM_Table','Tool workbench',x-350,y-550,100,yaw=yaw,box=True,folder=folder)
  p(G+'SM_Pickaxe','Pickaxe on workbench',x-350,y-550,38,z=100,yaw=yaw,solid=False,folder=folder)
  p(G+'SM_Toolshelf','Tool rack',x-550,y-300,160,yaw=yaw,box=True,folder=folder)
  p(G+'SM_BlastingMachine','Mining winch',x+400,y+300,110,box=True,folder=folder)
  lamp(x+400,y-800,False)
  low_plants(x,y,1350,60,'quarry')
 rail_path([(-5700,-6000),(-2800,-5500),(-1600,-2600),(0,0),(2600,3300),(2800,6400)])
 rail_path([(0,0),(3500,-1000),(6000,-3400)])
 # Dusty, rocky banks with dry trees; all large rocks block movement.
 placed=[]
 for _ in range(2500):
  x=b.rng.uniform(-7600,7600);y=b.rng.uniform(-8800,8800)
  if not free(x,y,300) or any(math.hypot(x-a,y-c)<950 for a,c in placed):continue
  placed.append((x,y));rock(x,y,b.rng.uniform(250,500),'quarry',b.rng.uniform(0,360))
  if len(placed)%3==0:p(G+'SM_DeadTree_a','Dry canyon tree',x+230,y-160,550,box=True,folder='20_Planting/Dry trees')
  low_plants(x,y,650,18,'quarry')
  if len(placed)>=70:break

def distant_world():
 # Dense near horizon, sparse large silhouettes farther away. Both grounded.
 for _ in range(720):
  x=b.rng.uniform(-19000,19000);y=b.rng.uniform(-20000,21000)
  d=edge_distance(b.name,x,y)
  if inside_polygon(x,y,boundary(b.name)) or d<700 or d>9500:continue
  z=backdrop_height(b.name,x,y)-30
  if b.name!='EmberQuarry':
   height=b.rng.uniform(1800,3000)
   if b.name=='MoonlitForest' and d<1800:height=min(1400,height)
   batch(V+'Foliage/Trees/'+('Tree_Large_Green' if b.rng.random()<.75 else 'Tree_Medium_Green'),'Distant woodland',x,y,height,z,folder='95_Backdrop')
  elif d<5500 and b.rng.random()<.33:
   height=b.rng.uniform(3000,5500)
   if d<2200:height=min(1500,height)
   a=p(G+'SM_LargeWall_b','Distant canyon formation',x,y,height,yaw=b.rng.uniform(0,360),z=z-250,solid=False,folder='95_Backdrop',mat=rockmat)
   a.set_editor_property('tags',['SurvivalMapBackdrop']);a.static_mesh_component.set_collision_profile_name('NoCollision');a.static_mesh_component.set_cast_shadow(False)
   from polish_survival_obstacles import clear_distant_cliff
   clear_distant_cliff(a)

def build(water):
 if b.name=='EmberQuarry':quarry()
 elif b.name=='MoonlitForest':forest()
 else:sanctuary(water)
 perimeter();distant_world()
 count=detail_instance_count
 for (path,label,folder),(positions,heights) in batches.items():
  material=rockmat if b.name=='EmberQuarry' and 'SM_FlatStone' in path else None
  b.instances(b.copied_mesh(path,tree='Trees/' in path),label,positions,heights,folder=folder,mat=material);count+=len(positions)
 return count
