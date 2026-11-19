#pragma once

#include <toast_gpu/fwd.hpp>
#include <toast_math/math_vector.hpp>

#include "tste_common.hpp"

namespace tste
{
	static constexpr uint16 c_maxPointLights{512u};

	struct PointLight
	{
		math::Float3 position;
		math::Float3 colour;
	};

	struct LightEnvironment
	{
		PFFArray<gpu::BufferHandle>  pointLightSSBOs;
		PFFArray<void *>             pointLightMappedDatas;
		PFFArray<gpu::DeviceAddress> pointLightBDAs;
	};

	auto light_env_create(LightEnvironment &p_out_light_env, gpu::GPUContext *p_gpu_ctx) -> void;
	auto light_env_destroy(LightEnvironment &p_light_env, gpu::GPUContext *p_gpu_ctx) -> void;

	struct PointLightComponent
	{
		math::Float3 colour;
	};
}
