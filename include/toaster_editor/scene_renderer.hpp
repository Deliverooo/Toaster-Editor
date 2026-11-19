#pragma once

#include "scene.hpp"

#include <toast_gpu/fwd.hpp>
#include <toast_math/math_vector.hpp>

#include "samplers.hpp"

namespace toaster::rnd
{
	struct MeshManager;
}

namespace tste
{
	struct ShaderLibrary;

	struct ObjectData
	{
		uint32 meshId;
		uint32 transformId;
	};

	struct DrawMeshTasksIndirectCountCommand
	{
		uint32 groupCountX{0u};
		uint32 groupCountY{0u};
		uint32 groupCountZ{0u};

		uint32 objectId{0u}; // Index into the object data buffer
	};

	struct OITFragment
	{
		uint32  colour;
		float32 depth;
	};

	#define OEX_OIT_LAYER_COUNT 2u

	struct OITPixel
	{
		uint32                                  count;
		Array<OITFragment, OEX_OIT_LAYER_COUNT> fragments;
	};

	constexpr uint32 c_maxDrawCalls{1028u * 1028u * 10u};

	struct SceneRenderer
	{
		TST_DEPENDS(Scene*, scene);
		TST_DEPENDS(ShaderLibrary *, shaderLib);
		TST_DEPENDS(Samplers*, samplers);

		enum class EAttachmentType : uint8 { eMSAADepth = 0u, eColour = 1u, eMax = 2u };

		TST_EARRAY(gpu::TextureHandle, EAttachmentType) attachments;

		PFFArray<gpu::BufferHandle> oitBuffers;
		uint64                      oitBufferSize{0u};
		PFFArray<gpu::BufferHandle> indirectBuffers;
		PFFArray<gpu::BufferHandle> objectDataBuffers;
		PFFArray<gpu::BufferHandle> countBuffers;

		uint32 colourAttachmentHeapSlot{UINT32_MAX};

		math::UInt2 renderAreaSize{0u, 0u};
	};

	struct SceneRendererDesc
	{
		math::UInt2 initialSize{1920u, 1080u};
	};

	auto scene_renderer_create(SceneRenderer &          p_out_scene_renderer, Scene &p_scene, ShaderLibrary &p_shader_lib, Samplers &p_samplers,
							   const SceneRendererDesc &p_desc) -> void;
	auto scene_renderer_destroy(SceneRenderer &p_scene_renderer) -> void;
	auto scene_renderer_on_resize(SceneRenderer &p_scene_renderer, math::UInt2 p_new_size) -> void;

	// Called before every other pass
	auto scene_renderer_setup_render_state(SceneRenderer &p_scene_renderer, gpu::CommandListHandle p_cmd) -> void;

	struct SkyboxPassExecutionInfo
	{
		gpu::TextureHandle targetColourAttachment;

		uintptr cameraBufferAddress;

		uint32                 frameIndex;
		gpu::CommandListHandle cmd;
	};

	auto scene_renderer_execute_skybox_pass(SceneRenderer &p_scene_renderer, const SkyboxPassExecutionInfo &p_execution_info) -> void;

	struct ObjectCullingPrePassExecutionInfo
	{
		uintptr cameraBufferAddress;
		uintptr transformBufferAddress;

		const rnd::MeshManager *meshManager;

		uint32                 frameIndex;
		gpu::CommandListHandle cmd;
	};

	// Only the number of objects that will be evaluated, not the number of remaining objects after the cull.
	// Use as the max draw count in gpu::drawMeshTasksIndirectCount.
	auto scene_renderer_execute_object_culling_pre_pass(const SceneRenderer &p_scene_renderer, const ObjectCullingPrePassExecutionInfo &p_execution_info,
														uint32 &             p_out_active_object_count) -> void;

	struct DepthPrePassExecutionInfo
	{
		uintptr cameraBufferAddress;
		uintptr transformBufferAddress;
		uintptr materialBufferAddress;
		uintptr meshMetadataBufferAddress;
		uintptr materialIndirectionBufferAddress;

		uint32                 frameIndex;
		gpu::CommandListHandle cmd;

		uint32 activeObjectCount; // Directly from executeObjectCullingPrePass
	};

	auto scene_renderer_execute_depth_pre_pass(SceneRenderer &p_scene_renderer, const DepthPrePassExecutionInfo &p_execution_info) -> void;

	struct GeometryPassExecutionInfo
	{
		gpu::TextureHandle targetColourAttachment;

		uintptr cameraBufferAddress;
		uintptr transformBufferAddress;
		uintptr materialBufferAddress;
		uintptr meshMetadataBufferAddress;
		uintptr materialIndirectionBufferAddress;
		uintptr pointLightBufferAddress;

		uint32                 frameIndex;
		gpu::CommandListHandle cmd;

		uint32 activeObjectCount; // Directly from executeObjectCullingPrePass
		uint32 activePointLightCount;
	};

	auto scene_renderer_execute_opaque_geometry_pass(SceneRenderer &p_scene_renderer, const GeometryPassExecutionInfo &p_execution_info) -> void;

	struct TransparentGeometryPassExecutionInfo
	{
		uintptr cameraBufferAddress;
		uintptr transformBufferAddress;
		uintptr materialBufferAddress;
		uintptr meshMetadataBufferAddress;
		uintptr materialIndirectionBufferAddress;
		uintptr pointLightBufferAddress;

		uint32                 frameIndex;
		gpu::CommandListHandle cmd;

		uint32 activeObjectCount; // Directly from executeObjectCullingPrePass
		uint32 activePointLightCount;
	};

	auto scene_renderer_execute_transparent_geometry_pass(SceneRenderer &p_scene_renderer, const TransparentGeometryPassExecutionInfo &p_execution_info) -> void;

	struct OITResolvePassExecutionInfo
	{
		uint32                 frameIndex;
		gpu::CommandListHandle cmd;
	};

	auto scene_renderer_execute_OIT_resolve_pass(SceneRenderer &p_scene_renderer, const OITResolvePassExecutionInfo &p_execution_info) -> void;

	struct CompositePassExecutionInfo
	{
		gpu::TextureHandle     targetColourAttachment;
		uint32                 frameIndex;
		gpu::CommandListHandle cmd;
	};

	auto scene_renderer_execute_composite_pass(SceneRenderer &p_scene_renderer, const CompositePassExecutionInfo &p_execution_info) -> void;
}
