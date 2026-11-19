#pragma once

#include "environment.hpp"

#include <entt/entt.hpp>

#include "light_environment.hpp"

namespace tste
{
	using Registry = entt::basic_registry<entt::entity, STLAllocator<entt::entity> >;

	struct EnvironmentContext;
	TST_DECLARE_HANDLE(Environment);

	struct Scene
	{
		TST_DEPENDS(EnvironmentContext *, environmentCtx);

		String   name;
		Registry registry;

		LightEnvironment  lightEnvironment;
		EnvironmentHandle currentEnvironment;
	};

	auto scene_create(Scene &p_out_scene, EnvironmentContext &p_environment_ctx, StringView p_name = "New_Scene") -> void;
	auto scene_destroy(Scene &p_scene) -> void;

	auto scene_set_environment(Scene &p_scene, EnvironmentHandle p_environment) -> void;
}
