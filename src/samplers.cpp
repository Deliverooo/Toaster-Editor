#include "toaster_editor/samplers.hpp"

#include <toast_gpu/api.hpp>

namespace tste
{
	consteval auto _get_sampler_descs() -> EArray<gpu::SamplerDesc, ESamplerType, ESamplerType::eMax>
	{
		EArray<gpu::SamplerDesc, ESamplerType, ESamplerType::eMax> out_descs;

		gpu::SamplerDesc &texture_sampler_linear_desc{out_descs[ESamplerType::eTextureSamplerLinear]};
		texture_sampler_linear_desc.minFilter    = gpu::EFilter::eLinear;
		texture_sampler_linear_desc.magFilter    = gpu::EFilter::eLinear;
		texture_sampler_linear_desc.mipmapMode   = gpu::ESamplerMipmapMode::eLinear;
		texture_sampler_linear_desc.addressModeU = gpu::ESamplerAddressMode::eRepeat;
		texture_sampler_linear_desc.addressModeV = gpu::ESamplerAddressMode::eRepeat;
		texture_sampler_linear_desc.addressModeW = gpu::ESamplerAddressMode::eRepeat;

		gpu::SamplerDesc &texture_sampler_nearest_desc{out_descs[ESamplerType::eTextureSamplerNearest]};
		texture_sampler_nearest_desc.minFilter    = gpu::EFilter::eNearest;
		texture_sampler_nearest_desc.magFilter    = gpu::EFilter::eNearest;
		texture_sampler_nearest_desc.mipmapMode   = gpu::ESamplerMipmapMode::eNearest;
		texture_sampler_nearest_desc.addressModeU = gpu::ESamplerAddressMode::eRepeat;
		texture_sampler_nearest_desc.addressModeV = gpu::ESamplerAddressMode::eRepeat;
		texture_sampler_nearest_desc.addressModeW = gpu::ESamplerAddressMode::eRepeat;

		return out_descs;
	}

	auto samplers_create(Samplers &p_out_samplers, gpu::GPUContext *p_gpu_ctx, gpu::SamplerDescriptorHeapHandle p_sampler_heap) -> void
	{
		p_out_samplers.gpuCtx      = p_gpu_ctx;
		p_out_samplers.samplerHeap = p_sampler_heap;

		constexpr auto sampler_descs{_get_sampler_descs()};

		gpu::sampler_descriptor_heap_alloc_sampler_slots(p_gpu_ctx, p_sampler_heap, p_out_samplers.heapSlots);
		gpu::sampler_descriptor_heap_write_samplers(p_gpu_ctx, p_sampler_heap, p_out_samplers.heapSlots, sampler_descs);
	}

	auto samplers_destroy(Samplers &p_samplers) -> void
	{
		gpu::sampler_descriptor_heap_free_sampler_slots(p_samplers.gpuCtx, p_samplers.samplerHeap, p_samplers.heapSlots);
	}
}
