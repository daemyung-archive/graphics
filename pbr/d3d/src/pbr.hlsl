//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

struct IA2VS {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VS2RS {
    float4 position : SV_POSITION;
    float3 world_position : POSITION;
    float3 world_normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct RS2PS {
    float4 position : SV_POSITION;
    float3 world_position : POSITION;
    float3 world_normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Constant : register(b0) {
    float4x4 p;
    float4x4 v;
    float4x4 m;
    float3 light;
    float3 light_color;
    float3 camera;
}

Texture2D albedo_texture : register(t0);
Texture2D metallic_texture : register(t1);
Texture2D roughness_texture : register(t2);
Texture2D ao_texture : register(t3);

SamplerState clamp_sampler : register(s0);

VS2RS vs_main(IA2VS input) {
    VS2RS output;

    float4 position = float4(input.position, 1.0);

    position = mul(m, position);

    output.world_position = position.xyz;

    position = mul(v, position);
    position = mul(p, position);

    output.position = position;

    float4 normal = float4(input.normal, 0.0);

    normal = mul(m, normal);

    output.world_normal = normal.xyz;
    output.uv = input.uv;

    return output;
}

static const float M_PI_F = 3.14159265f;

float DistributionGGX(float3 n, float3 h, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0f);
    float ndoth2 = ndoth * ndoth;

    float numerator = a2;
    float denominator = ndoth2 * (a2 - 1.0) + 1.0;
    denominator = M_PI_F * denominator * denominator;

    return numerator / denominator;
}

float GeometrySchlickGGX(float3 n, float3 v, float roughness) {
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    float ndotv = max(dot(n, v), 0.0);

    float numerator = ndotv;
    float denominator = ndotv * (1.0 - k) + k;
    
    return numerator / denominator;
}

float GeometrySmith(float3 n, float3 v, float3 l, float roughness) {
    return GeometrySchlickGGX(n, v, roughness) * GeometrySchlickGGX(n, l, roughness);
}

float3 FresnelSchlick(float3 n, float3 v, float3 f0) {
    float ndotv = max(dot(n, v), 0.0);

    return f0 + (1.0 - f0) * pow(1.0 - ndotv, 5.0);
}

float4 ps_main(RS2PS input) : SV_Target{
    float3 albedo = albedo_texture.Sample(clamp_sampler, input.uv).rgb;
    float metallic = metallic_texture.Sample(clamp_sampler, input.uv).r;
    float roughness = roughness_texture.Sample(clamp_sampler, input.uv).r;
    float ao = ao_texture.Sample(clamp_sampler, input.uv).r;
    float3 radiance = light_color;

    float3 n = normalize(input.world_normal);
    float3 v = normalize(camera - input.world_position);
    float3 f0 = float3(0.04, 0.04, 0.04);
    f0 = lerp(f0, albedo, metallic);

    float3 lo = float3(0.0, 0.0, 0.0);

    float3 l = normalize(light);
    float3 h = normalize(v + l);

    float  d = DistributionGGX(n, h, roughness);
    float  g = GeometrySmith(n, v, l, roughness);
    float3 f = FresnelSchlick(h, v, f0);

    float3 numerator = d * g * f;
    float denominator = 4 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 ks = f;
    float3 kd = 1.0 - ks;
    kd *= 1.0 - metallic;

    float ndotl = max(dot(n, l), 0.0);

    lo += (kd * albedo / M_PI_F + specular) * radiance * ndotl;

    float3 ambient = float3(0.3, 0.3, 0.3) * albedo * ao;
    float3 color = ambient + lo;

    return float4(color, 1.0);
}