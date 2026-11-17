#pragma once

#include "tste_common.hpp"

#include <entt/entt.hpp>

namespace tste
{
	using Registry = entt::basic_registry<entt::entity, STLAllocator<entt::entity> >;

	struct Scene
	{
		String   name;
		Registry registry;
	};

	auto scene_create(Scene &p_out_scene, StringView p_name = "New_Scene") -> void;
	auto scene_destroy(Scene &p_scene) -> void;
}
