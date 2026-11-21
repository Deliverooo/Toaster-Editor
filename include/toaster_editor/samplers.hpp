#pragma once

#include "tste_common.hpp"
#include  <toast_gpu/fwd.hpp>

namespace tste
{
	enum class ESamplerType : uint8 { eTextureSamplerLinear = 0u, eTextureSamplerNearest = 1u, eMax = 2u };

	struct Samplers
	{
		TST_DEPENDS(gpu::GPUContext*, gpuCtx);
		TST_DEPENDS(gpu::SamplerDescriptorHeapHandle, samplerHeap);

		TST_EARRAY(uint32, ESamplerType) heapSlots;
	};

	auto samplers_create(Samplers &p_out_samplers, gpu::GPUContext *p_gpu_ctx, gpu::SamplerDescriptorHeapHandle p_sampler_heap) -> void;
	auto samplers_destroy(Samplers &p_samplers) -> void;
}
