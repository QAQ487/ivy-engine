#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShaderCommon.hlsli"

cbuffer ShadowBuffer : register(b2)
{
    matrix LightViewProjection[MAX_LIGHTS]; // per-cascade World*LightVP
    float4 ShadowParams[MAX_LIGHTS]; // x=constBias, y=slopeBias, z=receiverNormalOffset(OS)
    uint CascadeCount;
    float CascadeBlend;
    float2 _padCascade;
    float4 CascadeSplits; // view-space far Z for cascades 0..3
    matrix WorldView;     // object -> view (for cascade selection)
}

struct LightParam
{
    float3 DirOS;
    float Type; // 0: Directional, 1: Spot, 2: Point
    float3 Color;
    float _pad1;
    float3 SpecColor;
    float SpecPower;
    float3 PosOS;
    float Range;
    float3 Atten; // (kc, kl, kq)
    float InnerCos;
    float OuterCos;
    float3 _pad2;
};

cbuffer LightingBuffer : register(b3)
{
    LightParam Lights[MAX_LIGHTS];
    float3 AmbientColor;
    float _padA;
    float3 ViewDirOS;
    float LightCountF;
    float3 CameraPosWS;
    float FogEnabled;
    float3 FogColor;
    float FogDensity;
    float FogStart;
    float FogHeight;
    float FogHeightFalloff;
    float _padFog;
    matrix World; // object -> world (CPU stores transposed)
}

Texture2D g_Texture : register(t0);
Texture2D<float> g_ShadowMap[MAX_LIGHTS] : register(t1);
Texture2D g_NormalMap : register(t5);
Texture2D g_RampTexture : register(t6);
SamplerState g_SamplerState : register(s0);
SamplerComparisonState g_ShadowSampler : register(s1);

struct BaseColorResult
{
    float3 rgb;
    float alpha;
};

BaseColorResult SampleBaseColor(float4 inDiffuse, float2 inTexCoord)
{
    BaseColorResult r;
    float hasTex = saturate(Diffuse.a);
    float4 texCol = g_Texture.Sample(g_SamplerState, inTexCoord);
    r.rgb = inDiffuse.rgb * Diffuse.rgb * lerp(float3(1, 1, 1), texCol.rgb, hasTex);
    r.alpha = inDiffuse.a * lerp(1.0, texCol.a, hasTex);
    return r;
}

// Cotangent-frame normal mapping (no mesh tangents required).
float3x3 CotangentFrame(float3 N, float3 p, float2 uv)
{
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, N);
}

float3 ApplyNormalMap(float3 N, float3 posOS, float2 uv)
{
    N = normalize(N);
    if (NormalStrength <= 0.0f)
        return N;

    float3 nTex = g_NormalMap.Sample(g_SamplerState, uv).xyz * 2.0f - 1.0f;
    nTex.xy *= NormalStrength;
    nTex = normalize(nTex);

    float3x3 TBN = CotangentFrame(N, posOS, uv);
    return normalize(mul(nTex, TBN));
}

float PCF3x3(Texture2D<float> sm, SamplerComparisonState cmpSamp, float2 uv, float cmp, float2 texel)
{
    float sum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 off = float2(x, y) * texel;
            sum += sm.SampleCmpLevelZero(cmpSamp, uv + off, cmp);
        }
    }
    return sum / 9.0f;
}

