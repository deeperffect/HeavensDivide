"""Deterministic, original 48 kHz mono combat SFX; no external samples."""
import math,random,wave,array,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'SourceAudio/Combat';OUT.mkdir(parents=True,exist_ok=True)
RATE=48000
# event, recipe, length, tonal center, mix level, semitone spread, concurrency family
ROWS=[
('SamuraiSwing','slice',.28,520,.65,1.6,'Weapon'),('SamuraiImpact','impact',.22,145,.65,2.,'Impact'),
('DoubleCut','slice',.48,360,.8,1.2,'Ability'),('BladeWave','wave',.65,220,.75,1.3,'Ability'),('BladeWaveHit','impact',.28,100,.65,1.5,'Impact'),
('SplinterWave','metal',.22,1400,.45,2.,'Proc'),('CrossingBlades','slice',.35,760,.55,1.5,'Proc'),('ReturningBlade','slice',.4,420,.55,1.5,'Proc'),
('OverkillBurst','burst',.7,64,.9,1.5,'Ability'),('BleedingEdge','blood',.22,180,.35,2.,'Proc'),('BloodTransfer','blood',.42,270,.55,1.5,'Proc'),
('SamuraiAbility','wave',.8,130,.9,1.,'Ability'),('SamuraiPulse','wave',.25,210,.5,1.3,'Pulse'),
('KunaiThrow','slice',.14,1700,.5,2.,'Weapon'),('KunaiImpact','metal',.16,900,.5,2.,'Impact'),('ShurikenThrow','metal',.42,470,.55,1.3,'Weapon'),('ShurikenImpact','metal',.22,640,.5,1.8,'Impact'),
('NinjaAbility','slice',.62,1100,.75,1.,'Ability'),('NinjaPulse','slice',.12,1800,.3,1.5,'Pulse'),('Poison','poison',.38,450,.35,2.,'Proc'),('EmbeddedScatter','metal',.32,1300,.45,2.,'Proc'),('ShadowClone','shadow',.4,320,.5,1.5,'Ability'),
('GrandEntrance','burst',.8,92,.9,1.,'Ability'),('TagTeam','slice',.3,650,.5,1.5,'Proc'),
('EnemySwing','slice',.24,240,.32,2.,'Enemy'),('OgreWindup','warning',.4,170,.5,.7,'Warning'),('OgreSlam','burst',.65,48,.85,1.2,'Enemy'),
('BombFuse','warning',.45,940,.5,.4,'Warning'),('BombExplosion','burst',.9,58,.85,1.,'Enemy'),('FireballCast','wave',.38,390,.45,1.5,'Enemy'),('FireballImpact','burst',.36,160,.5,1.8,'Impact'),
('DeathSmall','death',.36,180,.45,2.2,'Death'),('DeathCreature','death',.48,105,.55,1.8,'Death'),('DeathSkeleton','bone',.44,520,.45,2.,'Death'),('DeathElite','death',.85,55,.8,1.2,'Death'),('DeathBoss','death',1.4,42,1.,.6,'Major'),
('BossWarning','warning',.55,240,.65,.5,'Warning'),('BossCleave','slice',.6,170,.85,1.,'Major'),('BossAOE','burst',1.,60,.9,1.,'Major'),('BossDash','wave',.65,180,.75,1.,'Major'),('BossPhase','shadow',1.1,85,.85,.5,'Major'),('BossGroundHit','burst',.5,110,.7,1.3,'Enemy'),
('TrialStrike','wave',.6,190,.75,1.,'Enemy'),('TrialSuccess','chime',1.1,587,.6,.25,'UI'),('TrapHit','impact',.3,125,.5,1.5,'Impact'),
('Dash','slice',.22,430,.45,1.,'Movement'),('SwapArrival','shadow',.55,510,.65,.8,'Movement'),('SwapDeparture','shadow',.3,330,.45,.8,'Movement'),
('PlayerHit','impact',.24,78,.7,1.,'Player'),('PlayerDeath','shadow',1.3,100,.85,.5,'UI'),('LevelUp','chime',.85,659,.6,.2,'UI'),('UpgradeSelect','chime',.4,880,.45,.35,'UI'),
('XPPickup','chime',.12,1175,.18,2.,'Pickup'),('HealPickup','chime',.55,740,.5,.6,'Pickup'),('ChestOpen','chest',1.6,523,.8,.4,'UI'),('ObjectiveSpawn','shadow',.65,260,.5,.7,'Movement')]

