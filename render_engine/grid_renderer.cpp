#include "grid_renderer.h"
#include "orbital_camera.h"

#include <cmath>
#include <cstring>

struct alignas(16) grid_uniform {
	float mvp[16];
};
static_assert(sizeof(grid_uniform) == grid_renderer::uniform_size);

grid_renderer::grid_renderer() = default;
grid_renderer::~grid_renderer() {
	release_resources();
}

void grid_renderer::release_resources() {
	abstract_renderer::release_resources();

	vertex_buffer.reset();
	vertices.clear();
	vertices.shrink_to_fit();
	line_count = 0;
	geometry_changed = true;
	vertex_buffer_uploaded = false;
}

void grid_renderer::initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) {
	if (!initialize_abstract(rhi, render_pass, viewport_size)) {
		return;
	}
	if (!create_uniform_buffer(uniform_size)) {
		return;
	}
	if (!create_shader_bindings(QRhiShaderResourceBinding::VertexStage)) {
		return;
	}

	build_pipeline();
}

void grid_renderer::render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) {
	if (!is_ready() || !is_visible()) {
		return;
	}

	if (geometry_changed) {
		generate_geometry();
	}

	const auto required_bytes = static_cast<quint32>(vertices.size() * sizeof(float));

	if (!vertex_buffer || vertex_buffer->size() < required_bytes) {
		vertex_buffer.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, required_bytes));
		if (!vertex_buffer->create()) {
			report(QStringLiteral("Grid renderer: failed to allocate vertex buffer. "
								  "The GPU may not have sufficient memory for the requested grid geometry."));
			return;
		}
		vertex_buffer_uploaded = false;
	}

	if (!vertex_buffer_uploaded && !vertices.empty()) {
		if (!pending_updates) {
			pending_updates = rhi->nextResourceUpdateBatch();
		}

		pending_updates->uploadStaticBuffer(vertex_buffer.get(), 0, required_bytes, vertices.data());
		vertex_buffer_uploaded = true;

		vertices.clear();
		vertices.shrink_to_fit();
	}

	upload_uniform(viewport_size, camera);

	if (pending_updates) {
		command_buffer->resourceUpdate(pending_updates);
		pending_updates = nullptr;
	}

	if (line_count == 0 || !pipeline) {
		return;
	}

	command_buffer->setGraphicsPipeline(pipeline.get());
	command_buffer->setViewport({0.0f, 0.0f, static_cast<float>(viewport_size.width()), static_cast<float>(viewport_size.height())});
	command_buffer->setShaderResources(shader_bindings.get());

	const QRhiCommandBuffer::VertexInput vertex_binding(vertex_buffer.get(), 0);
	command_buffer->setVertexInput(0, 1, &vertex_binding);
	command_buffer->draw(line_count * 2);
}

void grid_renderer::set_grid_size(float size) {
	if (grid_size != size) {
		grid_size = size;
		geometry_changed = true;
	}
}

void grid_renderer::set_grid_spacing(float spacing) {
	if (grid_spacing != spacing) {
		grid_spacing = spacing;
		geometry_changed = true;
	}
}

void grid_renderer::build_pipeline() {
	if (!is_ready()) {
		return;
	}

	const QShader vert = load_shader(QStringLiteral(":/shaders/grid.vert.qsb"));
	const QShader frag = load_shader(QStringLiteral(":/shaders/grid.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		report(QStringLiteral("Grid renderer: one or more shader stages failed to load. "
							  "The grid overlay will not be displayed until valid shaders are provided."));
		return;
	}

	pipeline.reset(rhi->newGraphicsPipeline());
	pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert},{QRhiShaderStage::Fragment, frag}});

	QRhiVertexInputLayout input_layout;
	input_layout.setBindings({{vertex_stride}});
	input_layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
		{0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)}
	});
	pipeline->setVertexInputLayout(input_layout);
	pipeline->setTopology(QRhiGraphicsPipeline::Lines);

	pipeline->setDepthTest(true);
	pipeline->setDepthWrite(true);
	pipeline->setDepthOp(QRhiGraphicsPipeline::Less);
	pipeline->setCullMode(QRhiGraphicsPipeline::None);

	QRhiGraphicsPipeline::TargetBlend blend;
	blend.enable = true;
	blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
	blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	blend.srcAlpha = QRhiGraphicsPipeline::One;
	blend.dstAlpha = QRhiGraphicsPipeline::Zero;
	pipeline->setTargetBlends({blend});

	pipeline->setShaderResourceBindings(shader_bindings.get());
	pipeline->setRenderPassDescriptor(render_pass);

	if (!pipeline->create()) {
		report(QStringLiteral("Grid renderer: graphics pipeline creation failed. "
							  "This may indicate an incompatible shader configuration or unsupported render state for the current graphics API."));
		pipeline.reset();
	}
}

