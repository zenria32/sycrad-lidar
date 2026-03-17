#include "orbital_camera.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

orbital_camera::orbital_camera() = default;

void orbital_camera::get_bounds(const QVector3D &bounds_min, const QVector3D &bounds_max) {
	const QVector3D extent = bounds_max - bounds_min;
	const float diagonal = extent.length();

	if (diagonal < 1e-6f) {
		report(QStringLiteral(
			"Unable to apply scene bounds, the bounding box diagonal is "
			"approximately zero, "
			"indicating an empty or degenerate point cloud dataset."));
		return;
	}

	near_draw = std::max(diagonal * near_draw_ratio, 0.01f);
	far_draw = diagonal * far_draw_ratio;

	min_distance = std::max(diagonal * min_distance_ratio, 0.05f);
	max_distance = diagonal * max_distance_ratio;

	const QVector3D padding = {extent.x() * bounds_padding,
		extent.y() * bounds_padding, 0.0f};
	pan_min = bounds_min - padding;
	pan_max = bounds_max + padding;

	bounds_valid = true;

	target = (bounds_min + bounds_max) * 0.5f;
	distance = std::clamp(diagonal * distance_ratio, min_distance, max_distance);
	yaw = default_yaw;
	pitch = default_pitch;

	reset_target = target;
	reset_distance = distance;
	reset_yaw = yaw;
	reset_pitch = pitch;

	view_changed = true;
	projection_changed = true;
	orientation_changed = true;
}

void orbital_camera::reset() {
	target = reset_target;
	distance = reset_distance;
	yaw = reset_yaw;
	pitch = reset_pitch;
	view_changed = true;
	orientation_changed = true;
}

void orbital_camera::set_aspect_ratio(float aspect) {
	if (aspect > 0.0f && aspect != aspect_ratio) {
		aspect_ratio = aspect;
		projection_changed = true;
	}
}

void orbital_camera::orbit(float dx, float dy) {
	yaw -= dx * orbit_sensitivity;
	pitch += dy * orbit_sensitivity;

	yaw = std::remainder(yaw, 360.0f);
	pitch = std::clamp(pitch, -pitch_max, pitch_max);

	view_changed = true;
	orientation_changed = true;
}

void orbital_camera::pan(float dx, float dy) {
	if (z_backup_saved) {
		z_backup_start_target = target.z();
		z_backup_elapsed = 0.0f;
		z_backup_state = true;
		z_backup_saved = false;
	}

	const float scale = distance * pan_scale;

	const QVector3D cam_right = right();
	QVector3D cam_forward_xy = QVector3D::crossProduct(world_up(), cam_right);

	if (cam_forward_xy.lengthSquared() < 1e-8f) {
		cam_forward_xy = QVector3D(0.0f, 1.0f, 0.0f);
	} else {
		cam_forward_xy.normalize();
	}

	target += cam_right * (-dx * scale) + cam_forward_xy * (dy * scale);

	apply_bounds();

	view_changed = true;
	orientation_changed = true;
}

void orbital_camera::zoom(float delta) {
	const float factor = std::clamp(1.0f - delta * zoom_step, 0.5f, 2.0f);
	distance = std::clamp(distance * factor, min_distance, max_distance);
	view_changed = true;
	orientation_changed = true;
}

const QMatrix4x4 &orbital_camera::view_matrix() const {
	if (view_changed) {
		recompute_view();
	}
	return cached_view;
}

const QMatrix4x4 &orbital_camera::projection_matrix() const {
	if (projection_changed) {
		recompute_projection();
	}
	return cached_projection;
}

const QMatrix4x4 &orbital_camera::vp_matrix() const {
	const auto &p = projection_matrix();
	const auto &v = view_matrix();
	if (vp_changed) {
		cached_vp = p * v;
		vp_changed = false;
	}
	return cached_vp;
}

QVector3D orbital_camera::eye_position() const {
	if (orientation_changed) {
		recompute_orientation();
	}
	return cached_eye_pos;
}

QVector3D orbital_camera::forward() const {
	if (orientation_changed) {
		recompute_orientation();
	}
	return cached_forward;
}

QVector3D orbital_camera::right() const {
	if (orientation_changed) {
		recompute_orientation();
	}
	return cached_right;
}

void orbital_camera::recompute_view() const {
	if (orientation_changed) {
		recompute_orientation();
	}
	cached_view.setToIdentity();
	cached_view.lookAt(cached_eye_pos, target, world_up());
	view_changed = false;
	vp_changed = true;
}

