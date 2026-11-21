#pragma once

#include <filesystem>
#include <toast_gpu/api.hpp>
#include <toast_lib/map.hpp>
#include <toast_lib/pool.hpp>

#include "asset.hpp"

namespace tste
{
	struct AssetMetadata
	{
		AssetID               id{c_invalidAssetId};
		std::filesystem::path path; // The path to the actual asset, not the .meta
		uint64                sourceWriteTime{0ull};
		EAssetType            type{EAssetType::eMax};
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

	struct PendingDestroy
	{
		uint64     frame{0ull};
		uint64     handle{0ull};
		EAssetType type{EAssetType::eMax};
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

		Vector<PendingDestroy> pendingDestroys;
		uint64                 frameCounter{0ull};

		mtx_t                    jobQueueMtx;
		Vector<CompletedDiskJob> completedJobs;

		DecodedTexturePayload errorTexturePayload;
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
