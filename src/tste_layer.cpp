#include "toaster_editor/tste_layer.hpp"

#include <toast_gpu/api.hpp>
#include <toast_gpu/allocation.hpp>
#include <toast_gpu/upload.hpp>

#include <print>

#include <backends/imgui_impl_toaster.h>
#include <backends/imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <filesystem>
#include <GLFW/glfw3native.h>
#include <toast_gpu/frame.hpp>

#include <toast_kernel/events/event.hpp>
#include <toast_kernel/events/key_event.hpp>
#include <toast_kernel/events/window_event.hpp>
#include <toast_render/material.hpp>
#include <toast_render/mesh.hpp>
#include <toast_render/transform.hpp>

#include "toaster_editor/components.hpp"
#include "toaster_editor/environment.hpp"

#undef min
#undef max

#include <execution>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <toast_render/mesh.hpp>

#include <print>

#include <meshoptimizer.h>

namespace fastgltf
{
	template<>
	struct ElementTraits<toaster::math::Float2> : ElementTraitsBase<toaster::math::Float2, AccessorType::Vec2, float32>
	{
	};

	template<>
	struct ElementTraits<toaster::math::Float3> : ElementTraitsBase<toaster::math::Float3, AccessorType::Vec3, float32>
	{
	};

	template<>
	struct ElementTraits<toaster::math::Float4> : ElementTraitsBase<toaster::math::Float4, AccessorType::Vec4, float32>
	{
	};
}

