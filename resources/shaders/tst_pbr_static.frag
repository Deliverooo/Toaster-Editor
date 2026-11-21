#version 460
#define __FRAG__

#extension GL_GOOGLE_include_directive: require
#include "common.glsl"

layout (location = 0u) in float3 v_Position;
layout (location = 1u) in float3 v_WorldPos;
layout (location = 2u) in float3 v_Normal;
layout (location = 3u) in float2 v_TexCoord;
layout (location = 4u) flat in uint32 v_Material;
layout (location = 5u) in float3x3 v_TBN;

layout (location = 0u) out float4 o_Colour;

layout (push_constant) uniform PushData
{
    CameraBuffer camera;
    ObjectDataBuffer objectBuffer;
    MaterialBuffer materialBuffer;
    TransformDataBuffer transformBuffer;
    MeshMetadataBuffer meshMetadataBuffer;
    MaterialIndirectionBuffer materialIndirectionBuffer;
    PointLightBuffer pointLightBuffer;
    uintptr indirectBuffer;

    uintptr oitStorage;
    float2 screenResolution;

    uint32 diffuseIrradianceMapHeapSlot;
    uint32 samplerId;

    uint32 isTransparentPass;
    uint32 activePointLightCount;
} pcs;


float3 calcPointLights()
{
    float3 result = float3(0.0f);

    for (uint32 i = 0u; i < pcs.activePointLightCount; ++i)
    {
        PointLight light = pcs.pointLightBuffer.pointLights[i];

        float3 l = normalize(light.position.xyz - v_WorldPos);
        float3 h = normalize(glob.view + l);

        float distance = length(light.position.xyz - v_WorldPos);
        float attenuation = 1.0f / (distance * distance);

        float3 radiance = light.colour.rgb * attenuation;

        float ndf = distributionGGX(glob.normal, h, glob.roughness);

        float nDotL = max(dot(glob.normal, l), 0.0f);
        float g = geometrySmith(glob.nDotV, nDotL, glob.roughness);
        float3 f = fresnelSchlick(glob.f0, max(dot(h, glob.view), 0.0f));

        float3 ks = f;
        float3 kd = float3(1.0f) - ks;
        kd *= 1.0f - glob.metalness;

        float3 numerator = ndf * g * f;
        float denominator = 4.0f * glob.nDotV * nDotL + 0.0001f;
        float3 specular = numerator / denominator;

        result += (kd * glob.albedo.rgb / PI + specular) * radiance * nDotL;
    }

    return result;
}

void main()
{
    glob.normal = normalize(v_Normal);
    Material material = pcs.materialBuffer.materials[v_Material];

    if (!bool(material.flags & TST_MATERIAL_FLAGS_NO_NORMAL_MAP_BIT))
    {
        float3 normal = SAMPLE_TEXTURE(material.normalMapHeapSlot, pcs.samplerId, v_TexCoord).xyz;
        normal = normalize(normal * 2.0f - 1.0f);
        glob.normal = normalize(v_TBN * normal);
    }

    glob.view = normalize(pcs.camera.position.xyz - v_WorldPos); // Get the direction of the view from the camera to the frag pos
    float3 reflection_vec = reflect(-glob.view, glob.normal);
    glob.nDotV = max(dot(glob.normal, glob.view), 0.0001f); // Tells us how much the view direction is aligned with the surface normal

    glob.albedo = float4(1.0f);
    glob.albedo *= SAMPLE_TEXTURE(material.albedoMapHeapSlot, pcs.samplerId, v_TexCoord);
    glob.albedo *= material.albedoColour;

    glob.roughness = material.roughness;
    if (!bool(material.flags & TST_MATERIAL_FLAGS_NO_ROUGHNESS_MAP_BIT))
    {
        float roughness = SAMPLE_TEXTURE(material.roughnessMapHeapSlot, pcs.samplerId, v_TexCoord).g;
        glob.roughness = roughness;
    }

    glob.metalness = material.metalness;
    if (!bool(material.flags & TST_MATERIAL_FLAGS_NO_METALNESS_MAP_BIT))
    {
        float metalness = SAMPLE_TEXTURE(material.metalnessMapHeapSlot, pcs.samplerId, v_TexCoord).b;
        glob.metalness = metalness;
    }

    glob.f0 = float3(0.04f);
    glob.f0 = mix(glob.f0, glob.albedo.rgb, glob.metalness);

    float3 ks = fresnelSchlickRoughness(glob.nDotV, glob.f0, glob.roughness);
    float3 kd = float3(1.0f) - ks;
    kd *= 1.0f - glob.metalness;

    float3 diffuse_irradiance = SAMPLE_CUBE(pcs.diffuseIrradianceMapHeapSlot, pcs.samplerId, glob.normal, 0u).rgb;
    diffuse_irradiance *= 0.3f;

    float3 diffuse_ambient = kd * diffuse_irradiance * glob.albedo.rgb;

    float3 ambient = diffuse_ambient;

    float3 lo = float3(0.0f);
    lo += calcPointLights();

    float4 final_colour = float4(ambient + lo, 1.0f);
    final_colour.a *= glob.albedo.a;

    //    final_colour = float4(float3(glob.metalness), 1.0f);

    o_Colour = glob.albedo;
    //    o_Colour = float4(1.0f);
}