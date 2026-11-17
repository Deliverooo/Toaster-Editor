#version 460
#define __FRAG__

#extension GL_GOOGLE_include_directive: require
#include "common.glsl"

layout (early_fragment_tests) in;

layout (push_constant) uniform PushData
{
    OITStorage oitStorage;
    float2 screenResolution;
    uint32 colourAttachmentHeapId;
    uint32 samplerHeapId;
} pcs;

layout (location = 0u) out float4 o_Colour;

void main()
{
    uint32 x = uint32(gl_FragCoord.x);
    uint32 y = uint32(gl_FragCoord.y);

    if (x < pcs.screenResolution.x && y < pcs.screenResolution.y)
    {
        uint32 pixel_index = x + y * uint32(pcs.screenResolution.x);
        uint32 count = pcs.oitStorage.pixels[pixel_index].count;

        OITFragment local_fragments[OEX_OIT_LAYER_COUNT];

        uint32 iterations = min(count, uint32(OEX_OIT_LAYER_COUNT));
        for (uint32 i = 0u; i < iterations; ++i)
        {
            local_fragments[i] = pcs.oitStorage.pixels[pixel_index].fragments[i];
        }

        float3 transparent_accumulation = float3(0.0f);
        float transmittance = 1.0f;

        for (int32 i = int32(iterations) - 1; i >= 0; --i)
        {
            float4 layerColor = unpackUnorm4x8(local_fragments[i].colour);
            transparent_accumulation = (layerColor.rgb * layerColor.a) + (transparent_accumulation * (1.0f - layerColor.a));
            transmittance *= (1.0f - layerColor.a);
        }

        float4 scene_colour = SAMPLE_TEXTURE(pcs.colourAttachmentHeapId, pcs.samplerHeapId, gl_FragCoord.xy / pcs.screenResolution);
        o_Colour = float4(transparent_accumulation + (scene_colour.rgb * transmittance), 1.0f);
    }
    else
    {
        o_Colour = float4(1.0f);
    }
}