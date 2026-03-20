#pragma once

#include "abstract_renderer.h"
#include "data_loader.h"
#include "prepare_cmd_buffer.h"
#include "octree_node.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <vector>

class point_cloud_renderer final : public abstract_renderer {
	public:
	static constexpr quint32 uniform_size = 80;
	static constexpr float default_point_size = 3.0f;

	point_cloud_renderer();
	~point_cloud_renderer() override;

	void initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) override;
	void render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) override;
	void release_resources() override;

	void upload_data(data_variants data);
	void upload_octree_data(spatial::octree tree);

	// Metal API: Prepare indexed command buffer before beginPass().
	void dispatch_cull_info(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera);

	void set_point_size(float size);
	float get_point_size() const { return point_pixel_size; }
	data_format get_data_type() const { return active_format; }
	float get_ground_level() const { return data_ground_level; }

	std::size_t point_count() const { return active_point_count; }
	bool have_bounds() const { return bounds_valid; }
	QVector3D bounds_min() const { return data_bounds_min; }
	QVector3D bounds_max() const { return data_bounds_max; }

	struct memory_stats {
		std::size_t vram_size = 0;
		std::size_t ram_size = 0;
		std::size_t octree_nodes_size = 0;
	};
	memory_stats get_memory_stats() const;

	private:
	enum class color_mode {
		intensity = 0,
		rgb = 1,
		fallback = 2
	};

	void build_pipeline(quint32 stride);
	void upload_uniform(const QSize &viewport_size, orbital_camera *camera);

	std::unique_ptr<QRhiBuffer> vertex_buffer;

	data_variants active_data = std::monostate{};
	const void *active_data_ptr = nullptr;
	quint32 pending_byte_size = 0;
	bool pending_upload = false;

	std::size_t active_point_count = 0;
	quint32 active_stride = 0;
	quint32 pipeline_stride = 0;
	quint32 active_vertex_buffer_size = 0;
	float point_pixel_size = default_point_size;

	QVector3D data_bounds_min;
	QVector3D data_bounds_max;
	bool bounds_valid = false;
	float data_ground_level = 0.0f;

	data_format active_format = data_format::unknown;

	QMatrix4x4 last_mvp_matrix;
	bool uniform_changed = true;

	float cached_min_intensity = 0.0f;
	float cached_max_intensity = 1.0f;
	color_mode cached_color_mode = color_mode::fallback;

	prepare_cmd_buffer lod_buffer;
	spatial::octree moctree;

	std::unique_ptr<QRhiBuffer> index_buffer;
	quint32 index_buffer_size = 0;
	bool pending_index_upload = false;
	std::size_t octree_indices_size = 0;

	std::unique_ptr<QRhiBuffer> draw_indexed_command_buffer;
	quint32 command_buffer_capacity = 0;
	quint32 last_draw_count = 0;

	bool culling_enabled = false;
};
