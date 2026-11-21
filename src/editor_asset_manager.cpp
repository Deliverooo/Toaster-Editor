#include "toaster_editor/editor_asset_manager.hpp"

#include <execution>
#include <print>
#include <ranges>
#include <toast_gpu/api.hpp>
#include <toast_gpu/upload.hpp>

#include <stb/stb_image.h>

namespace tste
{
	struct DecodedTexturePayload
	{
		Vector<uint8> pixelData;
		math::UInt2   extent{0u, 0u};
		uint32        mipLevels{1u};
		gpu::EFormat  format{gpu::EFormat::eUndefined};
	};

	struct DecodedMaterialPayload
	{
		math::Float4                          albedoColour{1.0f, 1.0f, 1.0f, 1.0f};
		float32                               roughness{0.5f};
		float32                               metalness{0.0f};
		rnd::EMaterialFlags                   flags{};
		rnd::PerMaterialTextureArray<AssetID> textureIds{};
	};

	auto _decode_texture_from_disc(const std::filesystem::path &p_path, DecodedTexturePayload *p_payload) -> bool
	{
		int32 width{0};
		int32 height{0};
		int32 num_channels{0};

		uint8 *data{stbi_load(p_path.string().c_str(), &width, &height, &num_channels, 4)};
		if (!data)
			return false;

		const uint64 buffer_size{width * height * sizeof(uint32)};
		p_payload->pixelData.resize(buffer_size);
		std::memcpy(p_payload->pixelData.data(), data, buffer_size);

		p_payload->format    = gpu::EFormat::eB8G8R8A8Srgb;
		p_payload->extent    = {static_cast<uint32>(width), static_cast<uint32>(height)};
		p_payload->mipLevels = 1u;
		// p_payload->mipLevels = static_cast<uint32>(std::floor(std::log2(std::max(width, height)))) + 1u;

		stbi_image_free(data);

		return true;
	}

	auto _decode_material_from_disc(const std::filesystem::path &p_path, DecodedMaterialPayload *p_payload) -> bool
	{
		// TODO: Parse material

		return true;
	}

	auto _process_completed_texture_job(EditorAssetManager &p_mngr, const CompletedDiskJob &p_job) -> void
	{
		auto it{p_mngr.loadedTextures.find(p_job.assetId)};
		if (it == p_mngr.loadedTextures.end())
		{
			delete static_cast<DecodedTexturePayload *>(p_job.payload);
			return;
		}

		auto asset{p_mngr.textureAssets.try_get(it->second)};
		if (!asset)
		{
			delete static_cast<DecodedTexturePayload *>(p_job.payload);
			return;
		}

		auto payload{static_cast<DecodedTexturePayload *>(p_job.payload)};
		if (!p_job.success || !payload)
		{
			asset->state = EAssetState::eFailed;
			return;
		}

		gpu::TextureDesc texture_desc{};
		texture_desc.extent      = {payload->extent.x, payload->extent.y, 1u};
		texture_desc.mipCount    = payload->mipLevels;
		texture_desc.layerCount  = 1u;
		texture_desc.type        = gpu::ETextureType::e2D;
		texture_desc.sampleCount = gpu::ESampleCount::e1;
		texture_desc.format      = payload->format;
		texture_desc.usage       = gpu::ETextureUsageFlagBits::eSampled | gpu::ETextureUsageFlagBits::eTransferSrc | gpu::ETextureUsageFlagBits::eTransferDst;

		// Create the texture and heap slot into the pre-allocated handles
		gpu::texture_create_into(p_mngr.desc.gpuCtx, asset->texture, texture_desc);
		gpu::resource_descriptor_heap_write_texture(p_mngr.desc.gpuCtx, p_mngr.desc.resourceHeap, asset->shaderReadHeapSlot, asset->texture, false);

		constexpr gpu::BufferTextureCopyRegion copy_region{
			.bufferOffset = 0ull,
			.textureOffset = {0, 0, 0},
			.textureExtent = {0u, 0u, 0u},
			.textureSubresource = {.mipLevel = 0u, .baseLayer = 0u, .layerCount = 1u}
		};

		asset->state = EAssetState::eUploadingToGPU;
		gpu::upload_data_to_textures(p_mngr.desc.uploadCtx, asset->texture,
									 gpu::TextureUploadDesc{.data = payload->pixelData.data(), .size = payload->pixelData.size(), .uploadRegions = copy_region},
									 asset->stateTracker);
	}

