#include "toaster_editor/scene.hpp"

namespace tste
{
	auto scene_create(Scene &p_out_scene, EnvironmentContext &p_environment_ctx, StringView p_name) -> void
	{
		p_out_scene.environmentCtx = &p_environment_ctx;

		p_out_scene.name               = p_name;
		p_out_scene.currentEnvironment = p_environment_ctx.defaultEnvironment;

		light_env_create(p_out_scene.lightEnvironment, p_environment_ctx.gpuCtx);
	}

	auto scene_destroy(Scene &p_scene) -> void
	{
		if (p_scene.currentEnvironment.valid() && p_scene.currentEnvironment != p_scene.environmentCtx->defaultEnvironment)
			environment_destroy(*p_scene.environmentCtx, p_scene.currentEnvironment);

		light_env_destroy(p_scene.lightEnvironment, p_scene.environmentCtx->gpuCtx);
	}

	auto scene_set_environment(Scene &p_scene, EnvironmentHandle p_environment) -> void
	{
		p_scene.currentEnvironment = p_environment;
	}
}