namespace tste
{
	auto generate_geometry_data(const Vector<rnd::MeshVertex> &p_vertices, const Vector<uint32> &    p_indices, uint16               p_material_index,
								Vector<rnd::MeshVertex> &      p_out_vertices, Vector<rnd::Meshlet> &p_out_meshlets, Vector<uint32> &p_out_meshlet_vertices,
								Vector<uint8> &                p_out_meshlet_triangles) -> void
	{
		static constexpr uint64  maxVertices{64u};
		static constexpr uint64  maxTriangles{128u};
		static constexpr float32 coneWeight{0.0f};

		Vector<uint32> remap(p_vertices.size());
		const uint64   unique_vertex_count{
			meshopt_generateVertexRemap(remap.data(), p_indices.data(), p_indices.size(), p_vertices.data(), p_vertices.size(), sizeof(rnd::MeshVertex))
		};

		Vector<rnd::MeshVertex> optimised_vertices(unique_vertex_count);
		Vector<uint32>          optimised_indices(p_indices.size());

		meshopt_remapIndexBuffer(optimised_indices.data(), p_indices.data(), p_indices.size(), remap.data());
		meshopt_remapVertexBuffer(optimised_vertices.data(), p_vertices.data(), p_vertices.size(), sizeof(rnd::MeshVertex), remap.data());

		meshopt_optimizeVertexCache(optimised_indices.data(), optimised_indices.data(), optimised_indices.size(), optimised_vertices.size());
		meshopt_optimizeVertexFetch(optimised_vertices.data(), optimised_indices.data(), optimised_indices.size(), optimised_vertices.data(), optimised_vertices.size(),
									sizeof(rnd::MeshVertex));

		const uint64 max_meshlets{meshopt_buildMeshletsBound(optimised_indices.size(), maxVertices, maxTriangles)};

		Vector<meshopt_Meshlet> local_meshlets(max_meshlets);
		Vector<uint32>          local_meshlet_vertices(max_meshlets * maxVertices);
		Vector<uint8>           local_meshlet_triangles(max_meshlets * maxTriangles * 3u);

		const uint64 meshlet_count{
			meshopt_buildMeshlets(local_meshlets.data(), local_meshlet_vertices.data(), local_meshlet_triangles.data(), optimised_indices.data(),
								  optimised_indices.size(), &optimised_vertices[0u].position.x, optimised_vertices.size(), sizeof(rnd::MeshVertex), maxVertices,
								  maxTriangles, coneWeight)
		};

		// Shrink the meshlet data to the actual packed counts.
		local_meshlets.resize(meshlet_count);
		uint64 meshlet_vertex_count{0u};
		uint64 meshlet_triangle_count{0u};
		for (const meshopt_Meshlet &meshlet: local_meshlets)
		{
			meshlet_vertex_count   = std::max(meshlet_vertex_count, static_cast<uint64>(meshlet.vertex_offset) + static_cast<uint64>(meshlet.vertex_count));
			meshlet_triangle_count = std::max(meshlet_triangle_count, static_cast<uint64>(meshlet.triangle_offset) + static_cast<uint64>(meshlet.triangle_count) * 3ull);
		}
		local_meshlet_vertices.resize(meshlet_vertex_count);
		local_meshlet_triangles.resize(meshlet_triangle_count);

		const uint32 global_vertex_offset{static_cast<uint32>(p_out_vertices.size())};
		p_out_vertices.insert(p_out_vertices.end(), optimised_vertices.begin(), optimised_vertices.end());

		const uint64 global_meshlet_offset{p_out_meshlets.size()};
		p_out_meshlets.resize(global_meshlet_offset + meshlet_count);

		const uint64 global_meshlet_vertex_offset{p_out_meshlet_vertices.size()};
		const uint64 global_meshlet_triangle_offset{p_out_meshlet_triangles.size()};

		for (uint64 m{0u}; m < meshlet_count; ++m)
		{
			const meshopt_Meshlet &local_meshlet{local_meshlets[m]};

			const meshopt_Bounds meshlet_bounds{
				meshopt_computeMeshletBounds(&local_meshlet_vertices[local_meshlet.vertex_offset], &local_meshlet_triangles[local_meshlet.triangle_offset],
											 local_meshlet.triangle_count, &p_out_vertices[global_vertex_offset].position.x, optimised_vertices.size(),
											 sizeof(rnd::MeshVertex))
			};

			rnd::Meshlet &meshlet{p_out_meshlets[global_meshlet_offset + m]};
			meshlet.vertexOffset   = static_cast<uint32>(global_meshlet_vertex_offset) + local_meshlet.vertex_offset;
			meshlet.triangleOffset = static_cast<uint32>(global_meshlet_triangle_offset) + local_meshlet.triangle_offset;
			meshlet.vertexCount    = local_meshlet.vertex_count;
			meshlet.triangleCount  = local_meshlet.triangle_count;
			meshlet.materialIndex  = p_material_index;
			meshlet.boundingSphere = {meshlet_bounds.center[0u], meshlet_bounds.center[1u], meshlet_bounds.center[2u], meshlet_bounds.radius};
		}

		for (uint32 &vertex_index: local_meshlet_vertices)
			vertex_index += global_vertex_offset;

		p_out_meshlet_vertices.insert(p_out_meshlet_vertices.end(), local_meshlet_vertices.begin(), local_meshlet_vertices.end());
		p_out_meshlet_triangles.insert(p_out_meshlet_triangles.end(), local_meshlet_triangles.begin(), local_meshlet_triangles.end());
	}

