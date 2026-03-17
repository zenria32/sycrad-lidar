#pragma once

#include "abstract_renderer.h"

#include <QColor>
#include <QMatrix4x4>
#include <QVector3D>

#include <vector>

class grid_renderer final : public abstract_renderer {
	public:
	static constexpr quint32 uniform_size = 64;
	static constexpr quint32 floats_per_vertex = 6;
	static constexpr quint32 vertex_stride = floats_per_vertex * sizeof(float);

	grid_renderer();
	~grid_renderer() override;

	void initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) override;
	void render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) override;
	void release_resources() override;

	void set_grid_size(float size);
	float get_grid_size() const { return grid_size; }

	void set_grid_spacing(float spacing);

	void set_major_line_color(const QColor &color) { major_line_color = color; }
	void set_minor_line_color(const QColor &color) { minor_line_color = color; }

	QVector3D bounds_min() const { return {-grid_size, -grid_size, 0.0f}; }
	QVector3D bounds_max() const { return {grid_size, grid_size, 0.0f}; }

	private:
	void build_pipeline();
	void generate_geometry();
	void upload_uniform(const QSize &viewport_size, orbital_camera *camera);

	std::unique_ptr<QRhiBuffer> vertex_buffer;

	std::vector<float> vertices;
	quint32 line_count = 0;

	bool geometry_changed = true;
	bool vertex_buffer_uploaded = false;

	float grid_size = 60.0f;
	float grid_spacing = 1.0f;
	float major_spacing = 15.0f;

	QColor major_line_color{80, 80, 80, 255};
	QColor minor_line_color{45, 45, 45, 255};

	QMatrix4x4 last_mvp_matrix;
};
