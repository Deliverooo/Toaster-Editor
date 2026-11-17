#pragma once

#include <toast_gpu/fwd.hpp>

#include "tste_common.hpp"

namespace tste
{
	#define TSTE_MESH_PIXEL_STAGES(__name)  enum class __name : uint8 { eMesh = 0u, ePixel = 1u, eMax = 2u }

	TSTE_MESH_PIXEL_STAGES(EOITResolveShaderStage);
	TSTE_MESH_PIXEL_STAGES(ESkyboxShaderStage);
	TSTE_MESH_PIXEL_STAGES(ECompositeShaderStage);

	#undef TSTE_MESH_PIXEL_STAGES

	enum class EUnlinkedShaderType : uint8
	{
		eEquirectangularToCubemapCS     = 0u,
		eDiffuseIrradianceConvolutionCS = 1u,
		eObjectCullingCS                = 2u,
		eDepthPreMS                     = 3u,
		eStaticMeshTS                   = 4u,
		eStaticMeshMS                   = 5u,
		eStaticMeshOpaquePS             = 6u,
		eStaticMeshTransparentPS        = 7u,
		eMax                            = 8u
	};

	// Represents all the shaders ever needed by OEX. Loaded once at startup
	struct ShaderLibrary
	{
		// These shaders are linked, so they are in separate arrays
		TST_EARRAY(gpu::ShaderHandle, EOITResolveShaderStage) oitResolveShaders;
		TST_EARRAY(gpu::ShaderHandle, ESkyboxShaderStage)     skyboxShaders;
		TST_EARRAY(gpu::ShaderHandle, ECompositeShaderStage)  compositeShaders;

		// These shaders are unlinked, so they are in one big array for batch creation
		TST_EARRAY(gpu::ShaderHandle, EUnlinkedShaderType) shaders;
	};

	#undef TSTE_SHADER_ARRAY

	auto shader_library_create(ShaderLibrary &p_out_library, gpu::GPUContext *p_gpu_ctx) -> void;
	auto shader_library_destroy(ShaderLibrary &p_library, gpu::GPUContext *p_gpu_ctx) -> void;
}
