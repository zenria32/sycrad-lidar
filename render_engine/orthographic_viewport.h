#pragma once

#include "abstract_renderer.h"
#include "cuboid.h"

#include <QLabel>
#include <QMatrix4x4>
#include <QRhiWidget>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

class orthographic_viewport : public QRhiWidget {
	Q_OBJECT

	public:
	enum class view_axis { top, front, side };

	explicit orthographic_viewport(QWidget *parent = nullptr);
	~orthographic_viewport() override;

	void set_view_axis(view_axis axis);
	void set_data(const std::vector<float> &points, quint32 point_count, quint32 stride, float min_intensity, float max_intensity,
		const cuboid &selected_cuboid,
		const QVector3D &clip_min, const QVector3D &clip_max);

	void clear();

	void set_interaction_enabled(bool enabled) { interaction_enabled = enabled; }

	protected:
	void initialize(QRhiCommandBuffer *cb) override;
	void render(QRhiCommandBuffer *command_buffer) override;
	void releaseResources() override;
	void resizeEvent(QResizeEvent *event) override;

	private:
	void build_point_cloud_pipeline(quint32 stride);
	void build_cuboid_pipeline();
	void compute_matrices();

	view_axis current_axis = view_axis::top;

	bool pipelines_initialized = false;
	bool has_data = false;
	bool interaction_enabled = false;

	QVector3D aabb_min;
	QVector3D aabb_max;
	QVector3D aabb_center;

	QMatrix4x4 mvp_matrix;

	struct cuboid_vertex {
		float position[3];
		float normal[3];
		float color[4];
	};

	std::vector<float> staged_points;
	quint32 staged_point_count = 0;
	quint32 staged_stride = 16;
	float staged_min_intensity = 0.0f;
	float staged_max_intensity = 1.0f;
	bool point_cloud_upload_pending = false;

	std::vector<cuboid_vertex> staged_cuboid_vertices;
	std::vector<uint32_t> staged_face_indices;
	std::vector<uint32_t> staged_edge_indices;
	bool cuboid_upload_pending = false;

	std::unique_ptr<QRhiBuffer> point_cloud_uniform;
	std::unique_ptr<QRhiShaderResourceBindings> point_cloud_bindings;
	std::unique_ptr<QRhiBuffer> cuboid_uniform;
	std::unique_ptr<QRhiShaderResourceBindings> cuboid_bindings;

	std::unique_ptr<QRhiGraphicsPipeline> point_cloud_pipeline;
	std::unique_ptr<QRhiBuffer> point_vertex_buffer;
	quint32 point_vertex_buffer_capacity = 0;
	quint32 active_point_count = 0;
	quint32 active_stride = 16;
	float active_min_intensity = 0.0f;
	float active_max_intensity = 1.0f;

	std::unique_ptr<QRhiGraphicsPipeline> cuboid_face_pipeline;
	std::unique_ptr<QRhiGraphicsPipeline> cuboid_edge_pipeline;
	std::unique_ptr<QRhiBuffer> cuboid_vertex_buffer;
	std::unique_ptr<QRhiBuffer> cuboid_face_index_buffer;
	std::unique_ptr<QRhiBuffer> cuboid_edge_index_buffer;
	quint32 cuboid_face_count = 0;
	quint32 cuboid_edge_count = 0;

	QLabel *no_data_label = nullptr;
};