void orbital_camera::recompute_projection() const {
	cached_projection.setToIdentity();
	cached_projection.perspective(fov, aspect_ratio, near_draw, far_draw);
	projection_changed = false;
	vp_changed = true;
}

void orbital_camera::recompute_orientation() const {
	cached_eye_pos = recompute_eye();

	const QVector3D to_target = target - cached_eye_pos;
	const float length = to_target.length();
	cached_forward =
		(length > 1e-8f) ? to_target / length : QVector3D(0.0f, 1.0f, 0.0f);

	QVector3D right = QVector3D::crossProduct(cached_forward, world_up());
	cached_right = (right.lengthSquared() > 1e-8f) ? right.normalized()
												   : QVector3D(1.0f, 0.0f, 0.0f);

	orientation_changed = false;
}

QVector3D orbital_camera::recompute_eye() const {
	const float yaw_rad = qDegreesToRadians(yaw);
	const float pitch_rad = qDegreesToRadians(pitch);
	const float cos_pitch = qCos(pitch_rad);

	return target + QVector3D(distance * cos_pitch * qCos(yaw_rad), distance * cos_pitch * qSin(yaw_rad), distance * qSin(pitch_rad));
}

void orbital_camera::apply_bounds() {
	if (!bounds_valid) {
		return;
	}

	target.setX(std::clamp(target.x(), pan_min.x(), pan_max.x()));
	target.setY(std::clamp(target.y(), pan_min.y(), pan_max.y()));
}

ray orbital_camera::raycast(const QPointF &cursor_position, const QSize &viewport_size) const {
	const float nx = (static_cast<float>(cursor_position.x()) * 2.0f / static_cast<float>(viewport_size.width())) - 1.0f;
	const float ny = 1.0f - (static_cast<float>(cursor_position.y()) * 2.0f / static_cast<float>(viewport_size.height()));

	const QMatrix4x4 inverse_vp = vp_matrix().inverted();

	const QVector4D near_clip = inverse_vp * QVector4D(nx, ny, -1.0f, 1.0f);
	const QVector4D far_clip = inverse_vp * QVector4D(nx, ny, 1.0f, 1.0f);

	if (qFuzzyIsNull(near_clip.w()) || qFuzzyIsNull(far_clip.w())) {
		return {eye_position(), forward()};
	}

	const QVector3D near_3d = near_clip.toVector3DAffine();
	const QVector3D far_3d = far_clip.toVector3DAffine();

	QVector3D direction = (far_3d - near_3d).normalized();
	if (direction.lengthSquared() < 1e-8f) {
		direction = forward();
	}

	return {near_3d, direction};
}

void orbital_camera::animation_to_target(const QVector3D &new_target, float duration_ms) {
	if (!z_backup_saved) {
		z_backup_value = target.z();
		z_backup_saved = true;
	}

	animation_start_target = target;
	animation_end_target = new_target;
	animation_duration = std::max(duration_ms, 1.0f);
	animation_elapsed = 0.0f;
	animation_state = true;
}

void orbital_camera::advance_animation(float elapsed_ms) {
	if (animation_state) {
		animation_elapsed += elapsed_ms;
		float t = std::clamp(animation_elapsed / animation_duration, 0.0f, 1.0f);
		const float smooth = t * t * (3.0f - 2.0f * t);

		target = animation_start_target * (1.0f - smooth) + animation_end_target * smooth;

		view_changed = true;
		orientation_changed = true;

		if (t >= 1.0f) {
			animation_state = false;
			target = animation_end_target;
		}
	}

	if (z_backup_state) {
		z_backup_elapsed += elapsed_ms;
		float t = std::clamp(z_backup_elapsed / 300.0f, 0.0f, 1.0f);
		const float smooth = t * t * (3.0f - 2.0f * t);

		float current_z = z_backup_start_target + (z_backup_value - z_backup_start_target) * smooth;
		target.setZ(current_z);

		view_changed = true;
		orientation_changed = true;

		if (t >= 1.0f) {
			z_backup_state = false;
			target.setZ(z_backup_value);
		}
	}
}

void orbital_camera::cancel_animation() {
	animation_state = false;
	animation_elapsed = 0.0f;

	z_backup_state = false;
	z_backup_elapsed = 0.0f;
}

void orbital_camera::report(const QString &message) const {
	if (reporter) {
		reporter(message);
	}
}