	auto _process_completed_material_job(EditorAssetManager &p_mngr, const CompletedDiskJob &p_job) -> void
	{
		auto it{p_mngr.loadedMaterials.find(p_job.assetId)};
		if (it == p_mngr.loadedMaterials.end())
		{
			delete static_cast<DecodedMaterialPayload *>(p_job.payload);
			return;
		}

		auto asset{p_mngr.materialAssets.try_get(it->second)};
		if (!asset)
		{
			delete static_cast<DecodedMaterialPayload *>(p_job.payload);
			return;
		}

		auto payload{static_cast<DecodedMaterialPayload *>(p_job.payload)};
		if (!p_job.success || !payload)
		{
			asset->state = EAssetState::eFailed;
			return;
		}

		rnd::MaterialManager *material_mngr{p_mngr.desc.materialMngr};
		rnd::material_set_albedo_colour(material_mngr, asset->material, payload->albedoColour);
		rnd::material_set_roughness(material_mngr, asset->material, payload->roughness);
		rnd::material_set_metalness(material_mngr, asset->material, payload->metalness);
		rnd::material_set_flags(material_mngr, asset->material, payload->flags, true);

		for (uint8_t i = 0; i < static_cast<uint8_t>(rnd::EMaterialTextureType::eMax); ++i)
		{
			const auto    texture_type{static_cast<rnd::EMaterialTextureType>(i)};
			const AssetID texture_id{payload->textureIds[texture_type]};

			if (texture_id == 0)
				continue;

			const TextureAssetHandle texture_handle{texture_asset_load(p_mngr, texture_id)};
			asset->textureDeps[texture_type] = texture_handle;

			if (const auto *texture{p_mngr.textureAssets.try_get(texture_handle)})
			{
				// This is safe because the texture's heap slot and gpu handle were pre-allocated
				rnd::material_set_texture(material_mngr, asset->material, texture_type, rnd::MaterialTexture{
											  .texture = texture->texture,
											  .stateTracker = texture->stateTracker,
											  .shaderReadHeapSlot = texture->shaderReadHeapSlot
										  });
			}
		}
		rnd::material_mark_dirty(material_mngr, asset->material);
		asset->state = EAssetState::eReady;
		delete payload;
	}

	auto asset_mngr_create(EditorAssetManager &p_out_mngr, const EditorAssetManagerDesc &p_desc) -> void
	{
		p_out_mngr.desc = p_desc;

		p_out_mngr.textureAssets.setDestructorUserData(&p_out_mngr);
		p_out_mngr.textureAssets.setDestructorFn(+[](TextureAsset *p_data, void *p_user_data) -> void
		{
			const auto mngr{static_cast<EditorAssetManager *>(p_user_data)};

			TST_ASSERT_MSG(p_data->refCount == 0u, "You forgot to call texture_asset_release on one of your textures");

			gpu::resource_descriptor_heap_free_texture_slot(mngr->desc.gpuCtx, mngr->desc.resourceHeap, p_data->shaderReadHeapSlot);
			gpu::texture_destroy(mngr->desc.gpuCtx, p_data->texture);
		});

		p_out_mngr.materialAssets.setDestructorUserData(&p_out_mngr);
		p_out_mngr.materialAssets.setDestructorFn(+[](MaterialAsset *p_data, void *p_user_data) -> void
		{
			const auto mngr{static_cast<EditorAssetManager *>(p_user_data)};

			TST_ASSERT_MSG(p_data->refCount == 0u, "You forgot to call material_asset_release on one of your materials");

			rnd::material_destroy(mngr->desc.materialMngr, p_data->material);

			// Release child texture dependencies
			for (TextureAssetHandle texture: p_data->textureDeps)
			{
				if (texture.valid())
					texture_asset_release(*mngr, texture);
			}
		});

		mtx_init(&p_out_mngr.jobQueueMtx, mtx_plain);
	}

