#pragma once

#include "profiling.hpp"
#include <toast_lib/core_basic.hpp>

using namespace toaster;

namespace tste
{
	constexpr uint32 c_maxFramesInFlight{3u};

	// Better than a vector if we already know the size
	template<typename Type>
	using PFFArray = Array<Type, c_maxFramesInFlight>;
}
