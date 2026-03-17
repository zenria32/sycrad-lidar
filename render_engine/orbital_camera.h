#pragma once

#include <ray.h>

#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <QVector3D>

#include <functional>

using report_error = std::function<void(const QString &message)>;

class orbital_camera {
	public:
	static constexpr float orbit_sensitivity = 0.25f;
	static constexpr float pan_scale = 0.002f;
	static constexpr float zoom_step = 0.12f;
	static constexpr float default_fov = 60.0f;
	static constexpr float default_yaw = -45.0f;
	static constexpr float default_pitch = 30.0f;
	static constexpr float default_distance = 40.0f;
	static constexpr float pitch_max = 85.0f;

	orbital_camera();
	~orbital_camera() = default;
	orbital_camera(const orbital_camera &) = default;
	orbital_camera &operator=(const orbital_camera &) = default;

	void set_reporter(report_error error_handler) {
		reporter = std::move(error_handler);
	}

	void get_bounds(const QVector3D &bounds_min, const QVector3D &bounds_max);

	void set_aspect_ratio(float aspect);

	void reset();

	void orbit(float dx, float dy);
	void pan(float dx, float dy);
	void zoom(float delta);

	const QMatrix4x4 &view_matrix() const;
	const QMatrix4x4 &projection_matrix() const;
	const QMatrix4x4 &vp_matrix() const;

	QVector3D eye_position() const;
	QVector3D forward() const;
	QVector3D right() const;

	ray raycast(const QPointF &cursor_position, const QSize &viewport_size) const;

	void animation_to_target(const QVector3D &new_target, float duration_ms = 300.0f);
	bool is_animation_playing() const { return animation_state || z_backup_state; }
	void advance_animation(float elapsed_ms);
	void cancel_animation();


	const QVector3D &get_target() const { return target; }
	float get_distance() const { return distance; }
	float get_yaw() const { return yaw; }
	float get_pitch() const { return pitch; }
	float get_fov() const { return fov; }
	bool is_bounds_valid() const { return bounds_valid; }

	private:
	static constexpr QVector3D world_up() { return {0.0f, 0.0f, 1.0f}; }

	static constexpr float distance_ratio = 1.2f;
	static constexpr float min_distance_ratio = 0.01f;
	static constexpr float max_distance_ratio = 5.0f;
	static constexpr float near_draw_ratio = 0.0001f;
	static constexpr float far_draw_ratio = 10.0f;
	static constexpr float bounds_padding = 0.01f;

	void recompute_view() const;
	void recompute_projection() const;
	void recompute_orientation() const;

	QVector3D recompute_eye() const;

	void apply_bounds();

	void report(const QString &message) const;

	QVector3D target{0.0f, 0.0f, 0.0f};
	float yaw = default_yaw;
	float pitch = default_pitch;
	float distance = default_distance;

	float fov = default_fov;
	float aspect_ratio = 16.0f / 9.0f;
	float near_draw = 0.1f;
	float far_draw = 10000.0f;

	bool bounds_valid = false;
	QVector3D pan_min;
	QVector3D pan_max;
	float min_distance = 0.1f;
	float max_distance = 1000.0f;

	QVector3D reset_target{0.0f, 0.0f, 0.0f};
	float reset_yaw = default_yaw;
	float reset_pitch = default_pitch;
	float reset_distance = default_distance;

	mutable QMatrix4x4 cached_view;
	mutable QMatrix4x4 cached_projection;
	mutable QMatrix4x4 cached_vp;
	mutable bool view_changed = true;
	mutable bool projection_changed = true;
	mutable bool vp_changed = true;

	mutable QVector3D cached_eye_pos;
	mutable QVector3D cached_forward;
	mutable QVector3D cached_right;
	mutable bool orientation_changed = true;

	bool animation_state = false;
	QVector3D animation_start_target;
	QVector3D animation_end_target;
	float animation_duration = 300.0f;
	float animation_elapsed = 0.0f;

	float z_backup_value = 0.0f;
	bool z_backup_saved = false;

	bool z_backup_state = false;
	float z_backup_start_target = 0.0f;
	float z_backup_elapsed = 0.0f;

	report_error reporter;
};