	[[nodiscard]] auto TST_MATH_CALLCONV load_gltf_mesh(const fastgltf::Asset &              p_asset, const fastgltf::Mesh &             p_mesh,
														std::span<const rnd::MaterialHandle> p_scene_materials, rnd::MaterialHandle      p_default_material,
														Vector<rnd::MeshVertex> &            p_out_mesh_vertices, Vector<rnd::Meshlet> & p_out_mesh_meshlets,
														Vector<uint32> &                     p_out_mesh_meshlet_vertices, Vector<uint8> &p_out_mesh_meshlet_indices,
														Vector<rnd::MaterialHandle> &        p_out_mesh_materials) -> math::Vector
	{
		math::Vector min_bounds{XMVectorReplicate(FLT_MAX)};
		math::Vector max_bounds{XMVectorReplicate(-FLT_MAX)};

		for (const auto &primitive: p_mesh.primitives)
		{
			if (primitive.type != fastgltf::PrimitiveType::Triangles || !primitive.indicesAccessor.has_value())
				continue;

			rnd::MaterialHandle material_handle{p_default_material};
			if (primitive.materialIndex.has_value())
			{
				material_handle = p_default_material;
				// material_handle = p_scene_materials[primitive.materialIndex.value()];
			}

			uint16 local_material_index{0u};
			if (auto material_it{std::ranges::find(p_out_mesh_materials, material_handle)}; material_it != p_out_mesh_materials.end())
				local_material_index = static_cast<uint16>(std::distance(p_out_mesh_materials.begin(), material_it));
			else
			{
				local_material_index = static_cast<uint16>(p_out_mesh_materials.size());
				p_out_mesh_materials.push_back(material_handle);
			}

			const auto pos_it{primitive.findAttribute("POSITION")};
			if (pos_it == primitive.attributes.end())
				continue;

			const auto &pos_accessor{p_asset.accessors[pos_it->accessorIndex]};

			if (pos_accessor.min.has_value() && pos_accessor.max.has_value())
			{
				const auto & min_values{pos_accessor.min.value()};
				const auto & max_values{pos_accessor.max.value()};
				math::Vector min_vector{
					XMVectorSet(static_cast<float32>(min_values.get<float64>(0u)), static_cast<float32>(min_values.get<float64>(1u)),
								static_cast<float32>(min_values.get<float64>(2u)), 0.0f)
				};
				math::Vector max_vector{
					XMVectorSet(static_cast<float32>(max_values.get<float64>(0u)), static_cast<float32>(max_values.get<float64>(1u)),
								static_cast<float32>(max_values.get<float64>(2u)), 0.0f)
				};

				min_bounds = XMVectorMin(min_bounds, min_vector);
				max_bounds = XMVectorMax(max_bounds, max_vector);
			}

			Vector<rnd::MeshVertex> primitive_vertices(pos_accessor.count);

			fastgltf::iterateAccessorWithIndex<math::Float3>(p_asset, pos_accessor, [&primitive_vertices](math::Float3 p_pos, std::size_t p_idx) -> void
			{
				primitive_vertices[p_idx].position = p_pos;
			});

			if (const auto normal_it{primitive.findAttribute("NORMAL")}; normal_it != primitive.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<math::Float3>(p_asset, p_asset.accessors[normal_it->accessorIndex],
																 [&primitive_vertices](math::Float3 p_normal, std::size_t p_idx) -> void
																 {
																	 primitive_vertices[p_idx].normal = p_normal;
																 });
			}

			if (const auto texcoord_it{primitive.findAttribute("TEXCOORD_0")}; texcoord_it != primitive.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<math::Float2>(p_asset, p_asset.accessors[texcoord_it->accessorIndex],
																 [&primitive_vertices](math::Float2 p_texcoord, std::size_t p_idx) -> void
																 {
																	 primitive_vertices[p_idx].texCoord = p_texcoord;
																 });
			}

			if (const auto tangent_it{primitive.findAttribute("TANGENT")}; tangent_it != primitive.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<math::Float4>(p_asset, p_asset.accessors[tangent_it->accessorIndex],
																 [&primitive_vertices](math::Float4 p_tangent, std::size_t p_idx) -> void
																 {
																	 rnd::MeshVertex &vertex{primitive_vertices[p_idx]};
																	 math::Vector     tangent{XMLoadFloat4(&p_tangent)};
																	 math::Vector     normal{XMLoadFloat3(&vertex.normal)};

																	 math::Vector bitangent{XMVector3Cross(normal, tangent)};
																	 bitangent = XMVectorScale(bitangent, XMVectorGetW(tangent));

																	 XMStoreFloat3(&vertex.tangent, tangent);
																	 XMStoreFloat3(&vertex.bitangent, bitangent);
																 });
			}

			auto &         index_accessor{p_asset.accessors[primitive.indicesAccessor.value()]};
			Vector<uint32> primitive_indices(index_accessor.count);
			fastgltf::iterateAccessorWithIndex<uint32>(p_asset, index_accessor, [&primitive_indices](uint32 p_index, std::size_t p_idx) -> void
			{
				primitive_indices[p_idx] = p_index;
			});

			generate_geometry_data(primitive_vertices, primitive_indices, local_material_index, p_out_mesh_vertices, p_out_mesh_meshlets, p_out_mesh_meshlet_vertices,
								   p_out_mesh_meshlet_indices);
		}

		math::Vector  bounds_center{(min_bounds + max_bounds) * 0.5f};
		math::Vector  extent{max_bounds - bounds_center};
		const float32 radius{XMVectorGetX(XMVector3Length(extent))};
		return XMVectorSet(XMVectorGetX(bounds_center), XMVectorGetY(bounds_center), XMVectorGetZ(bounds_center), radius);
	}

