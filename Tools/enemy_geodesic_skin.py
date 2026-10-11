"""Surface-distance skinning: nearby hands cannot capture disconnected feet/robes."""
import heapq,math
from mathutils import Vector
from mathutils.kdtree import KDTree
def skin(obj,arm,name):
    vertices=obj.data.vertices;n=len(vertices);points=[v.co.copy() for v in vertices]
    adj=[{} for _ in vertices]
    for e in obj.data.edges:
        a,b=e.vertices;d=max(1e-6,(points[a]-points[b]).length);adj[a][b]=d;adj[b][a]=d
    # Logical weld at UV seams, without changing topology or texture coordinates.
    weld={}
    for i,p in enumerate(points):
        key=tuple(round(c,5) for c in p)
        if key in weld:j=weld[key];adj[i][j]=1e-6;adj[j][i]=1e-6
        else:weld[key]=i
    components=[];unseen=set(range(n))
    while unseen:
        start=unseen.pop();group=[start];stack=[start]
        while stack:
            for j in adj[stack.pop()]:
                if j in unseen:unseen.remove(j);group.append(j);stack.append(j)
        components.append(group)
    ends={'Hips':'Spine','Spine':'Spine1','Spine1':'Spine2','Spine2':'Neck','Neck':'Head','Head':'HeadTop_End'}
    for side in ['Left','Right']:
        ends.update({side+'Shoulder':side+'Arm',side+'Arm':side+'ForeArm',side+'ForeArm':side+'Hand',side+'Hand':None,side+'UpLeg':side+'Leg',side+'Leg':side+'Foot',side+'Foot':side+'ToeBase'})
    pos={b.name:obj.matrix_world.inverted()@arm.matrix_world@b.head_local for b in arm.data.bones}
    segments={key:(pos[key],pos[end] if end else pos[key]+Vector((-.015,0,-.08 if name!='OgreWarden' else -.11))) for key,end in ends.items()}
    kd=KDTree(n)
    for i,p in enumerate(points):kd.insert(p,i)
    kd.balance();seeds={key:set() for key in segments}
    for key,(a,b) in segments.items():
        for f in [.25,.5,.75]:seeds[key].add(kd.find(a.lerp(b,f))[1])
    # Give entirely disconnected accessories their nearest anatomical anchor.
    has_seed=set().union(*seeds.values())
    for group in components:
        if any(i in has_seed for i in group):continue
        center=sum((points[i] for i in group),Vector())/len(group)
        def distance(key):
            a,b=segments[key];v=b-a;t=max(0,min(1,(center-a).dot(v)/max(v.length_squared,1e-8)));return (center-(a+t*v)).length
        key=min(segments,key=distance)
        seeds[key].add(min(group,key=lambda i:(points[i]-center).length_squared))
    distances={}
    for key,starts in seeds.items():
        dist=[math.inf]*n;queue=[]
        for i in starts:dist[i]=.008;heapq.heappush(queue,(.008,i))
        while queue:
            d,i=heapq.heappop(queue)
            if d>dist[i]:continue
            for j,cost in adj[i].items():
                v=d+cost
                if v<dist[j]:dist[j]=v;heapq.heappush(queue,(v,j))
        distances[key]=dist
    for g in list(obj.vertex_groups):obj.vertex_groups.remove(g)
    for key in segments:obj.vertex_groups.new(name=key)
    for v in vertices:
        pairs=sorted([(key,d[v.index]) for key,d in distances.items() if math.isfinite(d[v.index])],key=lambda p:p[1])[:4]
        assert pairs,(name,'No surface seed',v.index)
        weights=[(key,1/(d+.025)**4) for key,d in pairs];total=sum(w for key,w in weights)
        for key,w in weights:
            if w/total>.001:obj.vertex_groups[key].add([v.index],w/total,'REPLACE')
        total=sum(g.weight for g in v.groups)
        for g in list(v.groups):obj.vertex_groups[g.group].add([v.index],g.weight/total,'REPLACE')
    return {'surface_components':len(components),'bones':len(segments)}
