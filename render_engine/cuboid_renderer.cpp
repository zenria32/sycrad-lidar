#include <cuboid_renderer.h>
#include <cuboid_manager.h>
#include <orbital_camera.h>

#include <algorithm>
#include <cstring>

struct alignas(16) cuboid_uniform {
    float mvp[16];
    float padding[4];
};
static_assert(sizeof(cuboid_uniform) == cuboid_renderer::uniform_size);

static constexpr uint32_t face_indices_template[36] = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    0, 4, 7, 7, 3, 0,
    1, 5, 6, 6, 2, 1,
    3, 7, 6, 6, 2, 3,
    0, 4, 5, 5, 1, 0,
};

static constexpr uint32_t edge_indices_template[24] = {
    0, 1, 1, 2, 2, 3, 3, 0,
    4, 5, 5, 6, 6, 7, 7, 4,
    0, 4, 1, 5, 2, 6, 3, 7,
};

static constexpr QVector3D vertex_positions[8] = {
    {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
    { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
    {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
    { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
};

static constexpr QVector3D face_normals[6] = {
    { 0.0f,  0.0f, -1.0f}, { 0.0f,  0.0f,  1.0f},
    {-1.0f,  0.0f,  0.0f}, { 1.0f,  0.0f,  0.0f},
    { 0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f},
};

static constexpr int face_vertex_indices[6][4] = {
    {0, 1, 2, 3}, {4, 5, 6, 7},
    {0, 4, 7, 3}, {1, 5, 6, 2},
    {3, 7, 6, 2}, {0, 4, 5, 1},
};

cuboid_renderer::cuboid_renderer() = default;
cuboid_renderer::~cuboid_renderer() { release_resources(); }

void cuboid_renderer::release_resources() {
    abstract_renderer::release_resources();

    face_pipeline.reset();
    edge_pipeline.reset();
    vertex_buffer.reset();
    face_index_buffer.reset();
    edge_index_buffer.reset();

    vertex_buffer_capacity = 0;
    face_index_buffer_capacity = 0;
    edge_index_buffer_capacity = 0;
    face_index_count = 0;
    edge_index_count = 0;
    geometry_changed = true;
    uniform_changed = true;
}

void cuboid_renderer::initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) {
	if (!initialize_abstract(rhi, render_pass, viewport_size)) {
		return;
	}
	if (!create_uniform_buffer(uniform_size)) {
		return;
	}
	if (!create_shader_bindings(QRhiShaderResourceBinding::VertexStage)) {
		return;
	}

	build_face_pipeline();
	build_edge_pipeline();
}

void cuboid_renderer::build_face_pipeline() {
	if (!is_ready()) {
		return;
	}

	const QShader vert = load_shader(QStringLiteral(":/shaders/cuboid.vert.qsb"));
	const QShader frag = load_shader(QStringLiteral(":/shaders/cuboid.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		report(QStringLiteral("Shader load failed for cuboid face pipeline."));
		return;
	}

	face_pipeline.reset(rhi->newGraphicsPipeline());
	face_pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});

	constexpr quint32 stride = sizeof(cuboid_vertex);
	QRhiVertexInputLayout input_layout;
	input_layout.setBindings({{stride}});
	input_layout.setAttributes({
		{0, 0, QRhiVertexInputAttribute::Float3, 0},
		{0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)},
		{0, 2, QRhiVertexInputAttribute::Float4, 6 * sizeof(float)}
	});

	face_pipeline->setVertexInputLayout(input_layout);
	face_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
	face_pipeline->setDepthTest(true);
	face_pipeline->setDepthWrite(false);
	face_pipeline->setCullMode(QRhiGraphicsPipeline::None);

	QRhiGraphicsPipeline::TargetBlend blend;

	blend.enable = true;
	blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
	blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	blend.srcAlpha = QRhiGraphicsPipeline::One;
	blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	face_pipeline->setTargetBlends({blend});

	face_pipeline->setShaderResourceBindings(shader_bindings.get());
	face_pipeline->setRenderPassDescriptor(render_pass);

	if (!face_pipeline->create()) {
		report(QStringLiteral("Cuboid face pipeline creation failed."));
		face_pipeline.reset();
	}
}

void cuboid_renderer::build_edge_pipeline() {
	if (!is_ready()) {
		return;
	}

	const QShader vert = load_shader(QStringLiteral(":/shaders/cuboid.vert.qsb"));
	const QShader frag = load_shader(QStringLiteral(":/shaders/cuboid.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		report(QStringLiteral("Shader load failed for cuboid edge pipeline."));
		return;
	}

	edge_pipeline.reset(rhi->newGraphicsPipeline());
	edge_pipeline->setShaderStages(
		{{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});

	constexpr quint32 stride = sizeof(cuboid_vertex);
	QRhiVertexInputLayout input_layout;
	input_layout.setBindings({{stride}});
	input_layout.setAttributes({
		{0, 0, QRhiVertexInputAttribute::Float3, 0},
		{0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)},
		{0, 2, QRhiVertexInputAttribute::Float4, 6 * sizeof(float)},
	});
	edge_pipeline->setVertexInputLayout(input_layout);

	edge_pipeline->setTopology(QRhiGraphicsPipeline::Lines);
	edge_pipeline->setDepthTest(true);
	edge_pipeline->setDepthWrite(true);
	edge_pipeline->setCullMode(QRhiGraphicsPipeline::None);
	edge_pipeline->setLineWidth(2.0f);

	QRhiGraphicsPipeline::TargetBlend blend;
	blend.enable = false;
	edge_pipeline->setTargetBlends({blend});

	edge_pipeline->setShaderResourceBindings(shader_bindings.get());
	edge_pipeline->setRenderPassDescriptor(render_pass);

	if (!edge_pipeline->create()) {
		report(QStringLiteral("Cuboid edge pipeline creation failed."));
		edge_pipeline.reset();
	}
}

void cuboid_renderer::generate_cuboid_vertices(const cuboid &c, bool is_selected, std::vector<cuboid_vertex> &output_vertices) const {

	const float alpha = is_selected ? highlight_alpha : default_alpha;
	const float r = is_selected ? 1.0f : 0.85f;
	const float g = is_selected ? 0.85f : 0.75f;
	const float b = is_selected ? 0.0f : 0.0f;

	const QMatrix4x4 rotation_matrix = QMatrix4x4(c.rotation.toRotationMatrix());

	for (int face = 0; face < 6; ++face) {
		const QVector3D normal = QVector3D(
			rotation_matrix(0, 0) * face_normals[face].x() + rotation_matrix(0, 1) * face_normals[face].y() + rotation_matrix(0, 2) * face_normals[face].z(),
			rotation_matrix(1, 0) * face_normals[face].x() + rotation_matrix(1, 1) * face_normals[face].y() + rotation_matrix(1, 2) * face_normals[face].z(),
			rotation_matrix(2, 0) * face_normals[face].x() + rotation_matrix(2, 1) * face_normals[face].y() + rotation_matrix(2, 2) * face_normals[face].z()
		);

		for (int v = 0; v < 4; ++v) {
			const QVector3D local_position = vertex_positions[face_vertex_indices[face][v]];
			const QVector3D scaled = QVector3D(
				local_position.x() * c.dimension.x(),
				local_position.y() * c.dimension.y(),
				local_position.z() * c.dimension.z()
			);
			const QVector3D world_position = c.position + c.rotation.rotatedVector(scaled);

			cuboid_vertex vertex{};
			vertex.position[0] = world_position.x();
			vertex.position[1] = world_position.y();
			vertex.position[2] = world_position.z();
			vertex.normal[0] = normal.x();
			vertex.normal[1] = normal.y();
			vertex.normal[2] = normal.z();
			vertex.color[0] = r;
			vertex.color[1] = g;
			vertex.color[2] = b;
			vertex.color[3] = alpha;

			output_vertices.push_back(vertex);
		}
	}
}

void cuboid_renderer::generate_cuboid_face_indices(uint32_t base_vertex, std::vector<uint32_t> &output_indices) const {
	for (uint32_t index : face_indices_template) {
		output_indices.push_back(base_vertex + index);
	}
}

void cuboid_renderer::generate_cuboid_edge_indices(uint32_t base_vertex, std::vector<uint32_t> &output_indices) const {
	for (uint32_t index : edge_indices_template) {
		output_indices.push_back(base_vertex + index);
	}
}

void cuboid_renderer::build_geometry() {
	if (!cmngr || !rhi) {
		return;
	}

	const auto &all_cuboids = cmngr->get_cuboids();
	const uint32_t cuboid_count = cmngr->count();

	if (cuboid_count == 0) {
		face_index_count = 0;
		edge_index_count = 0;
		geometry_changed = false;
		return;
	}

	std::vector<cuboid_vertex> vertices;
	std::vector<uint32_t> face_indices;
	std::vector<uint32_t> edge_indices;

	vertices.reserve(cuboid_count * vertices_per_cuboid);
	face_indices.reserve(cuboid_count * indices_per_cuboid_face);
	edge_indices.reserve(cuboid_count * 48);

	const uint32_t selected_id = cmngr->get_selected_id();

	for (const auto &c : all_cuboids) {
		const uint32_t base_vertex = static_cast<uint32_t>(vertices.size());
		const bool is_selected = (c.id == selected_id);

		generate_cuboid_vertices(c, is_selected, vertices);
		generate_cuboid_face_indices(base_vertex, face_indices);
		generate_cuboid_edge_indices(base_vertex, edge_indices);
	}

	const auto vertex_bytes = static_cast<quint32>(vertices.size() * sizeof(cuboid_vertex));
	const auto face_bytes = static_cast<quint32>(face_indices.size() * sizeof(uint32_t));
	const auto edge_bytes = static_cast<quint32>(edge_indices.size() * sizeof(uint32_t));

	if (!vertex_buffer || vertex_buffer_capacity < vertex_bytes) {
		const quint32 alloc = vertex_bytes + vertex_bytes / 2 + 1024;
		vertex_buffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, alloc));
		if (!vertex_buffer->create()) {
			report(QStringLiteral("Cuboid vertex buffer allocation failed."));
			return;
		}
		vertex_buffer_capacity = alloc;
	}

	if (!face_index_buffer || face_index_buffer_capacity < face_bytes) {
		const quint32 alloc = face_bytes + face_bytes / 2 + 256;
		face_index_buffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, alloc));
		if (!face_index_buffer->create()) {
			report(QStringLiteral("Cuboid face index buffer allocation failed."));
			return;
		}
		face_index_buffer_capacity = alloc;
	}

	if (!edge_index_buffer || edge_index_buffer_capacity < edge_bytes) {
		const quint32 alloc = edge_bytes + edge_bytes / 2 + 256;
		edge_index_buffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, alloc));
		if (!edge_index_buffer->create()) {
			report(QStringLiteral("Cuboid edge index buffer allocation failed."));
			return;
		}
		edge_index_buffer_capacity = alloc;
	}

	if (!pending_updates) {
		pending_updates = rhi->nextResourceUpdateBatch();
	}

	pending_updates->updateDynamicBuffer(vertex_buffer.get(), 0, vertex_bytes, vertices.data());
	pending_updates->updateDynamicBuffer(face_index_buffer.get(), 0, face_bytes, face_indices.data());
	pending_updates->updateDynamicBuffer(edge_index_buffer.get(), 0, edge_bytes, edge_indices.data());

	face_index_count = static_cast<quint32>(face_indices.size());
	edge_index_count = static_cast<quint32>(edge_indices.size());
	geometry_changed = false;
}