	auto asset_mngr_destroy(EditorAssetManager &p_mngr) -> void
	{
		mtx_destroy(&p_mngr.jobQueueMtx);

		p_mngr.materialAssets.clear();
		p_mngr.textureAssets.clear();
	}

	auto asset_mngr_update(EditorAssetManager &p_mngr) -> void
	{
		Vector<CompletedDiskJob> completed_jobs;

		mtx_lock(&p_mngr.jobQueueMtx);
		completed_jobs.swap(p_mngr.completedJobs);
		mtx_unlock(&p_mngr.jobQueueMtx);

		for (const auto &job: completed_jobs)
		{
			switch (job.type)
			{
				case EAssetType::eTexture:
				{
					_process_completed_texture_job(p_mngr, job);
					break;
				}
				case EAssetType::eMaterial:
				{
					_process_completed_material_job(p_mngr, job);
					break;
				}
				default: break;
			}
		}

		// Remove all unused textures
		for (auto texture_it{p_mngr.loadedTextures.begin()}; texture_it != p_mngr.loadedTextures.end();)
		{
			const TextureAsset &asset{p_mngr.textureAssets[texture_it->second]};
			if (asset.refCount == 0u && asset.state != EAssetState::eLoading && asset.state != EAssetState::eUploadingToGPU)
			{
				p_mngr.textureAssets.destroy(texture_it->second);
				texture_it = p_mngr.loadedTextures.erase(texture_it);
			}
			else
				++texture_it;
		}

		// Remove all unused materials
		for (auto material_it{p_mngr.loadedMaterials.begin()}; material_it != p_mngr.loadedMaterials.end();)
		{
			const MaterialAsset &asset{p_mngr.materialAssets[material_it->second]};
			if (asset.refCount == 0u && asset.state != EAssetState::eLoading && asset.state != EAssetState::eUploadingToGPU)
			{
				p_mngr.materialAssets.destroy(material_it->second);
				material_it = p_mngr.loadedMaterials.erase(material_it);
			}
			else
				++material_it;
		}
	}

	auto texture_asset_acquire(EditorAssetManager &p_mngr, TextureAssetHandle p_handle) -> void
	{
		TextureAsset &asset{p_mngr.textureAssets[p_handle]};
		++asset.refCount;
	}

	auto texture_asset_release(EditorAssetManager &p_mngr, TextureAssetHandle p_handle) -> void
	{
		TextureAsset &asset{p_mngr.textureAssets[p_handle]};
		--asset.refCount;
	}

	auto material_asset_acquire(EditorAssetManager &p_mngr, MaterialAssetHandle p_handle) -> void
	{
		MaterialAsset &asset{p_mngr.materialAssets[p_handle]};
		++asset.refCount;
	}

	auto material_asset_release(EditorAssetManager &p_mngr, MaterialAssetHandle p_handle) -> void
	{
		MaterialAsset &asset{p_mngr.materialAssets[p_handle]};
		--asset.refCount;
	}

