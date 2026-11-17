#include "toaster_editor/scene.hpp"

namespace tste
{
	auto scene_create(Scene &p_out_scene, StringView p_name) -> void
	{
		p_out_scene.name = p_name;
	}

	auto scene_destroy(Scene &p_scene) -> void
	{
	}
}