	[[nodiscard]] auto load_gltf_scene_entity(Application &p_app, TSTELayer &p_layer, const std::filesystem::path &p_path) -> entt::entity
	{
		fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);

		auto data{fastgltf::GltfDataBuffer::FromPath(p_path)};
		if (data.error() != fastgltf::Error::None)
		{
			TST_PERMA_ASSERT(false);
		}

		constexpr auto options{fastgltf::Options::LoadExternalBuffers};

		auto expected{parser.loadGltf(data.get(), p_path.parent_path(), options)};
		if (expected.error() != fastgltf::Error::None)
		{
			TST_PERMA_ASSERT(false);
		}

		auto &asset{expected.get()};

		// First, load the materials.
		Vector<rnd::MaterialHandle> scene_materials(asset.materials.size());
		auto                        material_view{std::views::enumerate(asset.materials)};
		std::for_each(std::execution::par_unseq, material_view.begin(), material_view.end(), [&scene_materials, &p_app](auto &&p_pair) -> void
		{
			auto &&[i, material]{p_pair};

			rnd::MaterialHandle &material_handle{scene_materials[i]};
			material_handle = rnd::material_create(p_app.materialMngr);

			rnd::Material &mat{rnd::material_get(p_app.materialMngr, material_handle)};
			mat.params.albedoColour = math::Float4{material.pbrData.baseColorFactor.data()};

			mat.params.flags = rnd::EMaterialFlagBits::eBackfaceCulling | rnd::EMaterialFlagBits::eNoNormalMap | rnd::EMaterialFlagBits::eNoRoughnessMap |
							   rnd::EMaterialFlagBits::eNoMetalnessMap;

			if (material.alphaMode != fastgltf::AlphaMode::Opaque)
				mat.params.flags |= rnd::EMaterialFlagBits::eTransparent;

			mat.params.roughness = 0.1f;
			// mat.params.roughness = material.pbrData.roughnessFactor;
			// mat.params.metalness = material.pbrData.metallicFactor;
			if (material.doubleSided)
				mat.params.flags &= ~rnd::EMaterialFlagBits::eBackfaceCulling;

			rnd::material_mark_dirty(p_app.materialMngr, material_handle);
		});

		gpu::StateTrackerHandle       mesh_state_tracker{gpu::state_tracker_register(p_app.uploadCtx, asset.meshes.size())};
		Vector<rnd::StaticMeshHandle> scene_meshes(asset.meshes.size());
		auto                          mesh_view{std::views::enumerate(asset.meshes)};
		std::for_each(std::execution::par_unseq, mesh_view.begin(), mesh_view.end(),
					  [&asset, &scene_meshes, &scene_materials, &p_app, &p_layer, mesh_state_tracker](auto &&p_pair) -> void
					  {
						  auto &&[i, mesh]{p_pair};

						  rnd::StaticMeshHandle &mesh_handle{scene_meshes[i]};

						  Vector<rnd::MeshVertex>     mesh_vertices;
						  Vector<rnd::Meshlet>        mesh_meshlets;
						  Vector<uint32>              mesh_meshlet_vertices;
						  Vector<uint8>               mesh_meshlet_indices;
						  Vector<rnd::MaterialHandle> mesh_materials;

						  math::Vector mesh_bounds{
							  load_gltf_mesh(asset, mesh, scene_materials, p_layer.defaultMaterial, mesh_vertices, mesh_meshlets, mesh_meshlet_vertices,
											 mesh_meshlet_indices, mesh_materials)
						  };

						  mesh_handle = rnd::static_mesh_create(p_app.meshMngr, mesh_state_tracker, mesh_vertices, mesh_meshlets, mesh_meshlet_vertices,
																mesh_meshlet_indices, mesh_materials, mesh_bounds);
					  });

		gpu::state_tracker_wait_for(p_app.uploadCtx, mesh_state_tracker);

