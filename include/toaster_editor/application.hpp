#pragma once

#include <toast_kernel/window.hpp>

namespace toaster::rnd
{
	struct MaterialManager;
	struct MeshManager;
	struct TransformManager;
}

using namespace toaster;

namespace tste
{
	struct Application;

	struct AppLayer
	{
		using InitFn     = void(*)(Application &, void *);
		using DestroyFn  = void(*)(Application &, void *);
		using OnRenderFn = void(*)(Application &, void *, gpu::CommandListHandle);
		using OnUpdateFn = void(*)(Application &, void *, float32);
		using OnEventFn  = void(*)(Application &, void *, knl::Event &);

		InitFn     initFn{nullptr};
		DestroyFn  destroyFn{nullptr};
		OnRenderFn onRenderFn{nullptr};
		OnUpdateFn onUpdateFn{nullptr};
		OnEventFn  onEventFn{nullptr};

		void *userData{nullptr}; // This is an TSTELayer*
	};

	constexpr uint32 c_appLayerCount{1u}; // Just TSTELayer

	struct Application
	{
		Application() = default;
		// I don't want to accidentally copy the application
		Application(const Application &)            = delete;
		Application(Application &&)                 = delete;
		Application &operator=(const Application &) = delete;
		Application &operator=(Application &&)      = delete;

		gpu::GPUContext *                 gpuCtx{nullptr};
		gpu::FrameContext *               frameCtx{nullptr};
		gpu::AllocationContext *          allocCtx{nullptr};
		gpu::UploadContext *              uploadCtx{nullptr};
		gpu::ResourceDescriptorHeapHandle resourceHeap{nullptr};
		gpu::SamplerDescriptorHeapHandle  samplerHeap{nullptr};

		rnd::MaterialManager * materialMngr{nullptr};
		rnd::MeshManager *     meshMngr{nullptr};
		rnd::TransformManager *transformMngr{nullptr};

		knl::Window window;

		Array<AppLayer, c_appLayerCount> layers;

		uint32  frameIndex{0u};
		float32 deltaTime{0.0f};

		bool isRunning{true}; // Layers can quit the app by setting this to false
	};

	inline auto application_layer_init(Application &p_app, uint32 p_layer_index) -> void
	{
		const AppLayer &layer{p_app.layers[p_layer_index]};
		if (layer.initFn)
			layer.initFn(p_app, layer.userData);
	}

	inline auto application_layer_destroy(Application &p_app, uint32 p_layer_index) -> void
	{
		const AppLayer &layer{p_app.layers[p_layer_index]};
		if (layer.destroyFn)
			layer.destroyFn(p_app, layer.userData);
	}
}
