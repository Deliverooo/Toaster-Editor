#pragma once

#include <toast_kernel/input.hpp>
#include <toast_kernel/events/event.hpp>
#include "tste_common.hpp"

namespace tste
{
	constexpr XMVECTORF32 unitForwardDir{.f{0.0f, 0.0f, -1.0f, 0.0f}};
	constexpr XMVECTORF32 unitRightDir{.f{1.0f, 0.0f, 0.0f, 0.0f}};
	constexpr XMVECTORF32 unitUpDir{.f{0.0f, -1.0f, 0.0f, 0.0f}};

	struct CameraCB
	{
		math::Float4x4 view;
		math::Float4x4 proj;
		math::Float4x4 invProj;

		math::Float4 position;
		math::Float4 frustumPlanes[6u];
	};

	// struct PlayerCamera
	// {
	// 	math::Float4x4 m_projectionMatrix;
	// 	math::Float4x4 m_viewMatrix;
	//
	// 	math::Float3 m_position{0.0f, 2.0f, -2.0f};
	//
	// 	float32 m_distance{2.0f};
	// 	float32 m_minDistance{0.5f};
	// 	float32 m_maxDistance{5.0f};
	//
	// 	float32 m_zoomVelocity{0.0f};
	//
	// 	float32 m_yaw{0.0f};
	// 	float32 m_pitch{0.0f};
	//
	// 	float32 m_targetYaw{0.0f};
	// 	float32 m_targetPitch{0.0f};
	//
	// 	float32 m_fov{90.0f};
	// 	float32 m_aspect{1920.0f / 1080.0f};
	//
	// 	float32 m_zNear{0.1f};
	// 	float32 m_zFar{1000.0f};
	//
	// 	float32 m_speed{10.0f};
	// };
	//
	// auto player_camera_create(PlayerCamera &p_out_camera, float32 p_fov, float32 p_aspec_ratio, float32 p_z_near = 0.1f, float32 p_z_far = 1000.0f) -> void;
	// auto TST_MATH_CALLCONV player_camera_on_update(PlayerCamera &p_camera, const knl::InputContext *p_input_ctx, math::Vector p_target_pos, float32 p_mouse_sensitivity,
	// 											   float32 p_dt) -> void;
	// auto player_camera_on_resize(PlayerCamera &p_camera, float32 p_aspect) -> void;
	class EditorCamera
	{
	public:
		EditorCamera() = default;
		EditorCamera(float32 p_fov, float32 p_aspec_ratio, float32 p_z_near = 0.1f, float32 p_z_far = 1000.0f);

		auto XM_CALLCONV onUpdate(const knl::InputContext *p_input_ctx, float32 p_mouse_sensitivity, float32 p_dt) -> void;
		auto             onResize(float32 p_aspect) -> void;

		auto XM_CALLCONV getViewMatrix() const -> math::Matrix { return XMLoadFloat4x4(&m_viewMatrix); }
		auto XM_CALLCONV getProjectionMatrix() const -> math::Matrix { return XMLoadFloat4x4(&m_projectionMatrix); }

		auto XM_CALLCONV getRotationMatrix() const -> math::Matrix
		{
			return XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_pitch), -XMConvertToRadians(m_yaw), 0.0f);
		}

		// Ignores the pitch
		auto XM_CALLCONV getRotationMatrixYaw() const -> math::Matrix { return XMMatrixRotationRollPitchYaw(0.0f, -XMConvertToRadians(m_yaw), 0.0f); }

		// Rotates the unit direction vectors by the camera's rotation matrix :)
		auto XM_CALLCONV getCameraForward() const -> math::Vector { return XMVector3Transform(unitForwardDir, getRotationMatrix()); }
		auto XM_CALLCONV getCameraForwardXZ() const -> math::Vector { return XMVector3Transform(unitForwardDir, getRotationMatrixYaw()); }

		auto XM_CALLCONV getCameraRight() const -> math::Vector { return XMVector3Transform(unitRightDir, getRotationMatrix()); }
		auto XM_CALLCONV getCameraRightXZ() const -> math::Vector { return XMVector3Transform(unitRightDir, getRotationMatrixYaw()); }

		auto XM_CALLCONV getCameraUp() const -> math::Vector { return XMVector3Transform(unitUpDir, getRotationMatrix()); }

		auto XM_CALLCONV getPosition() const -> math::Vector { return XMLoadFloat3(&m_position); }

		[[nodiscard]] auto XM_CALLCONV getFrustumPlanes() const -> Array<math::Vector, 6u>;

	private:
		auto             _calcProjection() -> void;
		auto XM_CALLCONV _updateView() -> void;
		math::Float4x4   m_projectionMatrix;
		math::Float4x4   m_viewMatrix;

		math::Float3 m_position{0.0f, 2.0f, -2.0f};

		float32 m_zoomVelocity{0.0f};

		float32 m_yaw{0.0f};
		float32 m_pitch{0.0f};

		float32 m_targetYaw{0.0f};
		float32 m_targetPitch{0.0f};

		float32 m_fov{90.0f};
		float32 m_aspect{1920.0f / 1080.0f};

		float32 m_zNear{0.1f};
		float32 m_zFar{1000.0f};

		float32 m_speed{10.0f};
	};
}
