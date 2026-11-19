#include "toaster_editor/scene_renderer.hpp"
#include "toaster_editor/shader_library.hpp"

#include <toast_gpu/upload.hpp>

#include <toast_lib/format.hpp>
#include <toast_render/mesh.hpp>

#include "toaster_editor/components.hpp"

namespace tste
{
	auto scene_renderer_create(SceneRenderer &          p_out_scene_renderer, Scene &p_scene, ShaderLibrary &p_shader_lib, Samplers &p_samplers,
							   const SceneRendererDesc &p_desc) -> void
	{
		p_out_scene_renderer.scene          = &p_scene;
		p_out_scene_renderer.shaderLib      = &p_shader_lib;
		p_out_scene_renderer.samplers       = &p_samplers;
		p_out_scene_renderer.renderAreaSize = p_desc.initialSize;
		p_out_scene_renderer.oitBufferSize  = sizeof(OITPixel) * static_cast<uint64>(p_desc.initialSize.x) * static_cast<uint64>(p_desc.initialSize.y);

		gpu::GPUContext *gpu_ctx{p_out_scene_renderer.scene->environmentCtx->gpuCtx};

		constexpr gpu::BufferDesc indirect_buffer_desc{
			.size = sizeof(DrawMeshTasksIndirectCountCommand) * c_maxDrawCalls,
			.usage = gpu::EBufferUsageFlagBits::eIndirectBuffer | gpu::EBufferUsageFlagBits::eStorageBuffer,
			.memoryType = gpu::EMemoryType::eDeviceLocal
		};

		constexpr gpu::BufferDesc object_buffer_desc{
			.size = sizeof(ObjectData) * c_maxDrawCalls,
			.usage = gpu::EBufferUsageFlagBits::eStorageBuffer,
			.memoryType = gpu::EMemoryType::eHostVisibleCoherent
		};

		constexpr gpu::BufferDesc count_buffer_desc{
			.size = sizeof(uint32),
			.usage = gpu::EBufferUsageFlagBits::eIndirectBuffer | gpu::EBufferUsageFlagBits::eStorageBuffer | gpu::EBufferUsageFlagBits::eTransferDst,
			.memoryType = gpu::EMemoryType::eDeviceLocal
		};

		gpu::BufferDesc oit_buffer_desc{};
		oit_buffer_desc.size       = p_out_scene_renderer.oitBufferSize;
		oit_buffer_desc.usage      = gpu::EBufferUsageFlagBits::eStorageBuffer | gpu::EBufferUsageFlagBits::eTransferDst;
		oit_buffer_desc.memoryType = gpu::EMemoryType::eDeviceLocal;

		// Create all the required buffers using batch creation functions
		gpu::buffer_create_n(gpu_ctx, indirect_buffer_desc, p_out_scene_renderer.indirectBuffers);
		gpu::buffer_create_n(gpu_ctx, object_buffer_desc, p_out_scene_renderer.objectDataBuffers);
		gpu::buffer_create_n(gpu_ctx, count_buffer_desc, p_out_scene_renderer.countBuffers);
		gpu::buffer_create_n(gpu_ctx, oit_buffer_desc, p_out_scene_renderer.oitBuffers);

		EArray<gpu::TextureDesc, SceneRenderer::EAttachmentType, SceneRenderer::EAttachmentType::eMax> attachment_descs;

		gpu::TextureDesc &msaa_depth_desc{attachment_descs[SceneRenderer::EAttachmentType::eMSAADepth]};
		msaa_depth_desc.extent      = {p_desc.initialSize.x, p_desc.initialSize.y, 1u};
		msaa_depth_desc.format      = gpu::EFormat::eD32Sfloat;
		msaa_depth_desc.usage       = gpu::ETextureUsageFlagBits::eDepthStencilAttachment | gpu::ETextureUsageFlagBits::eTransient;
		msaa_depth_desc.sampleCount = gpu::ESampleCount::e1;

		gpu::TextureDesc &msaa_colour_desc{attachment_descs[SceneRenderer::EAttachmentType::eColour]};
		msaa_colour_desc.extent      = {p_desc.initialSize.x, p_desc.initialSize.y, 1u};
		msaa_colour_desc.format      = gpu::EFormat::eR8G8B8A8Srgb;
		msaa_colour_desc.usage       = gpu::ETextureUsageFlagBits::eColourAttachment | gpu::ETextureUsageFlagBits::eSampled;
		msaa_colour_desc.sampleCount = gpu::ESampleCount::e1;

		gpu::texture_create_n(gpu_ctx, attachment_descs, p_out_scene_renderer.attachments);

		p_out_scene_renderer.colourAttachmentHeapSlot = gpu::resource_descriptor_heap_alloc_texture_slot(gpu_ctx, p_scene.environmentCtx->resourceHeap);
		gpu::resource_descriptor_heap_write_texture(gpu_ctx, p_scene.environmentCtx->resourceHeap, p_out_scene_renderer.colourAttachmentHeapSlot,
													p_out_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour], false);
	}

	auto scene_renderer_destroy(SceneRenderer &p_scene_renderer) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		gpu::texture_destroy_n(gpu_ctx, p_scene_renderer.attachments);

		gpu::resource_descriptor_heap_free_texture_slot(gpu_ctx, p_scene_renderer.scene->environmentCtx->resourceHeap, p_scene_renderer.colourAttachmentHeapSlot);

		gpu::buffer_destroy_n(gpu_ctx, p_scene_renderer.oitBuffers);
		gpu::buffer_destroy_n(gpu_ctx, p_scene_renderer.countBuffers);
		gpu::buffer_destroy_n(gpu_ctx, p_scene_renderer.objectDataBuffers);
		gpu::buffer_destroy_n(gpu_ctx, p_scene_renderer.indirectBuffers);
	}

	auto scene_renderer_on_resize(SceneRenderer &p_scene_renderer, math::UInt2 p_new_size) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		p_scene_renderer.renderAreaSize = p_new_size;

		const uint64 oit_buffer_size{sizeof(OITPixel) * static_cast<uint64>(p_new_size.x) * static_cast<uint64>(p_new_size.y)};

		if (oit_buffer_size > p_scene_renderer.oitBufferSize)
		{
			p_scene_renderer.oitBufferSize = oit_buffer_size;

			gpu::buffer_destroy_n(gpu_ctx, p_scene_renderer.oitBuffers);

			gpu::BufferDesc oit_buffer_desc{};
			oit_buffer_desc.size       = p_scene_renderer.oitBufferSize;
			oit_buffer_desc.usage      = gpu::EBufferUsageFlagBits::eStorageBuffer | gpu::EBufferUsageFlagBits::eTransferDst;
			oit_buffer_desc.memoryType = gpu::EMemoryType::eDeviceLocal;

			gpu::buffer_create_n(gpu_ctx, oit_buffer_desc, p_scene_renderer.oitBuffers);
		}

		gpu::texture_destroy_n(gpu_ctx, p_scene_renderer.attachments);

		EArray<gpu::TextureDesc, SceneRenderer::EAttachmentType, SceneRenderer::EAttachmentType::eMax> attachment_descs;

		gpu::TextureDesc &msaa_depth_desc{attachment_descs[SceneRenderer::EAttachmentType::eMSAADepth]};
		msaa_depth_desc.extent      = {p_new_size.x, p_new_size.y, 1u};
		msaa_depth_desc.format      = gpu::EFormat::eD32Sfloat;
		msaa_depth_desc.usage       = gpu::ETextureUsageFlagBits::eDepthStencilAttachment | gpu::ETextureUsageFlagBits::eTransient;
		msaa_depth_desc.sampleCount = gpu::ESampleCount::e1;

		gpu::TextureDesc &msaa_colour_desc{attachment_descs[SceneRenderer::EAttachmentType::eColour]};
		msaa_colour_desc.extent      = {p_new_size.x, p_new_size.y, 1u};
		msaa_colour_desc.format      = gpu::EFormat::eR8G8B8A8Srgb;
		msaa_colour_desc.usage       = gpu::ETextureUsageFlagBits::eColourAttachment | gpu::ETextureUsageFlagBits::eSampled;
		msaa_colour_desc.sampleCount = gpu::ESampleCount::e1;

		gpu::texture_create_n(gpu_ctx, attachment_descs, p_scene_renderer.attachments);

		gpu::resource_descriptor_heap_write_texture(gpu_ctx, p_scene_renderer.scene->environmentCtx->resourceHeap, p_scene_renderer.colourAttachmentHeapSlot,
													p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour], false);
	}

	auto scene_renderer_setup_render_state(SceneRenderer &p_scene_renderer, gpu::CommandListHandle p_cmd) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		gpu::cmd_set_primitive_topology(gpu_ctx, p_cmd, gpu::EPrimitiveTopology::eTriangleList);
		gpu::cmd_set_primitive_restart(gpu_ctx, p_cmd, false);

		gpu::cmd_set_viewport(gpu_ctx, p_cmd, math::Viewport{p_scene_renderer.renderAreaSize});
		gpu::cmd_set_scissor(gpu_ctx, p_cmd, p_scene_renderer.renderAreaSize);

		gpu::cmd_set_rasterizer_discard_enable(gpu_ctx, p_cmd, false);
		gpu::cmd_set_polygon_mode(gpu_ctx, p_cmd, gpu::EPolygonMode::eFill);
		gpu::cmd_set_cull_mode(gpu_ctx, p_cmd, gpu::ECullMode::eNone);
		gpu::cmd_set_front_face(gpu_ctx, p_cmd, gpu::EFrontFace::eCCW);
		gpu::cmd_set_depth_bias(gpu_ctx, p_cmd, false);
		gpu::cmd_set_line_width(gpu_ctx, p_cmd, 1.0f);

		// I will probably never use a stencil buffer...
		gpu::cmd_set_stencil_state(gpu_ctx, p_cmd, false);

		gpu::cmd_set_colour_write_mask(gpu_ctx, p_cmd, {gpu::EColourComponentFlagBits::eAll});
	}

	auto scene_renderer_execute_skybox_pass(SceneRenderer &p_scene_renderer, const SkyboxPassExecutionInfo &p_execution_info) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		gpu::RenderingInfo rendering_info{};
		rendering_info.colourAttachments = {
			gpu::RenderingAttachmentInfo{
				gpu::ClearColourValue{1.0f, 0.0f, 1.0f, 1.0f},
				p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour],
				nullptr,
				gpu::EAttachmentUsageOP::eClearStore,
				gpu::EAttachmentResolveMode::eNone
			}
		};
		rendering_info.renderArea = math::Rect{p_scene_renderer.renderAreaSize};

		gpu::cmd_begin_rendering(gpu_ctx, p_execution_info.cmd, rendering_info);

		gpu::cmd_bind_shaders(gpu_ctx, p_execution_info.cmd, p_scene_renderer.shaderLib->skyboxShaders);

		gpu::cmd_set_primitive_topology(gpu_ctx, p_execution_info.cmd, gpu::EPrimitiveTopology::eTriangleList);
		gpu::cmd_set_primitive_restart(gpu_ctx, p_execution_info.cmd, false);

		gpu::cmd_set_viewport(gpu_ctx, p_execution_info.cmd, math::Viewport{rendering_info.renderArea});
		gpu::cmd_set_scissor(gpu_ctx, p_execution_info.cmd, rendering_info.renderArea);

		gpu::cmd_set_rasterizer_discard_enable(gpu_ctx, p_execution_info.cmd, false);
		gpu::cmd_set_polygon_mode(gpu_ctx, p_execution_info.cmd, gpu::EPolygonMode::eFill);
		gpu::cmd_set_cull_mode(gpu_ctx, p_execution_info.cmd, gpu::ECullMode::eNone);
		gpu::cmd_set_front_face(gpu_ctx, p_execution_info.cmd, gpu::EFrontFace::eCCW);
		gpu::cmd_set_depth_bias(gpu_ctx, p_execution_info.cmd, false);
		gpu::cmd_set_line_width(gpu_ctx, p_execution_info.cmd, 1.0f);

		gpu::cmd_set_rasterization_samples(gpu_ctx, p_execution_info.cmd, gpu::ESampleCount::e1);
		gpu::cmd_set_colour_blend_enable(gpu_ctx, p_execution_info.cmd, {false});
		gpu::cmd_set_depth_state(gpu_ctx, p_execution_info.cmd, false, false);
		gpu::cmd_set_colour_write_enable(gpu_ctx, p_execution_info.cmd, {true});

		const Environment &environment{p_scene_renderer.scene->environmentCtx->environments[p_scene_renderer.scene->currentEnvironment]};
		struct PushData
		{
			uintptr cameraBuffer;
			uint32  samplerId;
			uint32  skyboxMap;
		};
		PushData push_data{};
		push_data.cameraBuffer = p_execution_info.cameraBufferAddress;
		push_data.samplerId    = p_scene_renderer.samplers->heapSlots[ESamplerType::eTextureSampler];
		push_data.skyboxMap    = environment.shaderReadHeapSlots[EEnvironmentMapType::eSkybox];

		gpu::cmd_push_data(gpu_ctx, p_execution_info.cmd, push_data);
		gpu::cmd_draw_mesh_tasks(gpu_ctx, p_execution_info.cmd, 1u, 1u, 1u);
		gpu::cmd_end_rendering(gpu_ctx, p_execution_info.cmd);
	}

	auto scene_renderer_execute_object_culling_pre_pass(const SceneRenderer &p_scene_renderer, const ObjectCullingPrePassExecutionInfo &p_execution_info,
														uint32 &             p_out_active_object_count) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		const uint32                 frame_index{p_execution_info.frameIndex};
		const gpu::CommandListHandle cmd{p_execution_info.cmd};

		auto &obj_data_buffer{p_scene_renderer.objectDataBuffers[frame_index]};
		auto &count_buffer{p_scene_renderer.countBuffers[frame_index]};
		auto &indirect_buffer{p_scene_renderer.indirectBuffers[frame_index]};

		p_out_active_object_count = 0u;

		auto       mapped_object_data{static_cast<ObjectData *>(gpu::buffer_get_mapped_data(gpu_ctx, obj_data_buffer))};
		const auto view{p_scene_renderer.scene->registry.view<StaticMeshComponent, GPUTransformComponent>()};
		view.each([p_execution_info, &p_out_active_object_count, mapped_object_data](const StaticMeshComponent &p_smc, const GPUTransformComponent &p_gpu_tc) -> void
		{
			const rnd::StaticMesh *mesh{rnd::static_mesh_try_get(p_execution_info.meshManager, p_smc.mesh)};
			TST_ASSERT(p_gpu_tc.transformId != UINT32_MAX);

			if (mesh && p_smc.visible /*&& gpu::state_tracker_is_ready(upload_ctx, mesh->stateTracker) */)
			{
				auto &obj_data{mapped_object_data[p_out_active_object_count]};
				obj_data.meshId      = p_smc.mesh.getId();
				obj_data.transformId = p_gpu_tc.transformId;
				++p_out_active_object_count;
			}
		});

		gpu::cmd_fill_buffer(gpu_ctx, cmd, count_buffer, 0u, sizeof(uint32), 0u); // Reset the count to 0

		struct PushData
		{
			uintptr cameraBuffer;
			uintptr objectDataBuffer;
			uintptr transformBuffer;
			uintptr meshMetadataBuffer;
			uintptr indirectBuffer;
			uintptr countBuffer;

			uint32 activeObjectCount;
		};

		PushData push_data{};
		push_data.cameraBuffer       = p_execution_info.cameraBufferAddress;
		push_data.objectDataBuffer   = gpu::buffer_get_address(gpu_ctx, obj_data_buffer);
		push_data.transformBuffer    = p_execution_info.transformBufferAddress;
		push_data.meshMetadataBuffer = rnd::mesh_mngr_get_mesh_metadata_buffer_address(p_execution_info.meshManager);
		push_data.indirectBuffer     = gpu::buffer_get_address(gpu_ctx, indirect_buffer);
		push_data.countBuffer        = gpu::buffer_get_address(gpu_ctx, count_buffer);
		push_data.activeObjectCount  = p_out_active_object_count;

		gpu::cmd_push_data(gpu_ctx, cmd, push_data);
		gpu::cmd_bind_shaders(gpu_ctx, cmd, p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eObjectCullingCS]);
		gpu::cmd_dispatch(gpu_ctx, cmd, gpu::distribute_workgroup_count(gpu_ctx, p_out_active_object_count), 1u, 1u);

		Array<gpu::BufferMemoryBarrier, 2u> buffer_memory_barriers;
		buffer_memory_barriers[0u].buffer        = indirect_buffer;
		buffer_memory_barriers[0u].size          = gpu::c_wholeSize;
		buffer_memory_barriers[0u].srcAccessMask = gpu::EAccessFlagBits::eShaderWrite;
		buffer_memory_barriers[0u].dstAccessMask = gpu::EAccessFlagBits::eIndirectCommandRead | gpu::EAccessFlagBits::eShaderRead;
		buffer_memory_barriers[0u].srcStageMask  = gpu::EPipelineStageFlagBits::eComputeShader;
		buffer_memory_barriers[0u].dstStageMask  = gpu::EPipelineStageFlagBits::eDrawIndirect | gpu::EPipelineStageFlagBits::eTaskShader;

		buffer_memory_barriers[1u].buffer        = count_buffer;
		buffer_memory_barriers[1u].size          = gpu::c_wholeSize;
		buffer_memory_barriers[1u].srcAccessMask = gpu::EAccessFlagBits::eShaderWrite;
		buffer_memory_barriers[1u].dstAccessMask = gpu::EAccessFlagBits::eIndirectCommandRead | gpu::EAccessFlagBits::eShaderRead;
		buffer_memory_barriers[1u].srcStageMask  = gpu::EPipelineStageFlagBits::eComputeShader;
		buffer_memory_barriers[1u].dstStageMask  = gpu::EPipelineStageFlagBits::eDrawIndirect | gpu::EPipelineStageFlagBits::eTaskShader;
		gpu::cmd_pipeline_barrier(gpu_ctx, cmd, buffer_memory_barriers, {});
	}

	auto scene_renderer_execute_depth_pre_pass(SceneRenderer &p_scene_renderer, const DepthPrePassExecutionInfo &p_execution_info) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		const uint32 frame_index{p_execution_info.frameIndex};

		gpu::RenderingInfo rendering_info{};
		rendering_info.depthAttachment = gpu::RenderingAttachmentInfo{
			gpu::ClearDepthStencilValue{},
			p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eMSAADepth],
			nullptr,
			gpu::EAttachmentUsageOP::eClearStore,
			gpu::EAttachmentResolveMode::eNone
		};
		rendering_info.renderArea = math::Rect{p_scene_renderer.renderAreaSize};

		gpu::cmd_begin_rendering(gpu_ctx, p_execution_info.cmd, rendering_info);
		gpu::cmd_bind_shaders(gpu_ctx, p_execution_info.cmd, {
								  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshTS],
								  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eDepthPreMS]
							  });

		gpu::cmd_set_colour_blend_enable(gpu_ctx, p_execution_info.cmd, {false});
		gpu::cmd_set_rasterization_samples(gpu_ctx, p_execution_info.cmd, gpu::ESampleCount::e1);
		gpu::cmd_set_depth_state(gpu_ctx, p_execution_info.cmd, true);

		gpu::cmd_set_colour_write_enable(gpu_ctx, p_execution_info.cmd, {false});

		struct PushData
		{
			uintptr cameraBuffer;
			uintptr objectDataBuffer;
			uintptr materialBuffer;
			uintptr transformBuffer;
			uintptr meshMetadataBuffer;
			uintptr materialIndirectionBuffer;

			uintptr pointLightBuffer;
			uintptr indirectBuffer;

			uintptr      oitStorage;
			math::Float2 screenResolution;

			uint32 skyboxId;
			uint32 samplerId;

			uint32 isTransparentPass;
			uint32 activePointLightCount;
		};

		PushData push_data{};
		push_data.cameraBuffer              = p_execution_info.cameraBufferAddress;
		push_data.objectDataBuffer          = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.objectDataBuffers[frame_index]);
		push_data.materialBuffer            = p_execution_info.materialBufferAddress;
		push_data.transformBuffer           = p_execution_info.transformBufferAddress;
		push_data.meshMetadataBuffer        = p_execution_info.meshMetadataBufferAddress;
		push_data.materialIndirectionBuffer = p_execution_info.materialIndirectionBufferAddress;
		push_data.indirectBuffer            = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.indirectBuffers[frame_index]);
		push_data.isTransparentPass         = false;

		gpu::cmd_push_data(gpu_ctx, p_execution_info.cmd, push_data);

		if (p_execution_info.activeObjectCount > 0u)
			gpu::cmd_draw_mesh_tasks_indirect_count(gpu_ctx, p_execution_info.cmd, p_scene_renderer.indirectBuffers[frame_index], 0u,
													p_scene_renderer.countBuffers[frame_index], 0u, p_execution_info.activeObjectCount,
													sizeof(DrawMeshTasksIndirectCountCommand));

		gpu::cmd_end_rendering(gpu_ctx, p_execution_info.cmd);

		gpu::TextureMemoryBarrier depth_sync_texture_barrier{};
		depth_sync_texture_barrier.texture       = p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eMSAADepth];
		depth_sync_texture_barrier.srcAccessMask = gpu::EAccessFlagBits::eDepthStencilAttachmentWrite;
		depth_sync_texture_barrier.dstAccessMask = gpu::EAccessFlagBits::eDepthStencilAttachmentRead;
		depth_sync_texture_barrier.srcStageMask  = gpu::EPipelineStageFlagBits::eColorAttachmentOutput | gpu::EPipelineStageFlagBits::eEarlyFragmentTests;
		depth_sync_texture_barrier.dstStageMask  = gpu::EPipelineStageFlagBits::eAllCommands;
		gpu::cmd_pipeline_barrier(gpu_ctx, p_execution_info.cmd, {}, depth_sync_texture_barrier);
	}

	auto scene_renderer_execute_opaque_geometry_pass(SceneRenderer &p_scene_renderer, const GeometryPassExecutionInfo &p_execution_info) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		const uint32 frame_index{p_execution_info.frameIndex};

		gpu::RenderingInfo rendering_info{};
		rendering_info.colourAttachments = {
			gpu::RenderingAttachmentInfo{
				gpu::ClearColourValue{1.0f, 0.0f, 1.0f, 1.0f},
				p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour],
				nullptr,
				gpu::EAttachmentUsageOP::eLoadStore,
				gpu::EAttachmentResolveMode::eNone
			}
		};
		rendering_info.depthAttachment = gpu::RenderingAttachmentInfo{
			gpu::ClearDepthStencilValue{},
			p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eMSAADepth],
			nullptr,
			gpu::EAttachmentUsageOP::eLoadStore,
			gpu::EAttachmentResolveMode::eNone
		};
		rendering_info.renderArea = math::Rect{p_scene_renderer.renderAreaSize};

		gpu::cmd_begin_rendering(gpu_ctx, p_execution_info.cmd, rendering_info);
		gpu::cmd_bind_shaders(gpu_ctx, p_execution_info.cmd, {
								  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshTS],
								  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshMS],
								  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshOpaquePS]
							  });

		gpu::cmd_set_colour_blend_enable(gpu_ctx, p_execution_info.cmd, {false});
		gpu::cmd_set_rasterization_samples(gpu_ctx, p_execution_info.cmd, gpu::ESampleCount::e1);
		gpu::cmd_set_depth_state(gpu_ctx, p_execution_info.cmd, true, false);
		gpu::cmd_set_colour_write_enable(gpu_ctx, p_execution_info.cmd, {true});

		const Environment &environment{p_scene_renderer.scene->environmentCtx->environments[p_scene_renderer.scene->currentEnvironment]};

		struct PushData
		{
			uintptr cameraBuffer;
			uintptr objectDataBuffer;
			uintptr materialBuffer;
			uintptr transformBuffer;
			uintptr meshMetadataBuffer;
			uintptr materialIndirectionBuffer;
			uintptr pointLightBuffer;
			uintptr indirectBuffer;

			uintptr      oitStorage;
			math::Float2 screenResolution;

			uint32 diffuseIrradianceMapHeapSlot;
			uint32 samplerId;

			uint32 isTransparentPass;
			uint32 activePointLightCount;
		};
		PushData push_data{};
		push_data.cameraBuffer                 = p_execution_info.cameraBufferAddress;
		push_data.objectDataBuffer             = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.objectDataBuffers[frame_index]);
		push_data.materialBuffer               = p_execution_info.materialBufferAddress;
		push_data.transformBuffer              = p_execution_info.transformBufferAddress;
		push_data.meshMetadataBuffer           = p_execution_info.meshMetadataBufferAddress;
		push_data.materialIndirectionBuffer    = p_execution_info.materialIndirectionBufferAddress;
		push_data.pointLightBuffer             = p_execution_info.pointLightBufferAddress;
		push_data.indirectBuffer               = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.indirectBuffers[frame_index]);
		push_data.diffuseIrradianceMapHeapSlot = environment.shaderReadHeapSlots[EEnvironmentMapType::eDiffuseIrradiance];
		push_data.samplerId                    = p_scene_renderer.samplers->heapSlots[ESamplerType::eTextureSampler];
		push_data.isTransparentPass            = false;
		push_data.activePointLightCount        = p_execution_info.activePointLightCount;
		push_data.screenResolution             = {static_cast<float32>(p_scene_renderer.renderAreaSize.x), static_cast<float32>(p_scene_renderer.renderAreaSize.y)};

		gpu::cmd_push_data(gpu_ctx, p_execution_info.cmd, push_data);

		if (p_execution_info.activeObjectCount > 0u)
			gpu::cmd_draw_mesh_tasks_indirect_count(gpu_ctx, p_execution_info.cmd, p_scene_renderer.indirectBuffers[frame_index], 0u,
													p_scene_renderer.countBuffers[frame_index], 0u, p_execution_info.activeObjectCount,
													sizeof(DrawMeshTasksIndirectCountCommand));

		gpu::cmd_end_rendering(gpu_ctx, p_execution_info.cmd);
	}

	auto scene_renderer_execute_transparent_geometry_pass(SceneRenderer &p_scene_renderer, const TransparentGeometryPassExecutionInfo &p_execution_info) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		const uint32 frame_index{p_execution_info.frameIndex};

		gpu::cmd_fill_buffer(gpu_ctx, p_execution_info.cmd, p_scene_renderer.oitBuffers[frame_index], 0u, p_scene_renderer.oitBufferSize, 0u);

		if (p_execution_info.activeObjectCount > 0u)
		{
			gpu::BufferMemoryBarrier buffer_memory_barrier{};
			buffer_memory_barrier.buffer        = p_scene_renderer.oitBuffers[frame_index];
			buffer_memory_barrier.size          = p_scene_renderer.oitBufferSize;
			buffer_memory_barrier.srcAccessMask = gpu::EAccessFlagBits::eTransferWrite;
			buffer_memory_barrier.dstAccessMask = gpu::EAccessFlagBits::eShaderWrite | gpu::EAccessFlagBits::eShaderRead;
			buffer_memory_barrier.srcStageMask  = gpu::EPipelineStageFlagBits::eTransfer | gpu::EPipelineStageFlagBits::eClear;
			buffer_memory_barrier.dstStageMask  = gpu::EPipelineStageFlagBits::eFragmentShader;

			gpu::cmd_pipeline_barrier(gpu_ctx, p_execution_info.cmd, buffer_memory_barrier, {});

			gpu::RenderingInfo rendering_info{};
			rendering_info.depthAttachment = gpu::RenderingAttachmentInfo{
				gpu::ClearDepthStencilValue{},
				p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eMSAADepth],
				nullptr,
				gpu::EAttachmentUsageOP::eLoadStore,
				gpu::EAttachmentResolveMode::eNone
			};
			rendering_info.renderArea = math::Rect{p_scene_renderer.renderAreaSize};

			gpu::cmd_begin_rendering(gpu_ctx, p_execution_info.cmd, rendering_info);
			gpu::cmd_bind_shaders(gpu_ctx, p_execution_info.cmd, {
									  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshTS],
									  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshMS],
									  p_scene_renderer.shaderLib->shaders[EUnlinkedShaderType::eStaticMeshTransparentPS]
								  });

			gpu::cmd_set_colour_blend_enable(gpu_ctx, p_execution_info.cmd, {false});
			gpu::cmd_set_rasterization_samples(gpu_ctx, p_execution_info.cmd, gpu::ESampleCount::e1);
			gpu::cmd_set_depth_state(gpu_ctx, p_execution_info.cmd, true, false);
			gpu::cmd_set_colour_write_enable(gpu_ctx, p_execution_info.cmd, {false});

			const Environment &environment{p_scene_renderer.scene->environmentCtx->environments[p_scene_renderer.scene->currentEnvironment]};

			struct PushData
			{
				uintptr cameraBuffer;
				uintptr objectDataBuffer;
				uintptr materialBuffer;
				uintptr transformBuffer;
				uintptr meshMetadataBuffer;
				uintptr materialIndirectionBuffer;
				uintptr pointLightBuffer;
				uintptr indirectBuffer;

				uintptr      oitStorage;
				math::Float2 screenResolution;

				uint32 diffuseIrradianceMapHeapSlot;
				uint32 samplerId;

				uint32 isTransparentPass;
				uint32 activePointLightCount;
			};
			PushData push_data{};
			push_data.cameraBuffer                 = p_execution_info.cameraBufferAddress;
			push_data.objectDataBuffer             = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.objectDataBuffers[frame_index]);
			push_data.materialBuffer               = p_execution_info.materialBufferAddress;
			push_data.transformBuffer              = p_execution_info.transformBufferAddress;
			push_data.meshMetadataBuffer           = p_execution_info.meshMetadataBufferAddress;
			push_data.materialIndirectionBuffer    = p_execution_info.materialIndirectionBufferAddress;
			push_data.pointLightBuffer             = p_execution_info.pointLightBufferAddress;
			push_data.indirectBuffer               = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.indirectBuffers[frame_index]);
			push_data.diffuseIrradianceMapHeapSlot = environment.shaderReadHeapSlots[EEnvironmentMapType::eDiffuseIrradiance];
			push_data.samplerId                    = p_scene_renderer.samplers->heapSlots[ESamplerType::eTextureSampler];
			push_data.isTransparentPass            = true;
			push_data.activePointLightCount        = p_execution_info.activePointLightCount;
			push_data.oitStorage                   = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.oitBuffers[frame_index]);
			push_data.screenResolution             = {static_cast<float32>(p_scene_renderer.renderAreaSize.x), static_cast<float32>(p_scene_renderer.renderAreaSize.y)};

			gpu::cmd_push_data(gpu_ctx, p_execution_info.cmd, push_data);

			gpu::cmd_draw_mesh_tasks_indirect_count(gpu_ctx, p_execution_info.cmd, p_scene_renderer.indirectBuffers[frame_index], 0u,
													p_scene_renderer.countBuffers[frame_index], 0u, p_execution_info.activeObjectCount,
													sizeof(DrawMeshTasksIndirectCountCommand));

			gpu::cmd_end_rendering(gpu_ctx, p_execution_info.cmd);
		}
	}

	auto scene_renderer_execute_OIT_resolve_pass(SceneRenderer &p_scene_renderer, const OITResolvePassExecutionInfo &p_execution_info) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		const uint32 frame_index{p_execution_info.frameIndex};

		gpu::BufferMemoryBarrier buffer_memory_barrier{};
		buffer_memory_barrier.buffer        = p_scene_renderer.oitBuffers[frame_index];
		buffer_memory_barrier.size          = p_scene_renderer.oitBufferSize;
		buffer_memory_barrier.srcAccessMask = gpu::EAccessFlagBits::eShaderWrite;
		buffer_memory_barrier.dstAccessMask = gpu::EAccessFlagBits::eShaderRead;
		buffer_memory_barrier.srcStageMask  = gpu::EPipelineStageFlagBits::eFragmentShader;
		buffer_memory_barrier.dstStageMask  = gpu::EPipelineStageFlagBits::eFragmentShader;

		gpu::TextureMemoryBarrier texture_memory_barrier{};
		texture_memory_barrier.texture       = p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour];
		texture_memory_barrier.srcAccessMask = gpu::EAccessFlagBits::eColorAttachmentWrite;
		texture_memory_barrier.dstAccessMask = gpu::EAccessFlagBits::eShaderRead;
		texture_memory_barrier.srcStageMask  = gpu::EPipelineStageFlagBits::eColorAttachmentOutput;
		texture_memory_barrier.dstStageMask  = gpu::EPipelineStageFlagBits::eFragmentShader;
		gpu::cmd_pipeline_barrier(gpu_ctx, p_execution_info.cmd, buffer_memory_barrier, texture_memory_barrier);

		gpu::RenderingInfo rendering_info{};
		rendering_info.colourAttachments = {
			gpu::RenderingAttachmentInfo{
				gpu::ClearColourValue{1.0f, 0.0f, 1.0f, 1.0f},
				p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour],
				nullptr,
				gpu::EAttachmentUsageOP::eLoadStore,
				gpu::EAttachmentResolveMode::eNone
			}
		};
		rendering_info.renderArea = math::Rect{p_scene_renderer.renderAreaSize};

		gpu::cmd_begin_rendering(gpu_ctx, p_execution_info.cmd, rendering_info);
		gpu::cmd_bind_shaders(gpu_ctx, p_execution_info.cmd, p_scene_renderer.shaderLib->oitResolveShaders);

		gpu::cmd_set_colour_blend_enable(gpu_ctx, p_execution_info.cmd, {false});
		gpu::cmd_set_rasterization_samples(gpu_ctx, p_execution_info.cmd, gpu::ESampleCount::e1);
		gpu::cmd_set_depth_state(gpu_ctx, p_execution_info.cmd, false, false);
		gpu::cmd_set_colour_write_enable(gpu_ctx, p_execution_info.cmd, {true});

		struct PushData
		{
			uintptr      oitStorage;
			math::Float2 screenResolution;
			uint32       colourAttachmentHeapId;
			uint32       samplerHeapId;
		};

		PushData push_data{};
		push_data.oitStorage             = gpu::buffer_get_address(gpu_ctx, p_scene_renderer.oitBuffers[frame_index]);
		push_data.screenResolution       = {static_cast<float32>(p_scene_renderer.renderAreaSize.x), static_cast<float32>(p_scene_renderer.renderAreaSize.y)};
		push_data.colourAttachmentHeapId = p_scene_renderer.colourAttachmentHeapSlot;
		push_data.samplerHeapId          = p_scene_renderer.samplers->heapSlots[ESamplerType::eTextureSampler];

		gpu::cmd_push_data(gpu_ctx, p_execution_info.cmd, push_data);
		gpu::cmd_draw_mesh_tasks(gpu_ctx, p_execution_info.cmd, 1u, 1u, 1u);
		gpu::cmd_end_rendering(gpu_ctx, p_execution_info.cmd);
	}

	auto scene_renderer_execute_composite_pass(SceneRenderer &p_scene_renderer, const CompositePassExecutionInfo &p_execution_info) -> void
	{
		gpu::GPUContext *gpu_ctx{p_scene_renderer.scene->environmentCtx->gpuCtx};

		gpu::TextureMemoryBarrier texture_memory_barrier{};
		texture_memory_barrier.texture       = p_scene_renderer.attachments[SceneRenderer::EAttachmentType::eColour];
		texture_memory_barrier.srcAccessMask = gpu::EAccessFlagBits::eColorAttachmentWrite;
		texture_memory_barrier.dstAccessMask = gpu::EAccessFlagBits::eShaderRead;
		texture_memory_barrier.srcStageMask  = gpu::EPipelineStageFlagBits::eColorAttachmentOutput;
		texture_memory_barrier.dstStageMask  = gpu::EPipelineStageFlagBits::eFragmentShader;
		gpu::cmd_pipeline_barrier(gpu_ctx, p_execution_info.cmd, {}, texture_memory_barrier);

		gpu::RenderingInfo rendering_info{};
		rendering_info.colourAttachments = {
			gpu::RenderingAttachmentInfo{
				gpu::ClearColourValue{1.0f, 0.0f, 1.0f, 1.0f},
				p_execution_info.targetColourAttachment,
				nullptr,
				gpu::EAttachmentUsageOP::eClearStore,
				gpu::EAttachmentResolveMode::eNone
			}
		};
		rendering_info.renderArea = math::Rect{p_scene_renderer.renderAreaSize};

		gpu::cmd_begin_rendering(gpu_ctx, p_execution_info.cmd, rendering_info);
		gpu::cmd_bind_shaders(gpu_ctx, p_execution_info.cmd, p_scene_renderer.shaderLib->compositeShaders);

		gpu::cmd_set_colour_blend_enable(gpu_ctx, p_execution_info.cmd, {false});
		gpu::cmd_set_rasterization_samples(gpu_ctx, p_execution_info.cmd, gpu::ESampleCount::e1);
		gpu::cmd_set_depth_state(gpu_ctx, p_execution_info.cmd, false, false);
		gpu::cmd_set_colour_write_enable(gpu_ctx, p_execution_info.cmd, {true});

		struct PushData
		{
			math::Float2 screenResolution;
			uint32       colourAttachmentHeapId;
			uint32       samplerHeapId;
		};

		PushData push_data{};
		push_data.screenResolution       = {static_cast<float32>(p_scene_renderer.renderAreaSize.x), static_cast<float32>(p_scene_renderer.renderAreaSize.y)};
		push_data.colourAttachmentHeapId = p_scene_renderer.colourAttachmentHeapSlot;
		push_data.samplerHeapId          = p_scene_renderer.samplers->heapSlots[ESamplerType::eTextureSampler];

		gpu::cmd_push_data(gpu_ctx, p_execution_info.cmd, push_data);
		gpu::cmd_draw_mesh_tasks(gpu_ctx, p_execution_info.cmd, 1u, 1u, 1u);
		gpu::cmd_end_rendering(gpu_ctx, p_execution_info.cmd);
	}
}
