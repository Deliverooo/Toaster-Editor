#include "toaster_editor/editor_camera.hpp"
#include "toast_kernel/events/key_event.hpp"

#include <cmath>

namespace tste
{
	EditorCamera::EditorCamera(float32 p_fov, float32 p_aspec_ratio, float32 p_z_near, float32 p_z_far) : m_fov(p_fov), m_aspect(p_aspec_ratio), m_zNear(p_z_near),
																										  m_zFar(p_z_far)
	{
		_calcProjection();
		_updateView();
	}

	auto XM_CALLCONV EditorCamera::onUpdate(const knl::InputContext *p_input_ctx, float32 p_mouse_sensitivity, float32 p_dt) -> void
	{
		const math::Float2 mouse_delta{knl::input_get_mouse_delta_pos(p_input_ctx)};

		const float32 delta_scroll{knl::input_get_mouse_scroll(p_input_ctx).y * 5.0f};
		const float32 target_zoom_velocity{delta_scroll * 10.0f};
		const float32 delta_velocity{target_zoom_velocity - m_zoomVelocity};
		m_zoomVelocity = delta_velocity * 0.1f + m_zoomVelocity;

		m_targetYaw   += mouse_delta.x * p_mouse_sensitivity;
		m_targetPitch += mouse_delta.y * p_mouse_sensitivity;

		if (m_targetPitch > 89.0f)
			m_targetPitch = 89.0f;
		if (m_targetPitch < -89.0f)
			m_targetPitch = -89.0f;

		const float32 blend_factor{1.0f - std::expf(-100.0f * p_dt)};
		m_yaw   = std::lerp(m_yaw, m_targetYaw, blend_factor);
		m_pitch = std::lerp(m_pitch, m_targetPitch, blend_factor);

		math::Vector delta_position{XMVectorZero()};
		if (knl::input_is_key_down(p_input_ctx, knl::EKeyCode::eW))
			delta_position += unitForwardDir;
		if (knl::input_is_key_down(p_input_ctx, knl::EKeyCode::eS))
			delta_position -= unitForwardDir;
		if (knl::input_is_key_down(p_input_ctx, knl::EKeyCode::eA))
			delta_position -= unitRightDir;
		if (knl::input_is_key_down(p_input_ctx, knl::EKeyCode::eD))
			delta_position += unitRightDir;

		math::Vector position{XMLoadFloat3(&m_position)};

		position += XMVector3Transform(math::XMVector3NormalizeSafe(delta_position), getRotationMatrix()) * p_dt * 10.0f;

		XMStoreFloat3(&m_position, position);

		_updateView();
	}

	auto EditorCamera::onResize(float32 p_aspect) -> void
	{
		m_aspect = p_aspect;
		_calcProjection();
	}

	auto XM_CALLCONV EditorCamera::getFrustumPlanes() const -> Array<math::Vector, 6u>
	{
		math::Matrix clip_matrix{XMMatrixTranspose(getViewMatrix() * getProjectionMatrix())};

		math::Vector left{clip_matrix.r[3u] + clip_matrix.r[0u]};
		math::Vector right{clip_matrix.r[3u] - clip_matrix.r[0u]};
		math::Vector bottom{clip_matrix.r[3u] + clip_matrix.r[1u]};
		math::Vector top{clip_matrix.r[3u] - clip_matrix.r[1u]};
		math::Vector near{clip_matrix.r[2u]};
		math::Vector far{clip_matrix.r[3u] - clip_matrix.r[2u]};

		Array<math::Vector, 6u> simd_planes{left, right, bottom, top, near, far};

		for (uint32 i{0u}; i < 6u; ++i)
			simd_planes[i] = XMPlaneNormalize(simd_planes[i]);

		return simd_planes;
	}

	auto EditorCamera::_calcProjection() -> void
	{
		float32      cot_half_fov{1.0f / std::tanf(XMConvertToRadians(m_fov) / 2.0f)};
		math::Matrix proj{
			XMMatrixSet(cot_half_fov / m_aspect, 0.0f, 0.0f, 0.0f, 0.0f, cot_half_fov, 0.0f, 0.0f, 0.0f, 0.0f, m_zFar / (m_zFar - m_zNear),
						(-m_zNear * m_zFar) / (m_zFar - m_zNear), 0.0f, 0.0f, 1.0f, 0.0f)
		};

		XMStoreFloat4x4(&m_projectionMatrix, XMMatrixTranspose(proj));
	}

	auto XM_CALLCONV EditorCamera::_updateView() -> void
	{
		math::Matrix view_matrix{XMMatrixLookToLH(getPosition(), getCameraForward(), unitUpDir)};
		XMStoreFloat4x4(&m_viewMatrix, view_matrix);
	}
}
