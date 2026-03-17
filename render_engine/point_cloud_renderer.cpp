#include "point_cloud_renderer.h"
#include "orbital_camera.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

struct alignas(16) point_cloud_uniform {
	float mvp[16];
	float min_intensity;
	float max_intensity;
	float point_size;
	float color_mode;
};
static_assert(sizeof(point_cloud_uniform) == point_cloud_renderer::uniform_size);

point_cloud_renderer::point_cloud_renderer() = default;
point_cloud_renderer::~point_cloud_renderer() { release_resources(); }

void point_cloud_renderer::release_resources() {
	abstract_renderer::release_resources();

	vertex_buffer.reset();

	active_data = std::monostate{};
	active_data_ptr = nullptr;
	pending_byte_size = 0;
	pending_upload = false;

	active_point_count = 0;
	active_stride = 0;
	pipeline_stride = 0;
	active_vertex_buffer_size = 0;

	bounds_valid = false;
	data_bounds_min = {};
	data_bounds_max = {};
	data_ground_level = 0.0f;
	active_format = data_format::unknown;

	last_mvp_matrix = {};
	uniform_changed = true;

	moctree = {};
	index_buffer.reset();
	index_buffer_size = 0;
	pending_index_upload = false;
	draw_indexed_command_buffer.reset();
	command_buffer_capacity = 0;
	last_draw_count = 0;
	culling_enabled = false;
}

void point_cloud_renderer::initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) {
	if (!initialize_abstract(rhi, render_pass, viewport_size)) {
		return;
	}
	if (!create_uniform_buffer(uniform_size)) {
		return;
	}
	if (!create_shader_bindings(QRhiShaderResourceBinding::VertexStage)) {
		return;
	}
}

void point_cloud_renderer::set_point_size(float size) {
	const float clamped_size = std::clamp(size, 1.0f, 6.0f);
	if (clamped_size == point_pixel_size) {
		return;
	}

	point_pixel_size = clamped_size;
	uniform_changed = true;
}

void point_cloud_renderer::upload_data(data_variants data) {
	if (std::holds_alternative<std::monostate>(data)) {
		active_data = std::monostate{};
		active_data_ptr = nullptr;
		pending_byte_size = 0;
		pending_upload = false;
		active_point_count = 0;
		active_stride = 0;
		bounds_valid = false;
		data_bounds_min = {};
		data_bounds_max = {};
		data_ground_level = 0.0f;
		active_format = data_format::unknown;
		uniform_changed = true;
		return;
	}

	bool valid = false;

	std::visit(
		[this, &valid](const auto &pending_data) {
			using T = std::decay_t<decltype(pending_data)>;

			if constexpr (!std::is_same_v<T, std::monostate>) {
				if (!pending_data || !pending_data->is_valid()) {
					return;
				}

				const std::size_t byte_size = pending_data->size_in_bytes();
				if (byte_size == 0 || byte_size > std::numeric_limits<quint32>::max()) {
					return;
				}

				active_data_ptr = pending_data->access();
				pending_byte_size = static_cast<quint32>(byte_size);
				active_stride = static_cast<quint32>(pending_data->stride());
				active_point_count = pending_data->point_count;
				data_bounds_min = {pending_data->bounds.min_x, pending_data->bounds.min_y, pending_data->bounds.min_z};
				data_bounds_max = {pending_data->bounds.max_x, pending_data->bounds.max_y, pending_data->bounds.max_z};

				active_format = pending_data->format;
				data_ground_level = 0.0f;

				if constexpr (std::is_same_v<T, std::shared_ptr<las_data>> || std::is_same_v<T, std::shared_ptr<pcd_data>>) {
					data_ground_level = pending_data->ground_level;
				}

				valid = true;
			}
		},
		data);

	if (!valid) {
		report(QStringLiteral("Point cloud renderer: incoming data is invalid and will be ignored."));
		return;
	}

	std::visit(
		[this](const auto &d) {
			using T = std::decay_t<decltype(d)>;
			if constexpr (!std::is_same_v<T, std::monostate>) {
				if (!d) {
					return;
				}
				if constexpr (std::is_same_v<T, std::shared_ptr<kitti_data>> || std::is_same_v<T, std::shared_ptr<nuscenes_data>> || std::is_same_v<T, std::shared_ptr<waymo_data>>) {
					cached_color_mode = color_mode::intensity;
					cached_min_intensity = d->min_intensity;
					cached_max_intensity = d->max_intensity;
				} else if constexpr (std::is_same_v<T, std::shared_ptr<las_data>> || std::is_same_v<T, std::shared_ptr<pcd_data>>) {
					if (d->color_format == coloration_format_for_las_pcd::xyzrgb) {
						cached_color_mode = color_mode::rgb;
					} else if (d->color_format == coloration_format_for_las_pcd::xyzi) {
						cached_color_mode = color_mode::intensity;
						cached_min_intensity = d->min_intensity;
						cached_max_intensity = d->max_intensity;
					} else {
						cached_color_mode = color_mode::fallback;
					}
				}
			}
		},
		data);

	bounds_valid = true;
	active_data = std::move(data);
	pending_upload = true;
	uniform_changed = true;
}

