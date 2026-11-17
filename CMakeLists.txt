cmake_minimum_required(VERSION 3.31)

set(CMAKE_DEBUG_POSTFIX "d")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

project(ToasterEditor VERSION 1.0.0)

set(CXX_FLAGS)

if (MSVC)
	set(CXX_FLAGS
			"/W3 /O2 /Oi /Ot /Gy /Gw"
	)
	add_compile_definitions(_HAS_EXCEPTIONS=0)
endif ()

set(CMAKE_CXX_FLAGS ${CXX_FLAGS})

# We need glslangValidator for shader compilation
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)

# We need Toaster...
find_package(Toaster CONFIG REQUIRED)

set(SRC
		src/main.cpp
		src/tste_layer.cpp
		src/scene.cpp
		src/shader_library.cpp
)

add_executable(toaster_editor WIN32 ${SRC})

target_compile_options(toaster_editor PRIVATE
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
		/O2
		/Oi
		/fp:fast
		/arch:AVX2
		/experimental:c11atomics
		>
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:
		/Od
		/experimental:c11atomics
		>
)

target_include_directories(toaster_editor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(IMGUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
		imgui
		URL https://github.com/Deliverooo/imgui/archive/refs/heads/docking.zip
)

set(ENTT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
		entt
		URL https://github.com/skypjack/entt/archive/refs/heads/main.zip
)

set(JPH_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
		jolt
		URL https://github.com/jrouwe/JoltPhysics/archive/refs/heads/master.zip
		SOURCE_SUBDIR "Build"
)

set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
		ktx
		GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
		GIT_TAG main
)

FetchContent_Declare(
		fastgltf
		URL https://github.com/spnda/fastgltf/archive/refs/tags/v1.0.1.zip
)

set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
FetchContent_Declare(
		tracy
		URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.14.1.zip
)

set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
		meshopt
		URL https://github.com/zeux/meshoptimizer/archive/refs/heads/master.zip
)

FetchContent_MakeAvailable(imgui)
FetchContent_MakeAvailable(entt)
FetchContent_MakeAvailable(jolt)
FetchContent_MakeAvailable(ktx)
FetchContent_MakeAvailable(fastgltf)
FetchContent_MakeAvailable(tracy)
FetchContent_MakeAvailable(meshopt)

target_link_libraries(toaster_editor PRIVATE
		Toaster::ToastKernel
		Toaster::ToastOS
		fastgltf::fastgltf
		EnTT::EnTT
		glfw
		ImGui
		Tracy::TracyClient
)

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders)
set(SHADER_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shader_headers)
file(MAKE_DIRECTORY ${SHADER_GEN_DIR})

set(SHADER_SOURCES_GLSL
		"${SHADER_SRC_DIR}/object_culling.comp"
		"${SHADER_SRC_DIR}/meshlet_culling.task"
		"${SHADER_SRC_DIR}/depth_pre.mesh"

		"${SHADER_SRC_DIR}/tst_pbr_static.mesh"
		"${SHADER_SRC_DIR}/tst_pbr_static.frag"
		"${SHADER_SRC_DIR}/tst_pbr_static_transparent.frag"

		"${SHADER_SRC_DIR}/skybox.mesh"
		"${SHADER_SRC_DIR}/skybox.frag"

		"${SHADER_SRC_DIR}/composite.mesh"
		"${SHADER_SRC_DIR}/composite.frag"

		"${SHADER_SRC_DIR}/equirectangular_to_cubemap.comp"
		"${SHADER_SRC_DIR}/diffuse_irradiance_convolution.comp"

		"${SHADER_SRC_DIR}/fullscreen_quad.mesh"
		"${SHADER_SRC_DIR}/oit_resolve.frag"
)

set(GENERATED_HEADERS "")
foreach (SHADER_PATH IN LISTS SHADER_SOURCES_GLSL)

	get_filename_component(SHADER_NAME ${SHADER_PATH} NAME)

	set(HEADER_PATH "${SHADER_GEN_DIR}/${SHADER_NAME}.h")
	list(APPEND GENERATED_HEADERS ${HEADER_PATH})

	string(REPLACE "." "_" VARIABLE_NAME "${SHADER_NAME}")

	add_custom_command(
			OUTPUT ${HEADER_PATH}
			COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V --target-env vulkan1.4 --vn "c_${VARIABLE_NAME}_bytecode" -o ${HEADER_PATH} ${SHADER_PATH}
			DEPENDS ${SHADER_PATH} "${SHADER_SRC_DIR}/common.glsl"
			COMMENT "Compiling shader: ${SHADER_NAME}"
			VERBATIM
	)
endforeach ()

add_custom_target(tste_shaders DEPENDS ${GENERATED_HEADERS})

add_dependencies(toaster_editor tste_shaders)
target_include_directories(toaster_editor PRIVATE ${SHADER_GEN_DIR})
