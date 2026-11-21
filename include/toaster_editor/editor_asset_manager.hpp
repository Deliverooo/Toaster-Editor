#pragma once

#include <filesystem>
#include <toast_lib/map.hpp>
#include <toast_lib/pool.hpp>

#include "asset.hpp"

namespace tste
{
	struct AssetMetadata
	{
		AssetID               id{c_invalidAssetId};
		EAssetType            type;
		std::filesystem::path path;
	};

	struct EditorAssetManagerDesc
	{
		rnd::MaterialManager *            materialMngr{nullptr};
		gpu::GPUContext *                 gpuCtx{nullptr};
		gpu::AllocationContext *          allocCtx{nullptr};
		gpu::UploadContext *              uploadCtx{nullptr};
		gpu::ResourceDescriptorHeapHandle resourceHeap{nullptr};
	};

	struct CompletedDiskJob
	{
		AssetID    assetId{c_invalidAssetId};
		void *     payload{nullptr};
		EAssetType type{EAssetType::eMax};
		bool       success{false};
	};

	struct EditorAssetManager
	{
		EditorAssetManagerDesc desc;

		UnorderedMap<AssetID, AssetMetadata>         registry;
		UnorderedMap<std::filesystem::path, AssetID> pathToId;

		FixedPool<TextureAsset>  textureAssets;
		FixedPool<MaterialAsset> materialAssets;

		UnorderedMap<AssetID, TextureAssetHandle>  loadedTextures;
		UnorderedMap<AssetID, MaterialAssetHandle> loadedMaterials;

		mtx_t                    jobQueueMtx;
		Vector<CompletedDiskJob> completedJobs;

		gpu::TextureHandle fallbackTexture;
		uint32             fallbackTextureShaderReadHeapSlot;
	};

	auto asset_mngr_create(EditorAssetManager &p_out_mngr, const EditorAssetManagerDesc &p_desc) -> void;
	auto asset_mngr_destroy(EditorAssetManager &p_mngr) -> void;
	auto asset_mngr_update(EditorAssetManager &p_mngr) -> void;

	auto texture_asset_acquire(EditorAssetManager &p_mngr, TextureAssetHandle p_handle) -> void;
	auto texture_asset_release(EditorAssetManager &p_mngr, TextureAssetHandle p_handle) -> void;

	auto material_asset_acquire(EditorAssetManager &p_mngr, MaterialAssetHandle p_handle) -> void;
	auto material_asset_release(EditorAssetManager &p_mngr, MaterialAssetHandle p_handle) -> void;

	auto texture_asset_load(EditorAssetManager &p_mngr, AssetID p_id) -> TextureAssetHandle;

	auto material_asset_load(EditorAssetManager &p_mngr, AssetID p_id) -> MaterialAssetHandle;
}
