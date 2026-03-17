#pragma once

#include <abstract_renderer.h>
#include <ray.h>

#include <QVector3D>
#include <QQuaternion>
#include <QMatrix4x4>

#include <vector>
#include <cstdint>

enum class gizmo_axis { none, x, y, z };
enum class gizmo_mode { move, rotate, scale };

class cuboid_manager;

class gizmo_renderer final : public abstract_renderer {
	public:
	static constexpr quint32 uniform_size = 96;

	static constexpr float world_scale = 2.25f;

	static constexpr float shaft_radius = 0.03f;
	static constexpr float shaft_length = 0.75f;

	static constexpr float cone_radius = 0.08f;
	static constexpr float cone_length = 0.22f;

	static constexpr float cube_half_size = 0.07f;

	static constexpr float arc_radius = 0.65f;
	static constexpr float arc_tube_radius = 0.02f;

	static constexpr float axis_hit_threshold = 0.12f;

	static constexpr int cylinder_segments = 16;
	static constexpr int arc_segments = 24;
	static constexpr int tube_segments = 8;

	gizmo_renderer();
	~gizmo_renderer() override;

	void initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) override;
	void render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) override;
	void release_resources() override;

	void set_gizmo_mode(gizmo_mode mode) { current_mode = mode; }
	gizmo_mode get_gizmo_mode() const { return current_mode; }

	void set_target(const QVector3D &position, const QQuaternion &rotation);

	void set_active(bool active) { is_active = active; }
	bool get_active() const { return is_active; }

	void set_dragging_axis(gizmo_axis axis) { dragging_axis = axis; }
	gizmo_axis get_dragging_axis() const { return dragging_axis; }

	gizmo_axis hit_test(const ray &r) const;

	private:
	struct gizmo_vertex {
		float position[3];
	};

	struct axis_mesh {
		quint32 vertex_offset = 0;
		quint32 vertex_count = 0;
		quint32 index_offset = 0;
		quint32 index_count = 0;
	};

	void build_pipeline();
	void build_move_geometry();
	void build_rotate_geometry();
	void build_scale_geometry();
	void upload_geometry();

	void generate_cylinder(const QVector3D &start, const QVector3D &end, float radius,
		int segments, std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices);

	void generate_cone(const QVector3D &base_center, const QVector3D &tip, float radius,
		int segments, std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices);

	void generate_cube(const QVector3D &center, float half_size,
		std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices);

	void generate_arc(const QVector3D &plane_u, const QVector3D &plane_v,
		float radius, float tube_radius, int arc_segments, int tube_segments,
		std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices);

	void render_axis(QRhiCommandBuffer *command_buffer, const QSize &viewport_size,
		orbital_camera *camera, gizmo_axis axis, const axis_mesh &mesh);

	QVector4D get_axis_color(gizmo_axis axis) const;
	QVector3D get_axis_direction(gizmo_axis axis) const;

	float closest_point_on_axis(const ray &r, const QVector3D &axis_direction) const;
	float closest_point_on_arc(const ray &r, const QVector3D &plane_u, const QVector3D &plane_v, float radius) const;

	gizmo_mode current_mode = gizmo_mode::move;
	bool is_active = false;

	QVector3D target_position;
	QQuaternion target_rotation;

	gizmo_axis dragging_axis = gizmo_axis::none;

	axis_mesh move_x, move_y, move_z;
	axis_mesh rotate_x, rotate_y, rotate_z;
	axis_mesh scale_x, scale_y, scale_z;

	std::unique_ptr<QRhiBuffer> vertex_buffer;
	std::unique_ptr<QRhiBuffer> index_buffer;

	std::vector<gizmo_vertex> all_vertices;
	std::vector<uint32_t> all_indices;
	bool is_geometry_built = false;

	std::unique_ptr<QRhiBuffer> axis_uniform[3];
	std::unique_ptr<QRhiShaderResourceBindings> axis_shader_resource[3];
};
