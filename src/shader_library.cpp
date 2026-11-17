#include "toaster_editor/shader_library.hpp"

#include <toast_gpu/api.hpp>

#include <composite.frag.h>
#include <composite.mesh.h>
#include <depth_pre.mesh.h>
#include <diffuse_irradiance_convolution.comp.h>
#include <equirectangular_to_cubemap.comp.h>
#include <fullscreen_quad.mesh.h>
#include <meshlet_culling.task.h>
#include <object_culling.comp.h>
#include <oit_resolve.frag.h>
#include <skybox.frag.h>
#include <skybox.mesh.h>
#include <tst_pbr_static.frag.h>
#include <tst_pbr_static.mesh.h>
#include <tst_pbr_static_transparent.frag.h>

namespace tste
{
	// Because I am compiling the shaders into inline headers, I can compute the descs at compile time as well
	consteval auto _get_unlinked_shader_descs() -> TST_EARRAY(gpu::ShaderDesc, EUnlinkedShaderType)
	{
		TST_EARRAY(gpu::ShaderDesc, EUnlinkedShaderType) out_descs;

		gpu::ShaderDesc &equirectangular_to_cubemap_desc{out_descs[EUnlinkedShaderType::eEquirectangularToCubemapCS]};
		equirectangular_to_cubemap_desc.entryPointName = "main";
		equirectangular_to_cubemap_desc.code           = c_equirectangular_to_cubemap_comp_bytecode;
		equirectangular_to_cubemap_desc.codeSizeWords  = sizeof(c_equirectangular_to_cubemap_comp_bytecode) / sizeof(uint32);
		equirectangular_to_cubemap_desc.nextStage      = gpu::EShaderStageFlagBits::eNone;
		equirectangular_to_cubemap_desc.stage          = gpu::EShaderStageFlagBits::eCompute;
		equirectangular_to_cubemap_desc.noTaskShader   = true;

		gpu::ShaderDesc &diffuse_irradiance_convolution_desc{out_descs[EUnlinkedShaderType::eDiffuseIrradianceConvolutionCS]};
		diffuse_irradiance_convolution_desc.entryPointName = "main";
		diffuse_irradiance_convolution_desc.code           = c_diffuse_irradiance_convolution_comp_bytecode;
		diffuse_irradiance_convolution_desc.codeSizeWords  = sizeof(c_diffuse_irradiance_convolution_comp_bytecode) / sizeof(uint32);
		diffuse_irradiance_convolution_desc.nextStage      = gpu::EShaderStageFlagBits::eNone;
		diffuse_irradiance_convolution_desc.stage          = gpu::EShaderStageFlagBits::eCompute;
		diffuse_irradiance_convolution_desc.noTaskShader   = true;

		gpu::ShaderDesc &object_culling_shader_desc{out_descs[EUnlinkedShaderType::eObjectCullingCS]};
		object_culling_shader_desc.entryPointName = "main";
		object_culling_shader_desc.code           = c_object_culling_comp_bytecode;
		object_culling_shader_desc.codeSizeWords  = sizeof(c_object_culling_comp_bytecode) / sizeof(uint32);
		object_culling_shader_desc.nextStage      = gpu::EShaderStageFlagBits::eNone;
		object_culling_shader_desc.stage          = gpu::EShaderStageFlagBits::eCompute;
		object_culling_shader_desc.noTaskShader   = true;

		gpu::ShaderDesc &depth_pre_shader_desc{out_descs[EUnlinkedShaderType::eDepthPreMS]};
		depth_pre_shader_desc.entryPointName = "main";
		depth_pre_shader_desc.code           = c_depth_pre_mesh_bytecode;
		depth_pre_shader_desc.codeSizeWords  = sizeof(c_depth_pre_mesh_bytecode) / sizeof(uint32);
		depth_pre_shader_desc.nextStage      = gpu::EShaderStageFlagBits::eNone;
		depth_pre_shader_desc.stage          = gpu::EShaderStageFlagBits::eMesh;
		depth_pre_shader_desc.noTaskShader   = false;

		gpu::ShaderDesc &tst_pbr_mesh_shader_desc{out_descs[EUnlinkedShaderType::eStaticMeshMS]};
		tst_pbr_mesh_shader_desc.entryPointName = "main";
		tst_pbr_mesh_shader_desc.code           = c_tst_pbr_static_mesh_bytecode;
		tst_pbr_mesh_shader_desc.codeSizeWords  = sizeof(c_tst_pbr_static_mesh_bytecode) / sizeof(uint32);
		tst_pbr_mesh_shader_desc.nextStage      = gpu::EShaderStageFlagBits::ePixel;
		tst_pbr_mesh_shader_desc.stage          = gpu::EShaderStageFlagBits::eMesh;
		tst_pbr_mesh_shader_desc.noTaskShader   = false;

		gpu::ShaderDesc &meshlet_culling_task_shader_desc{out_descs[EUnlinkedShaderType::eStaticMeshTS]};
		meshlet_culling_task_shader_desc.entryPointName = "main";
		meshlet_culling_task_shader_desc.code           = c_meshlet_culling_task_bytecode;
		meshlet_culling_task_shader_desc.codeSizeWords  = sizeof(c_meshlet_culling_task_bytecode) / sizeof(uint32);
		meshlet_culling_task_shader_desc.nextStage      = gpu::EShaderStageFlagBits::eMesh;
		meshlet_culling_task_shader_desc.stage          = gpu::EShaderStageFlagBits::eTask;
		meshlet_culling_task_shader_desc.noTaskShader   = true;

		gpu::ShaderDesc &tst_pbr_pixel_shader_desc{out_descs[EUnlinkedShaderType::eStaticMeshOpaquePS]};
		tst_pbr_pixel_shader_desc.entryPointName = "main";
		tst_pbr_pixel_shader_desc.code           = c_tst_pbr_static_frag_bytecode;
		tst_pbr_pixel_shader_desc.codeSizeWords  = sizeof(c_tst_pbr_static_frag_bytecode) / sizeof(uint32);
		tst_pbr_pixel_shader_desc.nextStage      = gpu::EShaderStageFlagBits::eNone;
		tst_pbr_pixel_shader_desc.stage          = gpu::EShaderStageFlagBits::ePixel;
		tst_pbr_pixel_shader_desc.noTaskShader   = true;

		gpu::ShaderDesc &tst_pbr_transparent_pixel_shader_desc{out_descs[EUnlinkedShaderType::eStaticMeshTransparentPS]};
		tst_pbr_transparent_pixel_shader_desc.entryPointName = "main";
		tst_pbr_transparent_pixel_shader_desc.code           = c_tst_pbr_static_transparent_frag_bytecode;
		tst_pbr_transparent_pixel_shader_desc.codeSizeWords  = sizeof(c_tst_pbr_static_transparent_frag_bytecode) / sizeof(uint32);
		tst_pbr_transparent_pixel_shader_desc.nextStage      = gpu::EShaderStageFlagBits::eNone;
		tst_pbr_transparent_pixel_shader_desc.stage          = gpu::EShaderStageFlagBits::ePixel;
		tst_pbr_transparent_pixel_shader_desc.noTaskShader   = true;

		return out_descs;
	}

