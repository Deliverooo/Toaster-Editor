#include "toaster_editor/tste_layer.hpp"

#include <toast_gpu/api.hpp>
#include <toast_gpu/allocation.hpp>
#include <toast_gpu/upload.hpp>

#include <print>

#include <backends/imgui_impl_toaster.h>
#include <backends/imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace tste
{
	auto tste_init(Application &p_app, TSTELayer &p_layer) -> void
	{
		std::println("tste_init");

		shader_library_create(p_layer.shaderLib, p_app.gpuCtx);

		(void) ImGui::CreateContext();
		ImGui_ImplToaster_InitInfo imgui_toaster_init_info{
			.gpuCtx = p_app.gpuCtx,
			.frameCtx = p_app.frameCtx,
			.uploadCtx = p_app.uploadCtx,
			.resourceHeap = p_app.resourceHeap,
			.samplerHeap = p_app.samplerHeap,
			.maxFramesInFlight = c_maxFramesInFlight
		};
		ImGui_ImplToaster_Init(imgui_toaster_init_info);
		ImGui_ImplGlfw_InitForVulkan(p_app.window.window, true);
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	}

	auto tste_destroy(Application &p_app, TSTELayer &p_layer) -> void
	{
		ImGui_ImplToaster_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		shader_library_destroy(p_layer.shaderLib, p_app.gpuCtx);
	}

	auto tste_on_update(Application &p_app, TSTELayer &p_layer, float32 p_dt) -> void
	{
	}

	auto tste_on_render(Application &p_app, TSTELayer &p_layer, gpu::CommandListHandle p_cmd) -> void
	{
		const gpu::TextureHandle swapchain_target{p_app.window.currentTexture};

		gpu::cmd_bind_resource_heap(p_app.gpuCtx, p_cmd, p_app.resourceHeap);
		gpu::cmd_bind_sampler_heap(p_app.gpuCtx, p_cmd, p_app.samplerHeap);

		ImGui_ImplToaster_NewFrame(p_app.frameIndex);
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		ImGui::Render();

		const gpu::RenderingInfo rendering_info{
			.renderArea = knl::window_get_size(p_app.window),
			.colourAttachments = {
				gpu::RenderingAttachmentInfo{
					.clearValue = gpu::ClearColourValue{1.0f, 0.0f, 1.0f, 1.0f},
					.renderTarget = swapchain_target,
					.resolveTarget = nullptr,
					.usageOp = gpu::EAttachmentUsageOP::eClearStore,
					.resolveMode = gpu::EAttachmentResolveMode::eNone
				}
			}
		};

		gpu::cmd_begin_rendering(p_app.gpuCtx, p_cmd, rendering_info);

		ImGui_ImplToaster_RenderDrawData(ImGui::GetDrawData(), p_cmd);

		gpu::cmd_end_rendering(p_app.gpuCtx, p_cmd);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	auto tste_on_event(Application &p_app, TSTELayer &p_layer, knl::Event &p_event) -> void
	{
	}
}
