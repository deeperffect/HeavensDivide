// World-space grass/soil variation; stone retains its authored scale and UVs.
struct GroundBlend
{
    float hash(float2 p)
    {
        return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
    }
    float noise(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(hash(i), hash(i + float2(1, 0)), f.x),
                    lerp(hash(i + float2(0, 1)), hash(i + 1), f.x), f.y);
    }
    float2 rotate(float2 v, float2 cs)
    {
        return float2(cs.x*v.x - cs.y*v.y, cs.y*v.x + cs.x*v.y);
    }
    float3 samplePatch(Texture2D tex, SamplerState samp, float2 uv,
                       float2 dx, float2 dy, float2 id, float amount)
    {
        float angle = hash(id + 7.3) * 6.283185307 * amount;
        float2 cs = float2(cos(angle), sin(angle));
        float2 offset = float2(hash(id + 19.7), hash(id + 83.1)) * 7.0 * amount;
        // Cell IDs change abruptly, but each sample keeps continuous UVs and
        // explicit derivatives. Mip selection cannot spike at cell boundaries.
        return Texture2DSampleGrad(tex, samp, rotate(uv, cs) + offset,
                                  rotate(dx, cs), rotate(dy, cs)).rgb;
    }
    float3 varied(Texture2D tex, SamplerState samp, float2 uv, float seed, float amount)
    {
        float2 dx = ddx(uv), dy = ddy(uv);
        // Equilateral triangular grid: only three overlapping samples per layer.
        float2 grid = float2(uv.x - uv.y * .577350269, uv.y * 1.154700538) * 1.1;
        float2 cell = floor(grid), f = frac(grid);
        float2 a, b, c;
        float3 w;
        if (f.x + f.y <= 1.0)
        {
            a = cell; b = cell + float2(1, 0); c = cell + float2(0, 1);
            w = float3(1.0 - f.x - f.y, f.x, f.y);
        }
        else
        {
            a = cell + 1; b = cell + float2(0, 1); c = cell + float2(1, 0);
            w = float3(f.x + f.y - 1.0, 1.0 - f.x, 1.0 - f.y);
        }
        // Favor one patch away from seams, preserving painted contrast.
        w = max(w, 0.0); w *= w; w *= w;
        w /= max(dot(w, 1.0), .00001);
        return samplePatch(tex, samp, uv, dx, dy, a + seed, amount) * w.x
             + samplePatch(tex, samp, uv, dx, dy, b + seed, amount) * w.y
             + samplePatch(tex, samp, uv, dx, dy, c + seed, amount) * w.z;
    }
};
GroundBlend n;
float2 p = Position.xy;
float2 macro = p / max(PatchSize, 1.0);
float grain = n.noise(macro * 9.0 + 19.0) - .5;
float g = n.noise(macro) * .72 + n.noise(macro * 2.7 + 3.4) * .28;
float s = n.noise(macro * 1.31 + 73.0);
// Small world-space contour variation lets soft edges interleave organically.
// Coverage still uses the existing layout; texture scale is independent of it.
float breakup = (n.noise(p / 260.0 + 31.0) * .65
               + n.noise(p / 90.0 + 71.0) * .35 - .5) * BlendBreakup;
float edge = max(BlendSoftness, .015);
float grassMask = smoothstep(1.0 - GrassCoverage - edge, 1.0 - GrassCoverage + edge,
                            g + grain * .09 + breakup);
float stoneMask = smoothstep(1.0 - StoneCoverage - edge, 1.0 - StoneCoverage + edge,
                            s + grain * .05 + breakup * .6);
grassMask *= step(.001, GrassCoverage);
stoneMask *= step(.001, StoneCoverage);
grassMask = lerp(grassMask, 1.0, step(.999, GrassCoverage));
stoneMask = lerp(stoneMask, 1.0, step(.999, StoneCoverage));
float3 automaticWeights = float3(grassMask * (1-stoneMask),
                                 (1-grassMask) * (1-stoneMask), stoneMask);
float3 weights = lerp(automaticWeights, PaintWeights / max(dot(PaintWeights, 1.0), .001),
                      saturate(UsePaint));
float3 grass = n.varied(GrassTexture, GrassTextureSampler,
                       p / max(GrassTileSize, 1.0), 37.0, saturate(TextureVariation)) * GrassTint.rgb;
float3 dirt = n.varied(DirtTexture, DirtTextureSampler,
                      p / max(DirtTileSize, 1.0), 91.0, saturate(TextureVariation)) * DirtTint.rgb;
float3 stone = Texture2DSample(StoneTexture, StoneTextureSampler,
                              p / max(StoneTileSize, 1.0)).rgb * StoneTint.rgb;
float3 rgb = grass * weights.x + dirt * weights.y + stone * weights.z;
float variation = 1.0 + (n.noise(macro * .47 + 12.0) - .5) * MacroVariation;
float grey = dot(rgb, float3(.2126, .7152, .0722));
return max(0.0, lerp(grey.xxx, rgb, Saturation) * Brightness * variation);
