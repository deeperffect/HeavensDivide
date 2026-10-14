// CONNECTED_GROUND: joint-aware transitions between locally coherent patches.
// All inputs are native texture samples with explicit world-space derivatives.
float3 a = PaintA.rgb, b = PaintB.rgb, c = PaintC.rgb;
// Grey-olive faces have similar red/green channels; soil and grass separate them.
// Let intact stone faces win patch transitions instead of ghosting two pavements.
float3 chroma = float3(abs(a.r-a.g)/max(a.r+a.g,.001),
                       abs(b.r-b.g)/max(b.r+b.g,.001),
                       abs(c.r-c.g)/max(c.r+c.g,.001));
float3 face = 1.0-smoothstep(.05,.22,chroma);
float3 priority = Weights + face * SeamPreservation;
float highest = max(priority.x,max(priority.y,priority.z));
float3 blend = max(priority-highest+PatchBlendWidth,.0) * step(.001,Weights);
blend *= blend;
blend /= max(dot(blend,1.0),.00001);
float3 paint = a*blend.x+b*blend.y+c*blend.z;
float value = dot(paint,float3(.2126,.7152,.0722));
paint = lerp(value.xxx,paint,Saturation);
float2 p=Position.xy;
float macro=sin(p.x*.0007+sin(p.y*.0006))*cos(p.y*.00085+.4);
return max(0.,paint*GroundTint.rgb*Brightness*(1.+macro*ColorVariation));