		Vector<entt::entity> node_entities(asset.nodes.size());
		for (uint32 i{0u}; i < node_entities.size(); ++i)
		{
			const fastgltf::Node &node{asset.nodes[i]};

			node_entities[i] = p_layer.scene.registry.create();

			auto &tc{p_layer.scene.registry.emplace<TransformComponent>(node_entities[i])};
			std::visit(fastgltf::visitor{
						   [&tc](const fastgltf::TRS &p_trs)
						   {
							   tc.translation = math::Float3{p_trs.translation.data()};
							   tc.orientation = math::Float4{p_trs.rotation.data()};
							   tc.scale       = math::Float3{p_trs.scale.data()};
						   },
						   [&tc](const fastgltf::math::fmat4x4 &p_matrix)
						   {
							   math::Vector translation;
							   math::Vector orientation;
							   math::Vector scale;

							   math::Float4x4 matrix{p_matrix.data()};
							   math::Matrix   transform{XMLoadFloat4x4(&matrix)};
							   XMMatrixDecompose(&scale, &orientation, &translation, transform);

							   tc.setTranslation(translation);
							   tc.setOrientation(orientation);
							   tc.setScale(scale);
						   }
					   }, node.transform);

			if (node.meshIndex.has_value())
			{
				auto &smc{p_layer.scene.registry.emplace<StaticMeshComponent>(node_entities[i])};
				smc.mesh = scene_meshes[node.meshIndex.value()];

				auto &gpu_tc{p_layer.scene.registry.emplace<GPUTransformComponent>(node_entities[i], rnd::transform_create(p_app.transformMngr, tc.getTransform()))};
			}

			if (node.lightIndex.has_value())
			{
				const fastgltf::Light &light{asset.lights[node.lightIndex.value()]};

				if (light.type == fastgltf::LightType::Point)
				{
					auto &plc{p_layer.scene.registry.emplace<PointLightComponent>(node_entities[i])};
					plc.colour = math::Float3{light.color.data()};
				}
			}
		}

