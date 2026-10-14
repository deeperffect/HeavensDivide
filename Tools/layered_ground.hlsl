// LAYERED_GROUND: broad composition and local art use independent coordinates.
// Increasing LayoutSize changes the mask extent, never the artwork/rock scale.
float2 p = Position.xy;
float2 layoutUV = (p - LayoutOrigin.xy) / max(LayoutSize.xy, 1.0);
float3 weights = max(Texture2DSample(LayoutTexture, LayoutTextureSampler, layoutUV).rgb, 0.0);
weights /= max(dot(weights, 1.0), .001);
float4 blend = Texture2DSample(BlendTexture, BlendTextureSampler, layoutUV);
float2 grassUV = float2(p.x*.939693-p.y*.342020, p.x*.342020+p.y*.939693)/max(GrassTileSize,1.0);
float2 dirtUV = float2(p.x*.857167+p.y*.515038, -p.x*.515038+p.y*.857167)/max(DirtTileSize,1.0);
// Two fixed-scale samples vary the artwork between broad patches. No stretched
// overview texture, forced residency, negative mip bias, or screen-space detail.
float alternate = smoothstep(.25,.75,blend.b) * RepeatVariation;
float3 grass = lerp(Texture2DSample(GrassTexture,GrassTextureSampler,grassUV).rgb,
                   Texture2DSample(GrassTexture,GrassTextureSampler,float2(-grassUV.y,grassUV.x)+float2(.37,.63)).rgb,alternate);
float3 dirt = lerp(Texture2DSample(DirtTexture,DirtTextureSampler,dirtUV).rgb,
                  Texture2DSample(DirtTexture,DirtTextureSampler,float2(-dirtUV.y,dirtUV.x)+float2(.71,.29)).rgb,alternate);
float originalGrass = weights.x;
float transition = 4.0*originalGrass*(1.0-originalGrass);
float grassWeight = saturate(originalGrass+(blend.a-.5)*2.0*BlendEdgeVariation*transition);
// A narrow, broken fringe follows every alpha silhouette and stays in world cm.
grassWeight = max(grassWeight,blend.r*RockEdgeGrass);
float bladeHeight = saturate(dot(grass,float3(.2126,.7152,.0722))*5.0);
grassWeight = smoothstep(.5-BlendWidth,.5+BlendWidth,grassWeight+(bladeHeight-.5)*.10);
float3 ground = lerp(dirt*DirtTint.rgb,grass*GrassTint.rgb,grassWeight);
ground *= (1.0-blend.g*ContactShade)*(1.0+(blend.b-.5)*GroundColorVariation);
return max(0.0,lerp(dot(ground,float3(.2126,.7152,.0722)).xxx,ground,Saturation)*Brightness);