float SampleCascadeShadow0(float3 posForShadowOS)
{
    float4 sp = mul(float4(posForShadowOS, 1.0f), LightViewProjection[0]);
    float3 proj = sp.xyz / max(sp.w, 1e-6f);
    float slope = max(abs(ddx(proj.z)), abs(ddy(proj.z)));
    float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f)) return 1.0f;
    float bias = ShadowParams[0].x + slope * ShadowParams[0].y;
    uint smw = 0, smh = 0;
    g_ShadowMap[0].GetDimensions(smw, smh);
    float2 texel = 1.0f / float2(max(1.0f, (float)smw), max(1.0f, (float)smh));
    return lerp(0.3f, 1.0f, PCF3x3(g_ShadowMap[0], g_ShadowSampler, uv, proj.z - bias, texel));
}
float SampleCascadeShadow1(float3 posForShadowOS)
{
    float4 sp = mul(float4(posForShadowOS, 1.0f), LightViewProjection[1]);
    float3 proj = sp.xyz / max(sp.w, 1e-6f);
    float slope = max(abs(ddx(proj.z)), abs(ddy(proj.z)));
    float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f)) return 1.0f;
    float bias = ShadowParams[1].x + slope * ShadowParams[1].y;
    uint smw = 0, smh = 0;
    g_ShadowMap[1].GetDimensions(smw, smh);
    float2 texel = 1.0f / float2(max(1.0f, (float)smw), max(1.0f, (float)smh));
    return lerp(0.3f, 1.0f, PCF3x3(g_ShadowMap[1], g_ShadowSampler, uv, proj.z - bias, texel));
}
float SampleCascadeShadow2(float3 posForShadowOS)
{
    float4 sp = mul(float4(posForShadowOS, 1.0f), LightViewProjection[2]);
    float3 proj = sp.xyz / max(sp.w, 1e-6f);
    float slope = max(abs(ddx(proj.z)), abs(ddy(proj.z)));
    float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f)) return 1.0f;
    float bias = ShadowParams[2].x + slope * ShadowParams[2].y;
    uint smw = 0, smh = 0;
    g_ShadowMap[2].GetDimensions(smw, smh);
    float2 texel = 1.0f / float2(max(1.0f, (float)smw), max(1.0f, (float)smh));
    return lerp(0.3f, 1.0f, PCF3x3(g_ShadowMap[2], g_ShadowSampler, uv, proj.z - bias, texel));
}
float SampleCascadeShadow3(float3 posForShadowOS)
{
    float4 sp = mul(float4(posForShadowOS, 1.0f), LightViewProjection[3]);
    float3 proj = sp.xyz / max(sp.w, 1e-6f);
    float slope = max(abs(ddx(proj.z)), abs(ddy(proj.z)));
    float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f)) return 1.0f;
    float bias = ShadowParams[3].x + slope * ShadowParams[3].y;
    uint smw = 0, smh = 0;
    g_ShadowMap[3].GetDimensions(smw, smh);
    float2 texel = 1.0f / float2(max(1.0f, (float)smw), max(1.0f, (float)smh));
    return lerp(0.3f, 1.0f, PCF3x3(g_ShadowMap[3], g_ShadowSampler, uv, proj.z - bias, texel));
}

float SampleCascadeShadowByIndex(uint cascadeIndex, float3 posForShadowOS)
{
    float result = 1.0f;
    if (cascadeIndex == 0) result = SampleCascadeShadow0(posForShadowOS);
    else if (cascadeIndex == 1) result = SampleCascadeShadow1(posForShadowOS);
    else if (cascadeIndex == 2) result = SampleCascadeShadow2(posForShadowOS);
    else if (cascadeIndex == 3) result = SampleCascadeShadow3(posForShadowOS);
    return result;
}

float CascadeSplitFar(uint cascadeIndex)
{
    if (cascadeIndex == 0) return CascadeSplits.x;
    if (cascadeIndex == 1) return CascadeSplits.y;
    if (cascadeIndex == 2) return CascadeSplits.z;
    return CascadeSplits.w;
}

