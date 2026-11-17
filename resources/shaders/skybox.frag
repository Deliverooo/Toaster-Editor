#version 460
#define __FRAG__

#extension GL_GOOGLE_include_directive: require
#include "common.glsl"

layout (location = 0u) in PSIn
{
    float3 position;
    float2 texCoord;
} m_In;

layout (location = 0u) out float4 o_Colour;

layout (push_constant) uniform PushData
{
    uint64 camera;
    uint32 samplerId;
    uint32 skyboxMap;
} pcs;

void main()
{
    float4 sky_colour = SAMPLE_CUBE(pcs.skyboxMap, pcs.samplerId, normalize(m_In.position), 0u);

    o_Colour = float4(sky_colour.rgb, 1.0f);
}