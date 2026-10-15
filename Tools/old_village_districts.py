"""Grounded building compounds with clear roads and non-blocking small decor."""
import math
import random
import unreal as u


def ground_actor(actor, height=0):
    center, extent = actor.get_actor_bounds(False)
    position = actor.get_actor_location()
    position.z += height - (center.z-extent.z)
    actor.set_actor_location(position, False, False)
    tags = list(actor.get_editor_property('tags'))
    tags.append('GroundedProp' if height == 0 else 'SupportedProp')
    actor.set_editor_property('tags', tags)
    return actor


def dress_districts(b):
    static, own, tree, line, fence, house = [b[k] for k in ['static','own','tree','line_own','fence','modular_house']]
    S, CFG, J, V = [b[k] for k in ['S','CFG','J','V']]
    rng = random.Random(913)
    def point(p): return (p[0]*S,p[1]*S)
    def prop(path,label,e,n,scale=1,yaw=0,folder='30_Village',solid=False,height=0):
        actor=ground_actor(static(path,label,e,n,0,scale,yaw,folder,solid),height)
        if not solid:
            actor.static_mesh_component.set_cull_distance(6000)
            if label in ['Planted garden bed','Compound bamboo screen','Water lilies']:
                actor.static_mesh_component.set_editor_property('cast_shadow',False)
        return actor
    def kit(name,e,n,scale=1,angle=0,folder='10_Shrine',solid=True,height=0,label=None):
        return ground_actor(own(name,e,n,0,scale,angle,folder,label,solid),height)
    def foliage(e,n,folder):
        # A planted bed at a building edge, not loose collision obstacles.
        for i in range(9):
            angle=i*2.39996
            radius=45+18*i
            prop(J+'SM_PlantainLily01a','Planted garden bed',e+math.cos(angle)*radius,n+math.sin(angle)*radius,
                 rng.uniform(1.5,2.8),rng.uniform(0,360),folder)
        prop(J+'SM_Rock_Ground_01a','Garden bed edging',e+160,n+60,1.6,25,folder)

    # Keep landmarks grouped and seat the sacred stone on its actual support.
    e,n=point((-200,0))
    altar=kit('AltarPlatform',e,n,1.6,folder='10_Shrine/Central altar')
    center,extent=altar.get_actor_bounds(False)
    kit('SacredStone',e,n,1.55,folder='10_Shrine/Central altar',height=center.z+extent.z-2)
    for de,dn in [(-470,-260),(470,-260)]:
        prop(J+'SM_LampStone01a','Altar entrance lantern',e+de,n+dn,1.5,0,'10_Shrine/Central altar')
    e,n=point((1300,2800))
    medallion=kit('CourtyardMedallion',e,n,(1.65,1.65,.03),solid=False,height=-1,folder='10_Shrine/Court paving')
    # Low paving remains visual only, so uneven generated triangles cannot snag dashes.
    for e0 in [-250,1450]:
        e,n=point((e0,4400));kit('GuardianStatue',e,n,1.3,folder='10_Shrine/Main shrine entrance')
    e,n=point((600,4400));kit('OfferingBox',e,n,1.7,folder='10_Shrine/Main shrine entrance',solid=False)

    # Continuous boundary rhythm; actual play limits remain hidden solid walls.
    for a,z,folder in [((-6300,6250),(6300,6250),'North'),((6300,6000),(6300,-6200),'East'),
                       ((-6300,6000),(-6300,-2300),'West'),((-2300,-6250),(6200,-6250),'South')]:
        line('RoofedBoundaryWall',point(a),point(z),470,folder='60_Boundary/'+folder)
    line('RetainingWallStraight',point((-6525,-2525)),point((-2525,-6525)),390,z=-125,folder='50_Waterfront')
    for e0,n0,angle in [(-6380,-2280,0),(-2300,-6340,180)]:
        e,n=point((e0,n0));own('RetainingWallCorner',e,n,-135,1,angle,'50_Waterfront')
    for i in range(8):
        e,n=point((-5200+i*220,-3500-i*220));kit('WaterfrontWalkway',e,n,angle=45,folder='50_Waterfront',solid=False,height=-15)
    for i in range(28):
        e,n=point((-6550+i*145,-2550-i*145))
        prop(J+'SM_Rock_Ground_01a','Shoreline rock',e,n,rng.uniform(2,3),rng.uniform(0,360),'50_Waterfront/Shoreline',height=-180)

    train=CFG['training']
    for a,z in [(train[2],train[3]),(train[3],train[4]),(train[4],train[5]),(train[5],train[0])]:fence(a,z)
    for e0 in [-900,0,900]:
        e,n=point((e0,-4700))
        prop(J+'SM_Katana_Holder','Practice weapons',e,n,1.6,90,'40_Training/Equipment')
        prop(J+'SM_Sack_01','Practice sack',e+100,n+170,1.3,0,'40_Training/Equipment')
        prop(J+'SM_Bench_01a','Training rest bench',e-200,n-150,1.4,0,'40_Training/Equipment')

    e,n=point((-4600,3500))
    static('/Engine/BasicShapes/Cylinder','Garden pond water',e,n,4,(7,11,.03),0,'20_Gardens/Pond',False,b['watermat'])
    for i in range(24):
        t=i*math.tau/24
        prop(J+'SM_PondRock01a','Pond rim stone',e+560*math.cos(t),n+360*math.sin(t),(2,1.6,1.5),-math.degrees(t),'20_Gardens/Pond')
    for de,dn in [(-200,0),(150,150),(-80,-200)]:
        prop(J+'SM_Plant_WaterLilies_Group_01a','Water lilies',e+de,n+dn,2,0,'20_Gardens/Pond',height=5)
    pond=static('/Engine/BasicShapes/Cylinder','Pond collision',e,n,60,(6.6,10.6,1.2),0,'70_Collision',True,b['stonepaint'])
    pond.set_actor_hidden_in_game(True);pond.static_mesh_component.set_visibility(False)
    tree(e-750,n+650,1000,False);tree(e+850,n+650,1100,True)
    prop(J+'SM_Bench_01a','Pond overlook bench',e+850,n,1.6,90,'20_Gardens/Pond seating')
    prop(J+'SM_LampStone01a','Pond seating lantern',e+850,n-220,1.6,0,'20_Gardens/Pond seating')
    foliage(e+900,n+330,'20_Gardens/Pond seating')

    # Each compound owns its props: storage yard, cart, planted bed, well or
    # seating, and an entrance lantern. Local offsets stay human scale.
    compounds = list(CFG['buildings'])
    for index,(name,e,n,angle,scale,base) in enumerate(compounds):
        if name in ['MainShrine','SmallShrine','ShrineGatehouse']:
            continue
        folder=base+'/Compound %02d - %s'%(index+1,name)
        bounds=b['load'](b['IMPORTED'][name]).get_bounds()
        # Conservative square yard offsets avoid overlapping rotated buildings.
        clearance=max(bounds.box_extent.x,bounds.box_extent.y)*scale
        side=clearance+230
        direction=-1 if e>0 else 1
        yard_e=e+direction*side
        prop(J+'SM_Cart01','Household cart',yard_e,n-180,1.25,angle+90,folder)
        for j,(de,dn) in enumerate([(0,150),(130,170),(-50,260)]):
            prop(J+('SM_PotUnSoiled01a' if j%2==0 else 'SM_PotUnSoiled02a'),
                 'Storage pottery',yard_e+direction*de,n+dn,1.3+j*.15,angle,folder)
        for j in range(3):
            prop(V+'Props/Furniture/SM_SM_Barrel_01','Workshop storage barrel',
                 e-direction*(clearance+90+j*95),n+100,1,angle+j*10,folder)
        prop(J+'SM_Sack_01','Stored grain sacks',yard_e+direction*170,n-300,1.2,angle,folder)
        prop(J+'SM_LampStone01a','Compound entrance lantern',e+direction*(clearance+100),n-clearance-120,1.4,0,folder)
        # Wells belong to shared yards, never isolated on a road.
        if index%3==0:
            prop(J+'SM_Well01a','Shared household well',yard_e+direction*410,n+120,1.25,0,folder,solid=True)
        else:
            prop(J+'SM_Bench_01a','Household garden bench',yard_e+direction*270,n+330,1.3,90,folder)
        te=e-direction*(clearance+230);tn=n-clearance-230
        tree(te,tn,750+index%3*90,index%3==0)
        foliage(te,tn,folder+'/Planted bed')
        for j in range(3):
            prop(J+'SM_TreeBamboo01a','Compound bamboo screen',e-direction*(clearance+150),n+clearance+j*120,.65,0,folder)

    for e0,n0,thatch in [(5500,650,False),(5300,-3400,True)]:
        e,n=point((e0,n0));house(e,n,thatch)