def synth(name,kind,duration,freq):
 rng=random.Random(int(hashlib.sha256(name.encode()).hexdigest()[:8],16));n=int(duration*RATE);y=[0.]*n
 low=0.;mid=0.;phase=0.;prev=0.
 for i in range(n):
  t=i/RATE;q=t/duration;noise=rng.uniform(-1,1);low+=.025*(noise-low);mid+=.24*(noise-mid);band=mid-low
  attack=min(1,t/.004);tail=min(1,(duration-t)/.018)
  if kind in ('slice','wave','shadow'):
   env=math.sin(math.pi*q)**(1.3 if kind=='wave' else 2.1)
   sweep=freq*(2.1-1.6*q);phase+=2*math.pi*sweep/RATE
   tonal=math.sin(phase+1.2*math.sin(phase*.501))
   x=env*(band*1.8+low*2.5+tonal*(.11 if kind=='slice' else .25))
   if kind=='shadow':x*=.65+.35*math.sin(2*math.pi*(7*t+9*t*t))
  elif kind in ('impact','burst','death','blood','bone'):
   phase+=2*math.pi*(freq*(.45+1.8*math.exp(-t*18)))/RATE
   body=math.sin(phase)*math.exp(-t/(duration*.25))
   grain=band*math.exp(-t/(duration*.22))
   x=.6*body+grain*(1.7 if kind=='impact' else 1.1)+low*3*math.exp(-t/(duration*.5))
   if kind=='burst':x+=band*.6*math.exp(-t/(duration*.6))*(.7+.3*math.sin(t*71))
   if kind=='death':x+=.2*math.sin(phase*2.37+2*low)*math.exp(-t/(duration*.35))*(.5+.5*math.sin(t*56))
   if kind=='blood':x=.35*body+band*(.5+.5*math.sin(t*117+math.sin(t*31)))*math.exp(-q*5)
   if kind=='bone':x=.2*body+sum(.12*math.sin(2*math.pi*freq*r*t)*math.exp(-q*(9+r)) for r in (1,1.43,2.19,3.71))+grain
  elif kind=='metal':
   x=sum(.16*math.sin(2*math.pi*freq*r*t+.8*math.sin(t*43))*math.exp(-q*(6+r)) for r in (1,1.41,2.17,3.63))
   x+=band*.8*math.sin(math.pi*q)**1.2
  elif kind=='poison':
   phase+=2*math.pi*freq*(.5+.5*math.sin(t*21)**2)/RATE
   x=(.2*math.sin(phase)*(math.sin(t*47)**8)+band*.7+low)*math.sin(math.pi*q)**1.5
  elif kind=='warning':
   phase+=2*math.pi*freq*(.8+.65*q)/RATE
   x=(.17*math.sin(phase)+.06*math.sin(phase*2)+band*.14)*math.sin(math.pi*q)**.7*(.65+.35*math.sin(2*math.pi*(3*t+6*t*t)))
  else:
   x=0.
   for k,r in enumerate((1,1.25,1.5,2)):
    z=t-k*min(.09,duration*.14)
    if z>=0:x+=.19*math.sin(2*math.pi*freq*r*z)*math.exp(-z/max(.03,duration*.24))*min(1,z/.003)
   if kind=='chest':x+=low*2.3*math.exp(-t*10)+band*.5*math.exp(-abs(t-.12)*35)
  # Smooth attack/release plus DC blocker.
  x*=attack*tail;v=x-prev+.995*(y[i-1] if i else 0);prev=x;y[i]=v
 peak=max(abs(v) for v in y);rms=math.sqrt(sum(v*v for v in y)/n)
 gain=min(.65/max(peak,1e-9),.105/max(rms,1e-9))
 return [v*gain for v in y]

manifest=[]
for name,kind,duration,freq,volume,pitch,group in ROWS:
 values=synth(name,kind,duration,freq);pcm=array.array('h',(round(v*32767) for v in values));file=OUT/(name+'.wav')
 with wave.open(str(file),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(pcm.tobytes())
 manifest.append(dict(event=name,recipe=kind,duration=duration,volume=volume,pitch_min=-pitch,pitch_max=pitch,group=group,wav=str(file.relative_to(ROOT)).replace('\\','/'),peak=max(map(abs,values)),rms=math.sqrt(sum(v*v for v in values)/len(values))))
(ROOT/'Tools/combat_audio_manifest.json').write_text(json.dumps(manifest,indent=2))
print('Generated',len(manifest),'original WAV files; peak',max(a['peak'] for a in manifest))