	auto texture_asset_load(EditorAssetManager &p_mngr, AssetID p_id) -> TextureAssetHandle
	{
		if (p_id == c_invalidAssetId)
			return nullptr;

		if (const auto it{p_mngr.loadedTextures.find(p_id)}; it != p_mngr.loadedTextures.end())
		{
			if (const auto asset{p_mngr.textureAssets.try_get(it->second)})
			{
				++asset->refCount;
				return it->second;
			}
		}

		const auto metadata_it{p_mngr.registry.find(p_id)};
		if (metadata_it == p_mngr.registry.end())
			return nullptr;

		const gpu::StateTrackerHandle state_tracker{gpu::state_tracker_register(p_mngr.desc.uploadCtx, 1u)};

		const TextureAssetHandle out_handle{
			p_mngr.textureAssets.emplace(TextureAsset{
											 .id = p_id,
											 .refCount = 1u,
											 .state = EAssetState::eLoading,
											 .texture = gpu::texture_register_null(p_mngr.desc.gpuCtx),
											 .stateTracker = state_tracker,
											 .shaderReadHeapSlot = gpu::resource_descriptor_heap_alloc_texture_slot(p_mngr.desc.gpuCtx, p_mngr.desc.resourceHeap)
										 })
		};

		gpu::state_tracker_register_ready_callback(p_mngr.desc.uploadCtx, state_tracker, [out_handle, &p_mngr]() -> void
		{
			auto asset{p_mngr.textureAssets.try_get(out_handle)};
			if (!asset)
				return;

			asset->state        = EAssetState::eReady;
			asset->stateTracker = nullptr;
		});

		p_mngr.loadedTextures[p_id] = out_handle;

		const std::filesystem::path &texture_path{metadata_it->second.path}; // TODO: Relative paths and asset root directory / VFS

		// TODO: Async this
		{
			auto       payload{new DecodedTexturePayload{}};
			const bool success{_decode_texture_from_disc(texture_path, payload)};

			if (!success)
			{
				delete payload;
				payload = nullptr;
				// Still progress because this will indicate the texture's state should be set to eFailed
			}

			mtx_lock(&p_mngr.jobQueueMtx);
			p_mngr.completedJobs.emplace_back(CompletedDiskJob{.assetId = p_id, .payload = payload, .type = EAssetType::eTexture, .success = success});
			mtx_unlock(&p_mngr.jobQueueMtx);
		}

		return out_handle;
	}

	auto material_asset_load(EditorAssetManager &p_mngr, AssetID p_id) -> MaterialAssetHandle
	{
		if (p_id == c_invalidAssetId)
			return nullptr;

		if (const auto it{p_mngr.loadedMaterials.find(p_id)}; it != p_mngr.loadedMaterials.end())
		{
			if (const auto asset{p_mngr.materialAssets.try_get(it->second)})
			{
				++asset->refCount;
				return it->second;
			}
		}

		const auto metadata_it{p_mngr.registry.find(p_id)};
		if (metadata_it == p_mngr.registry.end())
			return nullptr;

		rnd::MaterialManager *material_mngr{p_mngr.desc.materialMngr};

		rnd::MaterialHandle rnd_material{rnd::material_create(material_mngr)};

		MaterialAsset material_asset{};
		material_asset.id       = p_id;
		material_asset.refCount = 1u;
		material_asset.state    = EAssetState::eLoading;
		material_asset.material = rnd_material;

		const MaterialAssetHandle out_handle{p_mngr.materialAssets.emplace(material_asset)};

		p_mngr.loadedMaterials[p_id] = out_handle;

		const std::filesystem::path &material_path{metadata_it->second.path}; // TODO: Relative paths and asset root directory / VFS
		// TODO: Async this
		{
			auto       payload{new DecodedMaterialPayload{}};
			const bool success{_decode_material_from_disc(material_path, payload)};

			if (!success)
			{
				delete payload;
				payload = nullptr;
			}

			mtx_lock(&p_mngr.jobQueueMtx);
			p_mngr.completedJobs.emplace_back(CompletedDiskJob{.assetId = p_id, .payload = payload, .type = EAssetType::eMaterial, .success = success});
			mtx_unlock(&p_mngr.jobQueueMtx);
		}
	}
}
