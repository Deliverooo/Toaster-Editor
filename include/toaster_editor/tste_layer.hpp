#pragma once


#include "application.hpp"
#include "shader_library.hpp"

#include <toast_kernel/events/event.hpp>

namespace tste
{
	struct TSTELayer
	{
		ShaderLibrary shaderLib;
	};

	auto tste_init(Application &p_app, TSTELayer &p_layer) -> void;
	auto tste_destroy(Application &p_app, TSTELayer &p_layer) -> void;
	auto tste_on_update(Application &p_app, TSTELayer &p_layer, float32 p_dt) -> void;
	auto tste_on_render(Application &p_app, TSTELayer &p_layer, gpu::CommandListHandle p_cmd) -> void;
	auto tste_on_event(Application &p_app, TSTELayer &p_layer, knl::Event &p_event) -> void;
}
