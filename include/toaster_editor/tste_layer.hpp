#pragma once

#include <toast_render/material.hpp>

#include "application.hpp"
#include "editor_camera.hpp"
#include "shader_library.hpp"

#include "scene.hpp"
#include "scene_renderer.hpp"

namespace tste
{
	struct TSTELayer
	{
		gpu::CommandAllocatorHandle singleTimeCommandAllocator;

		Samplers           samplers;
		ShaderLibrary      shaderLib;
		EnvironmentContext environmentCtx;
		Scene              scene;
		SceneRenderer      sceneRenderer;

		EditorCamera                     editorCamera; // TODO: Actual first person camera that is not ripped from OEX
		PFFArray<gpu::BufferHandle>  cameraUBOs;
		PFFArray<gpu::DeviceAddress> cameraBDAs;

		rnd::MaterialHandle defaultMaterial;

		entt::entity testEntity;
	};

	auto tste_init(Application &p_app, TSTELayer &p_layer) -> void;
	auto tste_destroy(Application &p_app, TSTELayer &p_layer) -> void;
	auto tste_on_update(Application &p_app, TSTELayer &p_layer, float32 p_dt) -> void;
	auto tste_on_render(Application &p_app, TSTELayer &p_layer, gpu::CommandListHandle p_cmd) -> void;
	auto tste_on_event(Application &p_app, TSTELayer &p_layer, knl::Event &p_event) -> void;
}
