#include "toaster_editor/environment.hpp"
#include "toaster_editor/shader_library.hpp"

#include <toast_gpu/api.hpp>
#include <toast_gpu/upload.hpp>

namespace tste
{
	auto environment_ctx_create(EnvironmentContext &              p_out_ctx, gpu::GPUContext *p_gpu_ctx, gpu::UploadContext *p_upload_ctx,
								gpu::ResourceDescriptorHeapHandle p_resource_heap) -> void
	{
		p_out_ctx.gpuCtx       = p_gpu_ctx;
		p_out_ctx.uploadCtx    = p_upload_ctx;
		p_out_ctx.resourceHeap = p_resource_heap;

		p_out_ctx.environments.setDestructorUserData(&p_out_ctx);
		p_out_ctx.environments.setDestructorFn(+[](Environment *p_environment, void *p_user_data) -> void
		{
			const auto ctx{static_cast<EnvironmentContext *>(p_user_data)};
			gpu::texture_destroy_n(ctx->gpuCtx, p_environment->textures);
			gpu::resource_descriptor_heap_free_texture_slots(ctx->gpuCtx, ctx->resourceHeap, p_environment->shaderReadHeapSlots);
		});

		constexpr TST_EARRAY(gpu::TextureDesc, EEnvironmentMapType) environment_descs{
			gpu::TextureDesc{
				.extent = {1u, 1u, 1u},
				.mipCount = 1u,
				.layerCount = 6u,
				.type = gpu::ETextureType::eCube,
				.sampleCount = gpu::ESampleCount::e1,
				.format = gpu::EFormat::eR8G8B8A8Srgb,
				.usage = gpu::ETextureUsageFlagBits::eTransferDst | gpu::ETextureUsageFlagBits::eSampled
			},
			gpu::TextureDesc{
				.extent = {1u, 1u, 1u},
				.mipCount = 1u,
				.layerCount = 6u,
				.type = gpu::ETextureType::eCube,
				.sampleCount = gpu::ESampleCount::e1,
				.format = gpu::EFormat::eR8G8B8A8Srgb,
				.usage = gpu::ETextureUsageFlagBits::eTransferDst | gpu::ETextureUsageFlagBits::eSampled
			}
		};

		Environment default_environment{};
		gpu::texture_create_n(p_gpu_ctx, environment_descs, default_environment.textures);

		const gpu::StateTrackerHandle default_resource_state_tracker{gpu::state_tracker_register(p_upload_ctx, 1u)};

		constexpr Array<uint32, 6u> default_skybox_data{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
		constexpr Array<uint32, 6u> default_diffuse_irradiance_data{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

		constexpr TST_EARRAY(gpu::BufferTextureCopyRegion, EEnvironmentMapType) copy_regions{
			gpu::BufferTextureCopyRegion{.textureSubresource = {.layerCount = 6u}},
			gpu::BufferTextureCopyRegion{.textureSubresource = {.layerCount = 6u}}
		};
		TST_EARRAY(gpu::TextureUploadDesc, EEnvironmentMapType) upload_descs{
			gpu::TextureUploadDesc{.data = default_skybox_data.data(), .size = default_skybox_data.size_bytes(), .uploadRegions = copy_regions[0u]},
			gpu::TextureUploadDesc{
				.data = default_diffuse_irradiance_data.data(),
				.size = default_diffuse_irradiance_data.size_bytes(),
				.uploadRegions = copy_regions[1u]
			}
		};

		gpu::upload_data_to_textures(p_upload_ctx, default_environment.textures, upload_descs, default_resource_state_tracker);
		gpu::state_tracker_wait_for(p_upload_ctx, default_resource_state_tracker);

		gpu::resource_descriptor_heap_alloc_texture_slots(p_gpu_ctx, p_resource_heap, default_environment.shaderReadHeapSlots);
		gpu::resource_descriptor_heap_write_textures(p_gpu_ctx, p_resource_heap, default_environment.shaderReadHeapSlots, default_environment.textures, false,
													 gpu::c_allMipLevels);
		p_out_ctx.defaultEnvironment = p_out_ctx.environments.emplace(default_environment);
	}

	auto environment_ctx_destroy(EnvironmentContext &p_ctx) -> void
	{
		p_ctx.environments.destroy(p_ctx.defaultEnvironment);
		p_ctx.environments.clear();
	}

	auto environment_create(EnvironmentContext &p_ctx) -> EnvironmentHandle
	{
		return p_ctx.environments.emplace();
	}

	auto environment_destroy(EnvironmentContext &p_ctx, EnvironmentHandle p_environment) -> void
	{
		p_ctx.environments.destroy(p_environment);
	}
}