void point_cloud_renderer::build_pipeline(quint32 stride) {
	if (!is_ready()) {
		return;
	}
	if (pipeline && pipeline_stride == stride) {
		return;
	}

	const QShader vert = load_shader(QStringLiteral(":/shaders/point_cloud.vert.qsb"));
	const QShader frag = load_shader(QStringLiteral(":/shaders/point_cloud.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		report(QStringLiteral("Point cloud renderer: shader load failed. "
			"Verify point cloud shader resources are included in the build."));
		pipeline.reset();
		return;
	}

	pipeline.reset(rhi->newGraphicsPipeline());
	pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});

	QRhiVertexInputLayout input_layout;
	input_layout.setBindings({{stride}});
	input_layout.setAttributes(
		{{0, 0, QRhiVertexInputAttribute::Float3, 0},
			{0, 1, QRhiVertexInputAttribute::Float, 3 * sizeof(float)}
		});
	pipeline->setVertexInputLayout(input_layout);

	pipeline->setTopology(QRhiGraphicsPipeline::Points);
	pipeline->setDepthTest(true);
	pipeline->setDepthWrite(true);
	pipeline->setDepthOp(QRhiGraphicsPipeline::Less);
	pipeline->setCullMode(QRhiGraphicsPipeline::None);
	pipeline->setFlags(QRhiGraphicsPipeline::UsesIndirectDraws);

	QRhiGraphicsPipeline::TargetBlend blend;
	blend.enable = false;
	pipeline->setTargetBlends({blend});

	pipeline->setShaderResourceBindings(shader_bindings.get());
	pipeline->setRenderPassDescriptor(render_pass);

	if (!pipeline->create()) {
		report(QStringLiteral("Point cloud renderer: graphics pipeline creation failed."));
		pipeline.reset();
		pipeline_stride = 0;
	} else {
		pipeline_stride = stride;
	}
}