void cuboid_renderer::upload_uniform(const QSize &viewport_size, orbital_camera *camera) {
	if (!uniform_buffer) {
		return;
	}

	QMatrix4x4 mvp;
	if (camera) {
		mvp = camera->vp_matrix();
	} else {
		QMatrix4x4 projection;
		const float aspect = viewport_size.height() > 0 ? static_cast<float>(viewport_size.width()) / static_cast<float>(viewport_size.height()) : 1.0f;
		projection.perspective(45.0f, aspect, 0.1f, 1000.0f);

		QMatrix4x4 view;
		view.lookAt({0.0f, -50.0f, 20.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
		mvp = projection * view;
	}

	if (!uniform_changed && mvp == last_mvp_matrix) {
		return;
	}

	last_mvp_matrix = mvp;

	cuboid_uniform uniform_data{};
	std::memcpy(uniform_data.mvp, mvp.constData(), sizeof(uniform_data.mvp));

	if (!pending_updates) {
		pending_updates = rhi->nextResourceUpdateBatch();
	}
	pending_updates->updateDynamicBuffer(uniform_buffer.get(), 0, uniform_size, &uniform_data);

	uniform_changed = false;
}

void cuboid_renderer::render(QRhiCommandBuffer *command_buffer,
	const QSize &viewport_size, orbital_camera *camera) {
	if (!is_ready() || !is_visible() || !command_buffer || !cmngr) {
		return;
	}

	if (cmngr->count() == 0) {
		return;
	}

	if (geometry_changed) {
		build_geometry();
	}

	upload_uniform(viewport_size, camera);

	if (pending_updates) {
		command_buffer->resourceUpdate(pending_updates);
		pending_updates = nullptr;
	}

	if (face_index_count == 0 && edge_index_count == 0) {
		return;
	}

	const QRhiCommandBuffer::VertexInput vertex_binding(vertex_buffer.get(), 0);

	if (face_pipeline && face_index_count > 0) {
		command_buffer->setGraphicsPipeline(face_pipeline.get());
		command_buffer->setViewport({0.0f, 0.0f, static_cast<float>(viewport_size.width()), static_cast<float>(viewport_size.height())});
		command_buffer->setShaderResources(shader_bindings.get());
		command_buffer->setVertexInput(0, 1, &vertex_binding, face_index_buffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
		command_buffer->drawIndexed(face_index_count);
	}

	if (edge_pipeline && edge_index_count > 0) {
		command_buffer->setGraphicsPipeline(edge_pipeline.get());
		command_buffer->setViewport({0.0f, 0.0f, static_cast<float>(viewport_size.width()), static_cast<float>(viewport_size.height())});
		command_buffer->setShaderResources(shader_bindings.get());
		command_buffer->setVertexInput(0, 1, &vertex_binding, edge_index_buffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
		command_buffer->drawIndexed(edge_index_count);
	}
}