	auto shader_library_create(ShaderLibrary &p_out_library, gpu::GPUContext *p_gpu_ctx) -> void
	{
		// OIT resolve shaders
		constexpr TST_EARRAY(gpu::ShaderDesc, EOITResolveShaderStage) oit_resolve_shader_descs{
			gpu::ShaderDesc{
				.entryPointName = "main",
				.code = c_fullscreen_quad_mesh_bytecode,
				.codeSizeWords = sizeof(c_fullscreen_quad_mesh_bytecode) / sizeof(uint32),
				.nextStage = gpu::EShaderStageFlagBits::ePixel,
				.stage = gpu::EShaderStageFlagBits::eMesh,
				.noTaskShader = true,
			},
			gpu::ShaderDesc{
				.entryPointName = "main",
				.code = c_oit_resolve_frag_bytecode,
				.codeSizeWords = sizeof(c_oit_resolve_frag_bytecode) / sizeof(uint32),
				.nextStage = gpu::EShaderStageFlagBits::eNone,
				.stage = gpu::EShaderStageFlagBits::ePixel,
				.noTaskShader = true,
			}
		};

		gpu::shader_create_n(p_gpu_ctx, p_out_library.oitResolveShaders, oit_resolve_shader_descs, true);

		// Skybox shaders
		constexpr TST_EARRAY(gpu::ShaderDesc, ESkyboxShaderStage) skybox_shader_descs{
			gpu::ShaderDesc{
				.entryPointName = "main",
				.code = c_skybox_mesh_bytecode,
				.codeSizeWords = sizeof(c_skybox_mesh_bytecode) / sizeof(uint32),
				.nextStage = gpu::EShaderStageFlagBits::ePixel,
				.stage = gpu::EShaderStageFlagBits::eMesh,
				.noTaskShader = true,
			},
			gpu::ShaderDesc{
				.entryPointName = "main",
				.code = c_skybox_frag_bytecode,
				.codeSizeWords = sizeof(c_skybox_frag_bytecode) / sizeof(uint32),
				.nextStage = gpu::EShaderStageFlagBits::eNone,
				.stage = gpu::EShaderStageFlagBits::ePixel,
				.noTaskShader = true,
			}
		};

		gpu::shader_create_n(p_gpu_ctx, p_out_library.skyboxShaders, skybox_shader_descs, true);

		// Composite shaders
		constexpr TST_EARRAY(gpu::ShaderDesc, ECompositeShaderStage) composite_shader_descs{
			gpu::ShaderDesc{
				.entryPointName = "main",
				.code = c_composite_mesh_bytecode,
				.codeSizeWords = sizeof(c_composite_mesh_bytecode) / sizeof(uint32),
				.nextStage = gpu::EShaderStageFlagBits::ePixel,
				.stage = gpu::EShaderStageFlagBits::eMesh,
				.noTaskShader = true,
			},
			gpu::ShaderDesc{
				.entryPointName = "main",
				.code = c_composite_frag_bytecode,
				.codeSizeWords = sizeof(c_composite_frag_bytecode) / sizeof(uint32),
				.nextStage = gpu::EShaderStageFlagBits::eNone,
				.stage = gpu::EShaderStageFlagBits::ePixel,
				.noTaskShader = true,
			}
		};

		gpu::shader_create_n(p_gpu_ctx, p_out_library.compositeShaders, composite_shader_descs, true);

		// The rest of the shaders that cannot be linked
		constexpr auto shader_descs{_get_unlinked_shader_descs()};

		// Do not link these shaders, but still create them together
		gpu::shader_create_n(p_gpu_ctx, p_out_library.shaders, shader_descs, false);
	}

	auto shader_library_destroy(ShaderLibrary &p_library, gpu::GPUContext *p_gpu_ctx) -> void
	{
		gpu::shader_destroy_n(p_gpu_ctx, p_library.shaders);
		gpu::shader_destroy_n(p_gpu_ctx, p_library.compositeShaders);
		gpu::shader_destroy_n(p_gpu_ctx, p_library.skyboxShaders);
		gpu::shader_destroy_n(p_gpu_ctx, p_library.oitResolveShaders);
	}
}