// Cascaded directional shadow. Only directional lights (Type < 0.5) sample CSM.
float EvalShadow(uint lightIndex, float3 inPosOS, float3 N)
{
    if (CascadeCount == 0)
        return 1.0f;
    uint lightLoop = (uint)LightCountF;
    if (lightLoop > MAX_LIGHTS) lightLoop = MAX_LIGHTS;
    if (lightIndex >= lightLoop)
        return 1.0f;
    if (Lights[lightIndex].Type >= 0.5f)
        return 1.0f;

    float receiverNormalOffset = ShadowParams[0].z;
    float3 posForShadowOS = inPosOS + N * receiverNormalOffset;

    float viewZ = abs(mul(float4(inPosOS, 1.0f), WorldView).z);

    uint cascade = 0;
    if (viewZ >= CascadeSplits.x) cascade = 1;
    if (viewZ >= CascadeSplits.y) cascade = 2;
    if (viewZ >= CascadeSplits.z) cascade = 3;
    if (cascade >= CascadeCount)
        cascade = CascadeCount - 1;

    float shadow0 = 1.0f;
    if (cascade == 0) shadow0 = SampleCascadeShadow0(posForShadowOS);
    else if (cascade == 1) shadow0 = SampleCascadeShadow1(posForShadowOS);
    else if (cascade == 2) shadow0 = SampleCascadeShadow2(posForShadowOS);
    else shadow0 = SampleCascadeShadow3(posForShadowOS);

    if (cascade + 1 < CascadeCount)
    {
        float splitFar = CascadeSplitFar(cascade);
        float blendWidth = max(CascadeBlend, 1e-3f);
        float t = saturate((splitFar - viewZ) / blendWidth);
        if (t < 1.0f)
        {
            float shadow1 = 1.0f;
            uint nextC = cascade + 1;
            if (nextC == 0) shadow1 = SampleCascadeShadow0(posForShadowOS);
            else if (nextC == 1) shadow1 = SampleCascadeShadow1(posForShadowOS);
            else if (nextC == 2) shadow1 = SampleCascadeShadow2(posForShadowOS);
            else shadow1 = SampleCascadeShadow3(posForShadowOS);
            shadow0 = lerp(shadow1, shadow0, t);
        }
    }

    return shadow0;
}

float3 EvalHemisphereAmbient(float3 N, float3 baseRGB)
{
    float up01 = saturate(N.y * 0.5f + 0.5f);
    float3 skyCol = AmbientColor.rgb;
    float3 groundCol = Ambient.rgb;
    float groundBounceStrength = saturate(Ambient.a);

    float3 hemi = lerp(groundCol, skyCol, up01);
    float3 ambient = hemi * baseRGB;

    float bounce = saturate(-N.y);
    ambient += groundCol * baseRGB * groundBounceStrength * bounce;
    return ambient;
}

float3 EvalDirectionalLight(LightParam light, float3 N, float3 V, float3 baseRGB, bool withSpecular)
{
    float3 L = normalize(light.DirOS);
    float NdotL = saturate(dot(N, L));
    float3 diffuse = light.Color.rgb * baseRGB * NdotL;

    if (!withSpecular)
        return diffuse;

    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float specPow = max(1.0f, Shininess);
    float spec = pow(NdotH, specPow);
    float3 specular = light.SpecColor.rgb * Specular.rgb * spec;
    return diffuse + specular;
}

float3 EvalPointLight(LightParam light, float3 N, float3 V, float3 inPosOS, float3 baseRGB, bool withSpecular)
{
    float3 toLight = light.PosOS - inPosOS;
    float dist = length(toLight);
    float3 L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

    float kc = light.Atten.x;
    float kl = light.Atten.y;
    float kq = light.Atten.z;
    float atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);

    if (light.Range > 0.0f)
        atten *= saturate(1.0f - dist / light.Range);

    float NdotL = saturate(dot(N, L));
    float3 diffuse = light.Color.rgb * baseRGB * NdotL;

    float3 lit = diffuse;
    if (withSpecular)
    {
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        float specPow = max(1.0f, Shininess);
        float spec = pow(NdotH, specPow);
        lit += light.SpecColor.rgb * Specular.rgb * spec;
    }
    return lit * atten;
}

float3 EvalSpotLight(LightParam light, float3 N, float3 V, float3 inPosOS, float3 baseRGB, bool withSpecular)
{
    float3 toLight = light.PosOS - inPosOS;
    float dist = length(toLight);
    float3 L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

    float3 coneDir = normalize(light.DirOS);
    float cosTheta = dot(-L, coneDir);
    float innerC = light.InnerCos;
    float outerC = light.OuterCos;
    float t = saturate((cosTheta - outerC) / max(innerC - outerC, 1e-4f));

    float kc = light.Atten.x;
    float kl = light.Atten.y;
    float kq = light.Atten.z;
    float atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);
    float spotAtten = atten * t;

    float NdotL = saturate(dot(N, L));
    float3 diffuse = light.Color.rgb * baseRGB * NdotL;

    float3 lit = diffuse;
    if (withSpecular)
    {
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        float specPow = max(1.0f, Shininess);
        float spec = pow(NdotH, specPow);
        lit += light.SpecColor.rgb * Specular.rgb * spec;
    }
    return lit * spotAtten;
}

