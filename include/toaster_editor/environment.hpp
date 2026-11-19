#pragma once

#include <toast_gpu/fwd.hpp>
#include <toast_lib/pool.hpp>

#include "tste_common.hpp"

namespace tste
{
	TST_DECLARE_HANDLE(Environment);

	enum class EEnvironmentMapType : uint8 { eSkybox = 0u, eDiffuseIrradiance = 1u, eMax = 2u };

	struct Environment
	{
		TST_EARRAY(gpu::TextureHandle, EEnvironmentMapType) textures;
		TST_EARRAY(uint32, EEnvironmentMapType)             shaderReadHeapSlots;
	};

	struct EnvironmentContext
	{
		TST_DEPENDS(gpu::GPUContext*, gpuCtx);
		TST_DEPENDS(gpu::UploadContext*, uploadCtx);
		TST_DEPENDS(gpu::ResourceDescriptorHeapHandle, resourceHeap);

		FixedPool<Environment, 16u, 8u> environments;
		EnvironmentHandle               defaultEnvironment;
	};

	auto environment_ctx_create(EnvironmentContext &              p_out_ctx, gpu::GPUContext *p_gpu_ctx, gpu::UploadContext *p_upload_ctx,
								gpu::ResourceDescriptorHeapHandle p_resource_heap) -> void;
	auto environment_ctx_destroy(EnvironmentContext &p_ctx) -> void;

	auto environment_create(EnvironmentContext& p_ctx) -> EnvironmentHandle;
	auto environment_destroy(EnvironmentContext& p_ctx, EnvironmentHandle p_environment) -> void;
}