		return p_layer.scene.registry.create();
	}

	auto tste_init(Application &p_app, TSTELayer &p_layer) -> void
	{
		samplers_create(p_layer.samplers, p_app.gpuCtx, p_app.samplerHeap);

		p_layer.singleTimeCommandAllocator = gpu::cmd_allocator_create(p_app.gpuCtx, gpu::EQueueType::eGraphics);

		shader_library_create(p_layer.shaderLib, p_app.gpuCtx);
		environment_ctx_create(p_layer.environmentCtx, p_app.gpuCtx, p_app.uploadCtx, p_app.resourceHeap);

		scene_create(p_layer.scene, p_layer.environmentCtx, "Test_Scene");

		const math::UInt2 window_size{knl::window_get_size(p_app.window)};
		scene_renderer_create(p_layer.sceneRenderer, p_layer.scene, p_layer.shaderLib, p_layer.samplers, {.initialSize = window_size});

		asset_mngr_create(p_layer.assetMngr, EditorAssetManagerDesc{
							  .materialMngr = p_app.materialMngr,
							  .gpuCtx = p_app.gpuCtx,
							  .allocCtx = p_app.allocCtx,
							  .uploadCtx = p_app.uploadCtx,
							  .resourceHeap = p_app.resourceHeap
						  });

		#pragma region camera

		p_layer.editorCamera = EditorCamera{90.0f, knl::window_get_aspect_ratio(p_app.window), 0.1f, 1000.0f};

		constexpr gpu::BufferDesc camera_buffer_desc{
			.size = sizeof(CameraCB),
			.usage = gpu::EBufferUsageFlagBits::eUniformBuffer,
			.memoryType = gpu::EMemoryType::eHostVisibleCoherent
		};
		gpu::buffer_create_n(p_app.gpuCtx, camera_buffer_desc, p_layer.cameraUBOs);
		gpu::buffer_get_address_n(p_app.gpuCtx, p_layer.cameraUBOs, p_layer.cameraBDAs);

		#pragma endregion

		(void) ImGui::CreateContext();
		ImGui_ImplToaster_InitInfo imgui_toaster_init_info{
			.gpuCtx = p_app.gpuCtx,
			.uploadCtx = p_app.uploadCtx,
			.resourceHeap = p_app.resourceHeap,
			.samplerHeap = p_app.samplerHeap,
			.maxFramesInFlight = c_maxFramesInFlight
		};
		ImGui_ImplToaster_Init(imgui_toaster_init_info);
		ImGui_ImplGlfw_InitForVulkan(p_app.window.window, true);

		p_layer.defaultMaterial = rnd::material_create(p_app.materialMngr);
		rnd::material_set_albedo_colour(p_app.materialMngr, p_layer.defaultMaterial, {1.0f, 1.0f, 1.0f, 1.0f});

		p_layer.assetMngr.registry[1ull] = AssetMetadata{1ull, R"(C:\Users\Oliver\Pictures\baki_man.jpg)", 0ull, EAssetType::eTexture};
		TextureAssetHandle texture_asset{texture_asset_load(p_layer.assetMngr, 1ull)};

		const auto &asset{p_layer.assetMngr.textureAssets[texture_asset]};
		rnd::material_set_texture(p_app.materialMngr, p_layer.defaultMaterial, rnd::EMaterialTextureType::eAlbedoMap,
								  rnd::MaterialTexture{asset.texture, asset.stateTracker, asset.shaderReadHeapSlot});

		texture_asset_release(p_layer.assetMngr, texture_asset);

		const std::filesystem::path mesh_path{R"(C:\Users\Oliver\Downloads\main_sponza\main_sponza\NewSponza_Main_glTF_003.gltf)"};
		p_layer.testEntity = load_gltf_scene_entity(p_app, p_layer, mesh_path);
	}

	auto tste_destroy(Application &p_app, TSTELayer &p_layer) -> void
	{
		ImGui_ImplToaster_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		gpu::buffer_destroy_n(p_app.gpuCtx, p_layer.cameraUBOs);

		asset_mngr_destroy(p_layer.assetMngr);

		scene_renderer_destroy(p_layer.sceneRenderer);
		scene_destroy(p_layer.scene);

		shader_library_destroy(p_layer.shaderLib, p_app.gpuCtx);
		environment_ctx_destroy(p_layer.environmentCtx);

		gpu::cmd_allocator_destroy(p_app.gpuCtx, p_layer.singleTimeCommandAllocator);

		samplers_destroy(p_layer.samplers);
	}

	auto tste_on_update(Application &p_app, TSTELayer &p_layer, float32 p_dt) -> void
	{
		knl::InputContext *input_ctx{&p_app.window.cbData.inputCtx};

		if (knl::input_is_key_down(input_ctx, knl::EKeyCode::eLeftAlt) && knl::input_is_key_pressed(input_ctx, knl::EKeyCode::eE))
			p_app.isRunning = false;

		if (knl::input_is_mouse_button_pressed(input_ctx, knl::EMouseButton::eRight))
			knl::input_set_cursor_mode(input_ctx, knl::ECursorMode::eDisabled);
		if (knl::input_is_mouse_button_released(input_ctx, knl::EMouseButton::eRight))
			knl::input_set_cursor_mode(input_ctx, knl::ECursorMode::eNormal);

		if (knl::input_is_mouse_button_down(input_ctx, knl::EMouseButton::eRight))
		{
			p_layer.editorCamera.onUpdate(input_ctx, 0.1f, p_dt);
		}
	}

	auto tste_on_render(Application &p_app, TSTELayer &p_layer, gpu::CommandListHandle p_cmd) -> void
	{
		asset_mngr_update(p_layer.assetMngr);

		#pragma region ImGui

		ImGui_ImplToaster_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{
		}

		ImGui::Render();

		#pragma endregion

		const uint32             frame_index{p_app.frameIndex};
		const gpu::TextureHandle swapchain_target{p_app.window.currentTexture};

		#pragma region camera buffers

		math::Matrix camera_view{p_layer.editorCamera.getViewMatrix()};
		math::Matrix camera_proj{p_layer.editorCamera.getProjectionMatrix()};
		math::Matrix camera_inverse_proj{XMMatrixInverse(nullptr, camera_proj)};
		math::Vector camera_position{p_layer.editorCamera.getPosition()};

		CameraCB camera_cb{};
		XMStoreFloat4x4(&camera_cb.view, camera_view);
		XMStoreFloat4x4(&camera_cb.proj, camera_proj);
		XMStoreFloat4x4(&camera_cb.invProj, camera_inverse_proj);
		XMStoreFloat4(&camera_cb.position, camera_position);

		const auto planes{p_layer.editorCamera.getFrustumPlanes()};
		for (uint32 i{0u}; i < 6u; ++i)
			XMStoreFloat4(&camera_cb.frustumPlanes[i], planes[i]);

		gpu::buffer_write_data(p_app.gpuCtx, p_layer.cameraUBOs[frame_index], &camera_cb, sizeof(CameraCB));

		#pragma endregion

		uint32 active_point_light_count{0u};
		{
			auto mapped{static_cast<PointLight *>(p_layer.scene.lightEnvironment.pointLightMappedDatas[frame_index])};

			const auto view{p_layer.scene.registry.view<PointLightComponent, TransformComponent>()};
			view.each([mapped, &active_point_light_count](PointLightComponent &p_plc, const TransformComponent &p_tc) -> void
			{
				PointLight &point_light_data{mapped[active_point_light_count]};

				point_light_data.position = p_tc.translation;
				point_light_data.colour   = p_plc.colour;

				++active_point_light_count;
			});
		}
		{
			const auto view{p_layer.scene.registry.view<TransformComponent, GPUTransformComponent>()};
			view.each([&p_app, frame_index](TransformComponent &p_tc, const GPUTransformComponent &p_gpu_tc) -> void
			{
				if (p_tc.framesDirty > 0u)
				{
					--p_tc.framesDirty;
					rnd::transform_update(p_app.transformMngr, frame_index, p_gpu_tc.transformId, p_tc.getTransform());
				}
			});
		}

		const uintptr camera_buffer_address{p_layer.cameraBDAs[frame_index]};
		const uintptr transform_buffer_address{rnd::transform_mngr_get_transform_buffer_address(p_app.transformMngr, frame_index)};
		const uintptr material_buffer_address{rnd::material_mngr_get_material_buffer_address(p_app.materialMngr, frame_index)};
		const uintptr mesh_metadata_buffer_address{rnd::mesh_mngr_get_mesh_metadata_buffer_address(p_app.meshMngr)};
		const uintptr material_indirection_buffer_address{rnd::mesh_mngr_get_material_indirection_buffer_address(p_app.meshMngr)};

		Environment &environment{p_layer.environmentCtx.environments[p_layer.environmentCtx.defaultEnvironment]};

		scene_renderer_setup_render_state(p_layer.sceneRenderer, p_cmd);

		scene_renderer_execute_skybox_pass(p_layer.sceneRenderer, SkyboxPassExecutionInfo{
											   .targetColourAttachment = swapchain_target,
											   .cameraBufferAddress = camera_buffer_address,
											   .frameIndex = frame_index,
											   .cmd = p_cmd
										   });

		uint32 active_object_count{0u};
		scene_renderer_execute_object_culling_pre_pass(p_layer.sceneRenderer, ObjectCullingPrePassExecutionInfo{
														   .cameraBufferAddress = camera_buffer_address,
														   .transformBufferAddress = transform_buffer_address,
														   .meshManager = p_app.meshMngr,
														   .frameIndex = frame_index,
														   .cmd = p_cmd
													   }, active_object_count);

		scene_renderer_execute_depth_pre_pass(p_layer.sceneRenderer, DepthPrePassExecutionInfo{
												  .cameraBufferAddress = camera_buffer_address,
												  .transformBufferAddress = transform_buffer_address,
												  .materialBufferAddress = material_buffer_address,
												  .meshMetadataBufferAddress = mesh_metadata_buffer_address,
												  .materialIndirectionBufferAddress = material_indirection_buffer_address,
												  .frameIndex = frame_index,
												  .cmd = p_cmd,
												  .activeObjectCount = active_object_count
											  });

		const uintptr point_light_buffer_address{p_layer.scene.lightEnvironment.pointLightBDAs[frame_index]};
		const uint32  diffuse_irradiance_map_heap_slot{environment.shaderReadHeapSlots[EEnvironmentMapType::eDiffuseIrradiance]};

		scene_renderer_execute_opaque_geometry_pass(p_layer.sceneRenderer, GeometryPassExecutionInfo{
														.cameraBufferAddress = camera_buffer_address,
														.transformBufferAddress = transform_buffer_address,
														.materialBufferAddress = material_buffer_address,
														.meshMetadataBufferAddress = mesh_metadata_buffer_address,
														.materialIndirectionBufferAddress = material_indirection_buffer_address,
														.pointLightBufferAddress = point_light_buffer_address,
														.frameIndex = frame_index,
														.cmd = p_cmd,
														.activeObjectCount = active_object_count,
														.activePointLightCount = active_point_light_count
													});

		scene_renderer_execute_transparent_geometry_pass(p_layer.sceneRenderer, TransparentGeometryPassExecutionInfo{
															 .cameraBufferAddress = camera_buffer_address,
															 .transformBufferAddress = transform_buffer_address,
															 .materialBufferAddress = material_buffer_address,
															 .meshMetadataBufferAddress = mesh_metadata_buffer_address,
															 .materialIndirectionBufferAddress = material_indirection_buffer_address,
															 .pointLightBufferAddress = point_light_buffer_address,
															 .frameIndex = frame_index,
															 .cmd = p_cmd,
															 .activeObjectCount = active_object_count,
															 .activePointLightCount = active_point_light_count
														 });

		scene_renderer_execute_OIT_resolve_pass(p_layer.sceneRenderer, OITResolvePassExecutionInfo{.frameIndex = frame_index, .cmd = p_cmd});

		scene_renderer_execute_composite_pass(p_layer.sceneRenderer, CompositePassExecutionInfo{
												  .targetColourAttachment = swapchain_target,
												  .frameIndex = frame_index,
												  .cmd = p_cmd
											  });

		{
			const gpu::RenderingInfo rendering_info{
				.renderArea = knl::window_get_size(p_app.window),
				.colourAttachments = {
					gpu::RenderingAttachmentInfo{
						.clearValue = gpu::ClearColourValue{1.0f, 0.0f, 1.0f, 1.0f},
						.renderTarget = swapchain_target,
						.resolveTarget = nullptr,
						.usageOp = gpu::EAttachmentUsageOP::eLoadStore,
						.resolveMode = gpu::EAttachmentResolveMode::eNone
					}
				}
			};

			gpu::cmd_begin_rendering(p_app.gpuCtx, p_cmd, rendering_info);

			ImGui_ImplToaster_RenderDrawData(ImGui::GetDrawData(), p_cmd);

			gpu::cmd_end_rendering(p_app.gpuCtx, p_cmd);
		}
	}

	auto tste_on_event(Application &p_app, TSTELayer &p_layer, knl::Event &p_event) -> void
	{
		knl::EventDispatcher ed{p_event};

		// This has to happen here, so it gets called outside of the frame loop because the swapchain will experience issues
		ed.dispatch<knl::KeyPressEvent>([&p_app](knl::KeyPressEvent &p_e) -> bool
		{
			if (p_e.getKeyCode() == knl::EKeyCode::eF11)
			{
				knl::Window &window{p_app.window};

				if (knl::window_is_fullscreen(window))
				{
					knl::window_set_windowed(window);
					knl::window_maximise(window);
				}
				else
					knl::window_set_fullscreen(window);
			}

			return false;
		});

		ed.dispatch<knl::WindowResizeEvent>([&p_layer](knl::WindowResizeEvent &p_e)-> bool
		{
			const math::UInt2 size{p_e.getSize()};
			if (size.x > 0u && size.y > 0u)
			{
				scene_renderer_on_resize(p_layer.sceneRenderer, size);
				p_layer.editorCamera.onResize(p_e.getAspectRatio());
			}

			return true;
		});

		ed.dispatch<knl::WindowFileDropEvent>([&p_layer](knl::WindowFileDropEvent &p_e)-> bool
		{
			const Vector<String> &paths{p_e.getFilepaths()};

			return true;
		});
	}
}
