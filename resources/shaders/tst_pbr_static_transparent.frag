#version 460
#define __FRAG__

#extension GL_ARB_fragment_shader_interlock: require
#extension GL_GOOGLE_include_directive: require
#include "common.glsl"

layout (early_fragment_tests) in;
layout (pixel_interlock_ordered) in;

layout (location = 0u) in float3 v_Position;
layout (location = 1u) in float3 v_WorldPos;
layout (location = 2u) in float3 v_Normal;
layout (location = 3u) in float2 v_TexCoord;
layout (location = 4u) flat in uint32 v_Material;
layout (location = 5u) in float3x3 v_TBN;

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

    OITStorage oitStorage;
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

    uint32 x = uint32(gl_FragCoord.x);
    uint32 y = uint32(gl_FragCoord.y);

    beginInvocationInterlockARB();

    if (x < pcs.screenResolution.x && y < pcs.screenResolution.y)
    {
        uint32 pixel_index = x + y * uint32(pcs.screenResolution.x);

        float current_depth = gl_FragCoord.z;
        uint32 packed_colour = packUnorm4x8(final_colour);

        uint32 current_count = min(pcs.oitStorage.pixels[pixel_index].count, uint32(OEX_OIT_LAYER_COUNT));

        OITFragment local_fragments[OEX_OIT_LAYER_COUNT];
        for (uint32 i = 0u; i < current_count; ++i)
        {
            local_fragments[i].colour = pcs.oitStorage.pixels[pixel_index].fragments[i].colour;
            local_fragments[i].depth = pcs.oitStorage.pixels[pixel_index].fragments[i].depth;
        }

        bool modified = false;

        if (current_count < OEX_OIT_LAYER_COUNT)
        {
            uint32 insert_pos = current_count;

            while (insert_pos > 0u && local_fragments[insert_pos - 1u].depth > current_depth)
            {
                local_fragments[insert_pos] = local_fragments[insert_pos - 1u];
                insert_pos--;
            }
            local_fragments[insert_pos].colour = packed_colour;
            local_fragments[insert_pos].depth = current_depth;

            current_count++;
            pcs.oitStorage.pixels[pixel_index].count = current_count;
            modified = true;
        }
        else if (current_depth < local_fragments[OEX_OIT_LAYER_COUNT - 1u].depth)
        {
            uint32 insert_pos = OEX_OIT_LAYER_COUNT - 1u;
            while (insert_pos > 0u && local_fragments[insert_pos - 1u].depth > current_depth)
            {
                local_fragments[insert_pos] = local_fragments[insert_pos - 1u];
                insert_pos--;
            }
            local_fragments[insert_pos].colour = packed_colour;
            local_fragments[insert_pos].depth = current_depth;
            modified = true;
        }

        if (modified)
        {
            for (uint32 i = 0u; i < current_count; ++i)
            {
                pcs.oitStorage.pixels[pixel_index].fragments[i].colour = local_fragments[i].colour;
                pcs.oitStorage.pixels[pixel_index].fragments[i].depth = local_fragments[i].depth;
            }
        }

    }
    endInvocationInterlockARB();
}