uint GetLightLoopCount()
{
    uint LCount = (uint) LightCountF;
    return (LCount > MAX_LIGHTS) ? MAX_LIGHTS : LCount;
}

float QuantizeToon(float x, float steps, float smoothness)
{
    float s = max(1.0f, steps);
    float scaled = saturate(x) * s;
    float band = floor(scaled);
    float frac = scaled - band;
    float edge = max(1e-4f, smoothness);
    float stepped = band + smoothstep(0.5f - edge, 0.5f + edge, frac);
    return saturate(stepped / s);
}

// Diffuse-only Lambert intensity (0..1 * lightColor), no specular.
float3 EvalDirectionalDiffuse(LightParam light, float3 N)
{
    float3 L = normalize(light.DirOS);
    float NdotL = saturate(dot(N, L));
    return light.Color.rgb * NdotL;
}

float3 EvalPointDiffuse(LightParam light, float3 N, float3 inPosOS)
{
    float3 toLight = light.PosOS - inPosOS;
    float dist = length(toLight);
    float3 L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

    float kc = light.Atten.x;
    float kl = light.Atten.y;
    float kq = light.Atten.z;
    float atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);
    if (light.Range > 0.0f)
        atten *= saturate(1.0f - dist / light.Range);

    float NdotL = saturate(dot(N, L));
    return light.Color.rgb * NdotL * atten;
}

float3 EvalSpotDiffuse(LightParam light, float3 N, float3 inPosOS)
{
    float3 toLight = light.PosOS - inPosOS;
    float dist = length(toLight);
    float3 L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

    float3 coneDir = normalize(light.DirOS);
    float cosTheta = dot(-L, coneDir);
    float innerC = light.InnerCos;
    float outerC = light.OuterCos;
    float t = saturate((cosTheta - outerC) / max(innerC - outerC, 1e-4f));

    float kc = light.Atten.x;
    float kl = light.Atten.y;
    float kq = light.Atten.z;
    float atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);
    float spotAtten = atten * t;

    float NdotL = saturate(dot(N, L));
    return light.Color.rgb * NdotL * spotAtten;
}

float3 EvalCelSpecular(LightParam light, float3 N, float3 V, float type, float3 inPosOS)
{
    float3 L;
    float atten = 1.0f;

    if (type < 0.5f)
    {
        L = normalize(light.DirOS);
    }
    else
    {
        float3 toLight = light.PosOS - inPosOS;
        float dist = length(toLight);
        L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

        float kc = light.Atten.x;
        float kl = light.Atten.y;
        float kq = light.Atten.z;
        atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);
        if (light.Range > 0.0f)
            atten *= saturate(1.0f - dist / light.Range);

        if (type >= 1.5f)
        {
            // point — atten already set
        }
        else
        {
            float3 coneDir = normalize(light.DirOS);
            float cosTheta = dot(-L, coneDir);
            float t = saturate((cosTheta - light.OuterCos) / max(light.InnerCos - light.OuterCos, 1e-4f));
            atten *= t;
        }
    }

    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float specPow = max(1.0f, Shininess);
    float threshold = saturate(SpecularThreshold);
    float cel = step(threshold, pow(NdotH, specPow));
    return light.SpecColor.rgb * Specular.rgb * cel * atten;
}

float3 ApplyHeightFog(float3 color, float3 posOS)
{
    if (FogEnabled <= 0.0f)
        return color;

    float3 posWS = mul(float4(posOS, 1.0f), World).xyz;
    float dist = max(0.0f, length(posWS - CameraPosWS) - FogStart);
    float distTerm = 1.0f - exp(-FogDensity * dist);
    float heightTerm = saturate((FogHeight - posWS.y) * FogHeightFalloff);
    float fog = saturate(distTerm * heightTerm);
    return lerp(color, FogColor, fog);
}

