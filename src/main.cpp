#include <chrono>

#include "toaster_editor/tste_layer.hpp"

#include <iostream>

#include <toast_os/console.hpp>
#include <toast_os/entry_points.hpp>

#include <toast_gpu/allocation.hpp>
#include <toast_gpu/api.hpp>
#include <toast_gpu/frame.hpp>
#include <toast_gpu/upload.hpp>
#include <toast_kernel/events/event.hpp>

#include <toast_render/material.hpp>
#include <toast_render/mesh.hpp>
#include <toast_render/transform.hpp>

TST_OS_WINMAIN()
{
	#ifndef NDEBUG
	os::output_console_create("Toaster Editor Debug Output");
	#endif

	Allocator allocator{};
	allocator_create(allocator, +[](CString p_msg) -> void
	{
		TST_PERMA_ASSERT_MSG(false, p_msg);
	});

	{
		tste::Application app{};

		// Create the GPU contexts
		#ifndef NDEBUG
		static constexpr bool c_enable_debug_info{true};
		#else
		static constexpr bool c_enable_debug_info{false};
		#endif

		app.gpuCtx    = gpu::gpu_ctx_create(gpu::GPUContextDesc{c_enable_debug_info, true, tste::c_maxFramesInFlight});
		app.frameCtx  = gpu::frame_ctx_create(app.gpuCtx, gpu::FrameContextDesc{.maxFramesInFlight = tste::c_maxFramesInFlight});
		app.allocCtx  = gpu::allocation_ctx_create();
		app.uploadCtx = gpu::upload_ctx_create(app.gpuCtx, app.allocCtx, gpu::UploadContextDesc{.maxAllocationCommandLists = 2u});

		static constexpr uint32 c_max_buffer_descriptors{128u};
		static constexpr uint32 c_max_texture_descriptors{1024u};
		static constexpr uint32 c_max_sampler_descriptors{8u};
		app.resourceHeap = gpu::resource_descriptor_heap_create(app.gpuCtx, {c_max_buffer_descriptors, c_max_texture_descriptors});
		app.samplerHeap  = gpu::sampler_descriptor_heap_create(app.gpuCtx, {c_max_sampler_descriptors});

		static constexpr uint32 c_max_gpu_materials{1028u};
		static constexpr uint32 c_max_transforms{100000u};

		app.materialMngr = rnd::material_mngr_create(rnd::MaterialManagerDesc{
														 app.gpuCtx,
														 app.allocCtx,
														 app.uploadCtx,
														 app.frameCtx,
														 app.resourceHeap,
														 tste::c_maxFramesInFlight,
														 c_max_gpu_materials
													 });
		app.meshMngr      = rnd::mesh_mngr_create(rnd::MeshManagerDesc{app.gpuCtx, app.allocCtx, app.uploadCtx, app.frameCtx, app.resourceHeap});
		app.transformMngr = rnd::transform_mngr_create(rnd::TransformManagerDesc{app.gpuCtx, tste::c_maxFramesInFlight, c_max_transforms});

		knl::window_init_api(); // glfwInit()

		knl::window_create(app.window, app.gpuCtx, knl::WindowDesc{math::UInt2{1920u, 1080u}, "Toaster Editor", true});
		knl::window_set_event_callback_user_data(app.window, &app);
		knl::window_set_event_callback(app.window, +[](knl::Event &p_event, void *p_user_data) -> void
		{
			auto &app{*static_cast<tste::Application *>(p_user_data)};
			for (const tste::AppLayer &layer: app.layers)
			{
				if (p_event.isHandled())
					continue;

				if (layer.onEventFn)
					layer.onEventFn(app, layer.userData, p_event);
			}
		});

		static constexpr uint32 c_tste_layer_index{0u};

		tste::TSTELayer tste_layer{}; // There is no reason to heap allocate this
		tste::AppLayer &app_layer{app.layers[c_tste_layer_index]};
		app_layer.userData   = &tste_layer;
		app_layer.initFn     = +[](tste::Application &p_app, void *p_layer) -> void { tste::tste_init(p_app, *static_cast<tste::TSTELayer *>(p_layer)); };
		app_layer.destroyFn  = +[](tste::Application &p_app, void *p_layer) -> void { tste::tste_destroy(p_app, *static_cast<tste::TSTELayer *>(p_layer)); };
		app_layer.onUpdateFn = +[](tste::Application &p_app, void *p_layer, float32 p_dt) -> void
		{
			tste::tste_on_update(p_app, *static_cast<tste::TSTELayer *>(p_layer), p_dt);
		};
		app_layer.onRenderFn = +[](tste::Application &p_app, void *p_layer, gpu::CommandListHandle p_cmd) -> void
		{
			tste::tste_on_render(p_app, *static_cast<tste::TSTELayer *>(p_layer), p_cmd);
		};
		app_layer.onEventFn = +[](tste::Application &p_app, void *p_layer, knl::Event &p_event) -> void
		{
			tste::tste_on_event(p_app, *static_cast<tste::TSTELayer *>(p_layer), p_event);
		};

		tste::application_layer_init(app, c_tste_layer_index);

		tste::PFFArray<gpu::CommandAllocatorHandle> command_allocators;
		tste::PFFArray<gpu::CommandListHandle>      command_lists;
		for (uint32 i{0u}; i < tste::c_maxFramesInFlight; ++i)
		{
			command_allocators[i] = gpu::cmd_allocator_create(app.gpuCtx, gpu::EQueueType::eGraphics);
			gpu::cmd_list_allocate_n(app.gpuCtx, command_allocators[i], std::span<gpu::CommandListHandle>{&command_lists[i], 1ull});
		}

		static constexpr uint32 c_framerate{360u}; // TODO: Set to the monitor's refresh rate

		auto last_time{std::chrono::steady_clock::now()};
		gpu::swapchain_set_max_framerate(app.gpuCtx, app.window.swapchain, c_framerate);

		while (app.isRunning)
		{
			{
				TSTE_PROFILE_SCP_N("swapchain_wait_for_framerate");
				// gpu::swapchain_wait_for_framerate(app.gpuCtx, app.window.swapchain);
			}
			gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::eInputSample);

			if (!knl::window_process_messages(app.window)) // glfwPollEvents. returns !glfwWindowShouldClose(...);
				break;
			// Returns false if the window is either an invalid size or the swapchain failed to recreate
			if (!knl::window_check_for_resize(app.window))
				continue;

			gpu::poll_uploads(app.uploadCtx);
			gpu::begin_frame(app.frameCtx, app.frameIndex);

			auto                           current_time{std::chrono::steady_clock::now()};
			std::chrono::duration<float32> elapsed{current_time - last_time};
			app.deltaTime = elapsed.count();
			last_time     = current_time;

			const gpu::CommandListHandle cmd{command_lists[app.frameIndex]};
			gpu::cmd_allocator_reset(app.gpuCtx, command_allocators[app.frameIndex]);
			gpu::cmd_list_open(app.gpuCtx, cmd);

			if (!knl::window_begin_frame(app.window, cmd)) // Attempts to acquire the image
			{
				gpu::cmd_allocator_reset(app.gpuCtx, command_allocators[app.frameIndex]);
				continue;
			}

			if (!knl::window_is_minimised(app.window))
			{
				gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::eSimulationStart);
				for (const tste::AppLayer &layer: app.layers)
				{
					if (layer.onUpdateFn)
						layer.onUpdateFn(app, layer.userData, app.deltaTime);
				}
				gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::eSimulationEnd);
			}

			// Poll the material manager's pending textures and update all dirty materials (this should not just happen inside knl::window_is_minimised(app.window))
			rnd::material_mngr_poll_pending_textures(app.materialMngr);
			rnd::material_mngr_update_dirty_materials(app.materialMngr, app.frameIndex);

			// Bind the descriptor heaps for the frame
			gpu::cmd_bind_resource_heap(app.gpuCtx, cmd, app.resourceHeap);
			gpu::cmd_bind_sampler_heap(app.gpuCtx, cmd, app.samplerHeap);

			gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::eRendersubmitStart);
			if (!knl::window_is_minimised(app.window))
			{
				for (const tste::AppLayer &layer: app.layers)
				{
					if (layer.onRenderFn)
						layer.onRenderFn(app, layer.userData, cmd);
				}
			}

			gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::eRendersubmitEnd);
			gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::ePresentStart);
			window_submit_and_present(app.window, app.frameCtx, cmd); // also updates the input states
			gpu::swapchain_set_frame_marker(app.gpuCtx, app.window.swapchain, gpu::EFrameMarker::ePresentEnd);
			app.frameIndex = (app.frameIndex + 1u) % tste::c_maxFramesInFlight;
		}

		gpu::wait_idle(app.gpuCtx); // Wait for the commands lists to finish before destroying the allocators
		for (const gpu::CommandAllocatorHandle alloc: command_allocators)
			gpu::cmd_allocator_destroy(app.gpuCtx, alloc);

		gpu::poll_uploads(app.uploadCtx);

		tste::application_layer_destroy(app, c_tste_layer_index);

		knl::window_destroy(app.window);
		knl::window_shutdown_api();

		rnd::transform_mngr_destroy(app.transformMngr);
		rnd::mesh_mngr_destroy(app.meshMngr);
		rnd::material_mngr_destroy(app.materialMngr);

		gpu::sampler_descriptor_heap_destroy(app.gpuCtx, app.samplerHeap);
		gpu::resource_descriptor_heap_destroy(app.gpuCtx, app.resourceHeap);

		gpu::upload_ctx_destroy(app.uploadCtx);
		gpu::allocation_ctx_destroy(app.allocCtx);
		gpu::frame_ctx_destroy(app.frameCtx);
		gpu::gpu_ctx_destroy(app.gpuCtx);
	}

	allocator_destroy(allocator);

	#ifndef NDEBUG
	os::output_console_destroy();
	#endif

	return 0;
}
