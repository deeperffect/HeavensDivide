"""Shared centimetre coordinates for the decorative countryside beyond the arena."""
import math
R=6500*math.sqrt(2)
def height(e,n):
    d=max(abs(e)-R,abs(n)-R,0)
    if d<=0:return -300.
    shore=e+n+9000+1300*math.sin(n/4600)
    coastal=max(0,min(1,shore/4000))
    hills=sum(h*math.exp(-((e-x)**2+(n-y)**2)/(2*w*w)) for x,y,h,w in
              [(-7000,23000,4800,6500),(23000,14000,5300,7000),(-21000,9000,3400,5000),(14000,-22000,3500,6000),(33000,32000,6000,8000)])
    noise=180*math.sin(e/1700)*math.cos(n/2100)+100*math.sin((e+n)/850)
    rise=min(d/2500,1)
    return -350+coastal*(350+rise*(350+hills+noise))
def outside_arena(e,n,margin=650):
    return abs(e)>R+margin or abs(n)>R+margin