void point_cloud_renderer::upload_uniform(const QSize &viewport_size, orbital_camera *camera) {
	if (!uniform_buffer) {
		return;
	}

	QMatrix4x4 mvp;
	if (camera) {
		mvp = camera->vp_matrix();
	} else {
		QMatrix4x4 projection;
		const float aspect = viewport_size.height() > 0
			? static_cast<float>(viewport_size.width()) /
				static_cast<float>(viewport_size.height())
			: 1.0f;
		projection.perspective(45.0f, aspect, 0.1f, 1000.0f);

		QMatrix4x4 view;
		view.lookAt({0.0f, -50.0f, 20.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
		mvp = projection * view;
	}

	if (!uniform_changed && mvp == last_mvp_matrix) {
		return;
	}

	last_mvp_matrix = mvp;

	point_cloud_uniform uniform_data{};
	std::memcpy(uniform_data.mvp, mvp.constData(), sizeof(uniform_data.mvp));
	uniform_data.min_intensity = cached_min_intensity;
	uniform_data.max_intensity = cached_max_intensity;
	uniform_data.point_size = point_pixel_size;
	uniform_data.color_mode =
		static_cast<float>(static_cast<int>(cached_color_mode));

	if (!pending_updates) {
		pending_updates = rhi->nextResourceUpdateBatch();
	}
	pending_updates->updateDynamicBuffer(uniform_buffer.get(), 0, uniform_size,
		&uniform_data);

	uniform_changed = false;
}

void point_cloud_renderer::upload_octree_data(spatial::octree tree) {
	moctree = std::move(tree);

	const auto index_count = static_cast<uint32_t>(moctree.point_indices.size());

	if (index_count > 0) {
		pending_index_upload = true;
	}

	culling_enabled = (!moctree.empty() && index_count > 0);
}

void point_cloud_renderer::dispatch_cull_info(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) {
	if (!culling_enabled || !rhi || !command_buffer || moctree.empty()) {
		return;
	}

	const QMatrix4x4 &vp = camera ? camera->vp_matrix() : last_mvp_matrix;
	const float fov = camera ? camera->get_fov() : 60.0f;

	lod_buffer.traverse(moctree, vp, viewport_size, fov);
	const auto &commands = lod_buffer.get_draw_commands();
	const auto draw_count = static_cast<uint32_t>(commands.size());

	last_draw_count = draw_count;

	if (draw_count == 0) {
		return;
	}

	if (pending_index_upload) {
		const auto index_bytes = static_cast<quint32>(
			moctree.point_indices.size() * sizeof(uint32_t));

		if (!index_buffer || index_buffer_size != index_bytes) {
			index_buffer.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, index_bytes));
			if (!index_buffer->create()) {
				report(QStringLiteral("Point cloud renderer: index buffer allocation failed."));
				return;
			}
			index_buffer_size = index_bytes;
		}

		QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
		rub->uploadStaticBuffer(index_buffer.get(), 0, index_bytes,
			moctree.point_indices.data());
		command_buffer->resourceUpdate(rub);

		std::vector<uint32_t>().swap(moctree.point_indices);
		pending_index_upload = false;
	}

	if (!draw_indexed_command_buffer || command_buffer_capacity < draw_count) {
		const quint32 new_capacity = draw_count + draw_count / 2 + 64;
		const quint32 alloc_bytes = new_capacity * 20u;

		draw_indexed_command_buffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndirectBuffer, alloc_bytes));

		if (!draw_indexed_command_buffer->create()) {
			report(QStringLiteral("Point cloud renderer: draw command buffer allocation failed."));
			draw_indexed_command_buffer.reset();
			command_buffer_capacity = 0;
			return;
		}
		command_buffer_capacity = new_capacity;
	}

	const quint32 upload_bytes = draw_count * 20u;
	QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
	rub->updateDynamicBuffer(draw_indexed_command_buffer.get(), 0, upload_bytes, commands.data());
	command_buffer->resourceUpdate(rub);

	command_buffer->prepareDrawIndexedIndirect(
		draw_indexed_command_buffer.get(),
		draw_count,
		index_buffer.get(),
		0,
		QRhiCommandBuffer::IndexUInt32,
		QRhiGraphicsPipeline::Points,
		20);
}

void point_cloud_renderer::render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) {
	if (!is_ready() || !is_visible() || !command_buffer) {
		return;
	}

	if (pending_upload) {
		build_pipeline(active_stride);
		if (!pipeline) {
			return;
		}

		if (!vertex_buffer || active_vertex_buffer_size != pending_byte_size) {
			vertex_buffer.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, pending_byte_size));
			if (!vertex_buffer->create()) {
				report(QStringLiteral("Point cloud renderer: vertex buffer allocation failed."));
				return;
			}
			active_vertex_buffer_size = pending_byte_size;
		}

		if (!pending_updates) {
			pending_updates = rhi->nextResourceUpdateBatch();
		}
		pending_updates->uploadStaticBuffer(vertex_buffer.get(), 0, pending_byte_size, active_data_ptr);

		active_data_ptr = nullptr;
		active_data = std::monostate{};
		pending_byte_size = 0;
		pending_upload = false;
	}

	upload_uniform(viewport_size, camera);

	if (pending_updates) {
		command_buffer->resourceUpdate(pending_updates);
		pending_updates = nullptr;
	}

	if (!pipeline || !vertex_buffer || active_point_count == 0) {
		return;
	}

	command_buffer->setGraphicsPipeline(pipeline.get());
	command_buffer->setViewport({0.0f, 0.0f, static_cast<float>(viewport_size.width()), static_cast<float>(viewport_size.height())});
	command_buffer->setShaderResources(shader_bindings.get());

	if (culling_enabled && index_buffer && last_draw_count > 0) {
		const QRhiCommandBuffer::VertexInput vertex_binding(vertex_buffer.get(), 0);
		command_buffer->setVertexInput(0, 1, &vertex_binding, index_buffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
		command_buffer->executeDrawIndexedIndirect();
	} else {
		const QRhiCommandBuffer::VertexInput vertex_binding(vertex_buffer.get(), 0);
		command_buffer->setVertexInput(0, 1, &vertex_binding);
		command_buffer->draw(static_cast<quint32>(active_point_count));
	}
}

point_cloud_renderer::memory_stats
point_cloud_renderer::get_memory_stats() const {
	memory_stats stats;

	if (vertex_buffer) {
		stats.vram_data_size = active_vertex_buffer_size;
	}
	
	stats.ram_octree_size = moctree.nodes.size() * sizeof(spatial::octree_node) + moctree.point_indices.size() * sizeof(uint32_t);
	
	return stats;
}
