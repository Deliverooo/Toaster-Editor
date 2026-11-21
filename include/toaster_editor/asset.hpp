#pragma once

#include <random>
#include <toast_render/material.hpp>

using namespace toaster;

namespace tste
{
	using AssetID = uint64;

	constexpr AssetID c_invalidAssetId{0ull};

	inline auto asset_id_generate() -> AssetID
	{
		thread_local std::random_device random_device{};
		thread_local std::mt19937_64    random_generator{random_device()};

		const AssetID id{random_generator()};
		return (id == c_invalidAssetId) ? 1ull : id;
	}

	enum class EAssetType : uint8
	{
		eTexture    = 0u,
		eMaterial   = 1u,
		eStaticMesh = 2u,
		eMax        = 3u
	};

	enum class EAssetState : uint8
	{
		eUnloaded,
		eLoading,
		eUploadingToGPU,
		eReady,
		eFailed
	};

	struct TextureAsset;
	TST_DECLARE_HANDLE(TextureAsset);

	struct TextureAsset
	{
		AssetID id{c_invalidAssetId};

		uint32      refCount{0u};
		EAssetState state{EAssetState::eUnloaded};

		gpu::TextureHandle      texture{nullptr};
		gpu::StateTrackerHandle stateTracker{nullptr};
		uint32                  shaderReadHeapSlot{UINT32_MAX};
	};

	struct MaterialAsset;
	TST_DECLARE_HANDLE(MaterialAsset);

	struct MaterialAsset
	{
		AssetID id{c_invalidAssetId};

		uint32      refCount{0u};
		EAssetState state{EAssetState::eUnloaded};

		rnd::MaterialHandle                              material{nullptr};
		rnd::PerMaterialTextureArray<TextureAssetHandle> textureDeps{};
	};
}
