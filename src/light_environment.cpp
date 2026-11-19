#include "toaster_editor/light_environment.hpp"

#include <toast_gpu/api.hpp>

namespace tste
{
	auto light_env_create(LightEnvironment &p_out_light_env, gpu::GPUContext *p_gpu_ctx) -> void
	{
		constexpr gpu::BufferDesc point_light_buffer_desc{
			.size = c_maxPointLights * sizeof(PointLight),
			.usage = gpu::EBufferUsageFlagBits::eStorageBuffer,
			.memoryType = gpu::EMemoryType::eHostVisibleCoherent
		};
		gpu::buffer_create_n(p_gpu_ctx, point_light_buffer_desc, p_out_light_env.pointLightSSBOs);
		gpu::buffer_get_address_n(p_gpu_ctx, p_out_light_env.pointLightSSBOs, p_out_light_env.pointLightBDAs);
		gpu::buffer_get_mapped_data_n(p_gpu_ctx, p_out_light_env.pointLightSSBOs, p_out_light_env.pointLightMappedDatas);
	}

	auto light_env_destroy(LightEnvironment &p_light_env, gpu::GPUContext *p_gpu_ctx) -> void
	{
		gpu::buffer_destroy_n(p_gpu_ctx, p_light_env.pointLightSSBOs);
	}
}