void grid_renderer::generate_geometry() {
	line_count = 0;

	if (grid_spacing <= 0.0f || grid_size <= 0.0f) {
		geometry_changed = false;
		vertex_buffer_uploaded = false;
		return;
	}

	constexpr float z_axis = 0.0f;
	const float half_extent = grid_size;

	const auto major_r = static_cast<float>(major_line_color.redF());
	const auto major_g = static_cast<float>(major_line_color.greenF());
	const auto major_b = static_cast<float>(major_line_color.blueF());
	const auto minor_r = static_cast<float>(minor_line_color.redF());
	const auto minor_g = static_cast<float>(minor_line_color.greenF());
	const auto minor_b = static_cast<float>(minor_line_color.blueF());

	constexpr float x_axis_r = 0.55f, x_axis_g = 0.18f, x_axis_b = 0.18f;
	constexpr float y_axis_r = 0.18f, y_axis_g = 0.55f, y_axis_b = 0.18f;

	const int steps = static_cast<int>(std::round(2.0f * half_extent / grid_spacing));
	const int major_interval = (major_spacing > 0.0f) ? std::max(1, static_cast<int>(std::round(major_spacing / grid_spacing))) : 1;

	const std::size_t floats_needed = static_cast<std::size_t>((steps + 1) * 2 + 2) * 2 * floats_per_vertex;

	vertices.clear();
	vertices.reserve(floats_needed);

	auto push_segment = [this](float x0, float y0, float z0, float r, float g, float b, float x1, float y1, float z1) {
		vertices.push_back(x0);
		vertices.push_back(y0);
		vertices.push_back(z0);
		vertices.push_back(r);
		vertices.push_back(g);
		vertices.push_back(b);
		vertices.push_back(x1);
		vertices.push_back(y1);
		vertices.push_back(z1);
		vertices.push_back(r);
		vertices.push_back(g);
		vertices.push_back(b);

		++line_count;
	};

	for (int step = 0; step <= steps; ++step) {
		const float y = -half_extent + step * grid_spacing;
		const bool major = (step % major_interval) == 0;
		push_segment(-half_extent, y, z_axis,
			major ? major_r : minor_r,
			major ? major_g : minor_g,
			major ? major_b : minor_b,
			half_extent, y, z_axis);
	}

	for (int step = 0; step <= steps; ++step) {
		const float x = -half_extent + step * grid_spacing;
		const bool major = (step % major_interval) == 0;
		push_segment(x, -half_extent, z_axis,
			major ? major_r : minor_r,
			major ? major_g : minor_g,
			major ? major_b : minor_b,
			x, half_extent, z_axis);
	}

	push_segment(-half_extent, 0.0f, z_axis, x_axis_r, x_axis_g, x_axis_b, half_extent, 0.0f, z_axis);

	push_segment(0.0f, -half_extent, z_axis, y_axis_r, y_axis_g, y_axis_b, 0.0f, half_extent, z_axis);

	geometry_changed = false;
	vertex_buffer_uploaded = false;
}

void grid_renderer::upload_uniform(const QSize &viewport_size, orbital_camera *camera) {
	if (!uniform_buffer) {
		return;
	}

	QMatrix4x4 mvp;
	if (camera) {
		mvp = camera->vp_matrix();
	} else {
		QMatrix4x4 projection;
		const float aspect = (viewport_size.height() > 0) ? static_cast<float>(viewport_size.width()) / static_cast<float>(viewport_size.height()) : 1.0f;
		projection.perspective(45.0f, aspect, 0.1f, 1000.0f);

		QMatrix4x4 view;
		view.lookAt({0.0f, -50.0f, 20.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
		mvp = projection * view;
	}

	if (mvp == last_mvp_matrix) {
		return;
	}
	last_mvp_matrix = mvp;

	grid_uniform uniform_buffer_object{};
	std::memcpy(uniform_buffer_object.mvp, mvp.constData(), sizeof(uniform_buffer_object.mvp));

	if (!pending_updates) {
		pending_updates = rhi->nextResourceUpdateBatch();
	}
	pending_updates->updateDynamicBuffer(uniform_buffer.get(), 0, uniform_size, &uniform_buffer_object);
}