// Fresnel rim weighted toward the side opposite the key light (anime backlit rim).
float EvalLightFacingRim(float3 N, float3 V, float3 posOS, float rimStrength, float rimPower)
{
    float fresnel = pow(1.0f - saturate(dot(N, V)), max(1.0f, rimPower));
    float lightAlign = 1.0f;

    uint loopCount = GetLightLoopCount();
    if (loopCount > 0)
    {
        // Prefer first directional as key light; else Lights[0].
        uint key = 0;
        for (uint i = 0; i < loopCount; ++i)
        {
            if (Lights[i].Type < 0.5f)
            {
                key = i;
                break;
            }
        }

        float3 L;
        if (Lights[key].Type < 0.5f)
        {
            L = normalize(Lights[key].DirOS);
        }
        else
        {
            float3 toLight = Lights[key].PosOS - posOS;
            float dist = length(toLight);
            L = (dist > 1e-6f) ? (toLight / dist) : float3(0.0f, 0.0f, 1.0f);
        }

        // High where the surface faces away from the key light.
        lightAlign = saturate(1.0f - saturate(dot(N, L)));
    }

    return fresnel * lightAlign * saturate(rimStrength);
}

// ---------------------------------------------------------------------------
// Cook-Torrance PBR (additive; unused by existing variants)
// G = Schlick-GGX / Smith with k = (roughness+1)^2 / 8 (direct lighting).
// ---------------------------------------------------------------------------

static const float kPBR_PI = 3.14159265f;

float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0f) + 1.0f;
    return a2 / (kPBR_PI * d * d + 1e-7f);
}

float G_SchlickSmith(float NoV, float NoL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    float gv = NoV / (NoV * (1.0f - k) + k);
    float gl = NoL / (NoL * (1.0f - k) + k);
    return gv * gl;
}

float3 F_Schlick(float VoH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - VoH), 5.0f);
}

float3 EvalCookTorrance(
    float3 N, float3 V, float3 L,
    float3 baseColor, float roughness, float metallic,
    float3 lightRGB, float atten)
{
    float r = max(roughness, 0.045f);
    float3 H = normalize(L + V);
    float NoV = max(dot(N, V), 1e-4f);
    float NoL = saturate(dot(N, L));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metallic);
    float a = r * r;
    float D = D_GGX(NoH, a);
    float G = G_SchlickSmith(NoV, NoL, r);
    float3 F = F_Schlick(VoH, F0);

    float3 spec = (D * G * F) / (4.0f * NoV * NoL + 1e-4f) * NoL;
    float3 diffuse = (baseColor / kPBR_PI) * (1.0f - F) * (1.0f - metallic) * NoL;
    return (diffuse + spec) * lightRGB * atten;
}

float3 EvalDirectionalPBR(LightParam light, float3 N, float3 V, float3 baseColor, float roughness, float metallic)
{
    float3 L = normalize(light.DirOS);
    return EvalCookTorrance(N, V, L, baseColor, roughness, metallic, light.Color.rgb, 1.0f);
}

float3 EvalPointPBR(LightParam light, float3 N, float3 V, float3 inPosOS, float3 baseColor, float roughness, float metallic)
{
    float3 toLight = light.PosOS - inPosOS;
    float dist = length(toLight);
    float3 L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

    float kc = light.Atten.x;
    float kl = light.Atten.y;
    float kq = light.Atten.z;
    float atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);
    if (light.Range > 0.0f)
        atten *= saturate(1.0f - dist / light.Range);

    return EvalCookTorrance(N, V, L, baseColor, roughness, metallic, light.Color.rgb, atten);
}

float3 EvalSpotPBR(LightParam light, float3 N, float3 V, float3 inPosOS, float3 baseColor, float roughness, float metallic)
{
    float3 toLight = light.PosOS - inPosOS;
    float dist = length(toLight);
    float3 L = (dist > 1e-6f) ? (toLight / dist) : float3(0, 0, 1);

    float3 coneDir = normalize(light.DirOS);
    float cosTheta = dot(-L, coneDir);
    float t = saturate((cosTheta - light.OuterCos) / max(light.InnerCos - light.OuterCos, 1e-4f));

    float kc = light.Atten.x;
    float kl = light.Atten.y;
    float kq = light.Atten.z;
    float atten = 1.0f / max(kc + kl * dist + kq * dist * dist, 1e-4f);
    float spotAtten = atten * t;

    return EvalCookTorrance(N, V, L, baseColor, roughness, metallic, light.Color.rgb, spotAtten);
}

#endif // LIGHTING_COMMON_HLSLI
