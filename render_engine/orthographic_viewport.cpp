#include "orthographic_viewport.h"

#include <QFile>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>
#include <cstring>

static constexpr quint32 point_cloud_uniform_size = 80;
static constexpr quint32 cuboid_uniform_size = 80;

struct alignas(16) point_cloud_uniform_data {
	float mvp[16];
	float min_intensity;
	float max_intensity;
	float point_size;
	float color_mode;
};
static_assert(sizeof(point_cloud_uniform_data) == point_cloud_uniform_size);

struct alignas(16) cuboid_uniform_data {
	float mvp[16];
	float padding[4];
};
static_assert(sizeof(cuboid_uniform_data) == cuboid_uniform_size);

static constexpr QVector3D face_normals[6] = {
	{ 0.0f,  0.0f, -1.0f}, { 0.0f,  0.0f,  1.0f},
	{-1.0f,  0.0f,  0.0f}, { 1.0f,  0.0f,  0.0f},
	{ 0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f},
};

static constexpr QVector3D vertex_positions[8] = {
	{-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
	{ 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
	{-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
	{ 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
};

static constexpr int face_vertex_map[6][4] = {
	{0, 1, 2, 3}, {4, 5, 6, 7},
	{0, 4, 7, 3}, {1, 5, 6, 2},
	{3, 7, 6, 2}, {0, 4, 5, 1},
};

static constexpr uint32_t face_index_template[36] = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4,
	0, 4, 7, 7, 3, 0,
	1, 5, 6, 6, 2, 1,
	3, 7, 6, 6, 2, 3,
	0, 4, 5, 5, 1, 0,
};

static constexpr uint32_t edge_index_template[24] = {
	0, 1, 1, 2, 2, 3, 3, 0,
	4, 5, 5, 6, 6, 7, 7, 4,
	0, 4, 1, 5, 2, 6, 3, 7,
};

static QShader load_shader_from_resource(const QString &path) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		return {};
	}
	return QShader::fromSerialized(f.readAll());
}

orthographic_viewport::orthographic_viewport(QWidget *parent) : QRhiWidget(parent) {
#if defined(Q_OS_MACOS)
	setApi(Api::Metal);
#elif defined(Q_OS_WIN)
	setApi(Api::Direct3D12);
#endif

	no_data_label = new QLabel(QStringLiteral("No Data"), this);
	no_data_label->setObjectName("no_data_label");
	no_data_label->setAlignment(Qt::AlignCenter);
	no_data_label->setAttribute(Qt::WA_TransparentForMouseEvents);
	no_data_label->show();
}

orthographic_viewport::~orthographic_viewport() = default;

void orthographic_viewport::set_view_axis(view_axis axis) {
	current_axis = axis;
}

void orthographic_viewport::clear(){
	has_data = false;
	active_point_count = 0;
	cuboid_face_count = 0;
	cuboid_edge_count = 0;
	staged_points.clear();
	staged_cuboid_vertices.clear();
	staged_face_indices.clear();
	staged_edge_indices.clear();

	if (no_data_label) {
		no_data_label->show();
	}

	update();
}

void orthographic_viewport::initialize(QRhiCommandBuffer *) {
	if (pipelines_initialized) {
		return;
	}

	QRhi *rhi_ptr = rhi();
	QRhiRenderPassDescriptor *render_pass = renderTarget()->renderPassDescriptor();
	if (!rhi_ptr || !render_pass) {
		return;
	}

	point_cloud_uniform.reset(rhi_ptr->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, point_cloud_uniform_size));
	if (!point_cloud_uniform->create()) {
		return;
	}
	point_cloud_bindings.reset(rhi_ptr->newShaderResourceBindings());
	point_cloud_bindings->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, point_cloud_uniform.get())});
	if (!point_cloud_bindings->create()) {
		return;
	}

	cuboid_uniform.reset(rhi_ptr->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, cuboid_uniform_size));
	if (!cuboid_uniform->create()) {
		return;
	}
	cuboid_bindings.reset(rhi_ptr->newShaderResourceBindings());
	cuboid_bindings->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, cuboid_uniform.get())});
	if (!cuboid_bindings->create()) {
		return;
	}

	build_point_cloud_pipeline(staged_stride > 0 ? staged_stride : 16);
	build_cuboid_pipeline();

	pipelines_initialized = true;
}

void orthographic_viewport::render(QRhiCommandBuffer *command_buffer) {
	const QSize pixel = renderTarget()->pixelSize();
	if (pixel.isEmpty()) {
		return;
	}

	QRhi *rhi_ptr = rhi();
	if (!rhi_ptr || !pipelines_initialized) {
		return;
	}

	QRhiResourceUpdateBatch *updates = rhi_ptr->nextResourceUpdateBatch();

	if (point_cloud_upload_pending && staged_point_count > 0) {
		const auto byte_size = static_cast<quint32>(staged_points.size() * sizeof(float));

		if (staged_stride != active_stride) {
			point_cloud_pipeline.reset();
			build_point_cloud_pipeline(staged_stride);
		}

		if (!point_vertex_buffer || byte_size > point_vertex_buffer_capacity) {
			point_vertex_buffer.reset(rhi_ptr->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, byte_size));
			if (!point_vertex_buffer->create()) {
				point_vertex_buffer.reset();
				return;
			}
			point_vertex_buffer_capacity = byte_size;
		}

		updates->uploadStaticBuffer(point_vertex_buffer.get(), 0, byte_size, staged_points.data());
		active_point_count = staged_point_count;
		active_stride = staged_stride;
		active_min_intensity = staged_min_intensity;
		active_max_intensity = staged_max_intensity;
		point_cloud_upload_pending = false;
	}

	if (cuboid_upload_pending && !staged_cuboid_vertices.empty()) {
		const auto vertex_buffer_size = static_cast<quint32>(staged_cuboid_vertices.size() * sizeof(cuboid_vertex));
		const auto face_index_buffer_size = static_cast<quint32>(staged_face_indices.size() * sizeof(uint32_t));
		const auto edge_index_buffer_size = static_cast<quint32>(staged_edge_indices.size() * sizeof(uint32_t));

		cuboid_vertex_buffer.reset(rhi_ptr->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, vertex_buffer_size));
		if (!cuboid_vertex_buffer->create()) { cuboid_vertex_buffer.reset(); return; }

		cuboid_face_index_buffer.reset(rhi_ptr->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, face_index_buffer_size));
		if (!cuboid_face_index_buffer->create()) { cuboid_face_index_buffer.reset(); return; }

		cuboid_edge_index_buffer.reset(rhi_ptr->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, edge_index_buffer_size));
		if (!cuboid_edge_index_buffer->create()) { cuboid_edge_index_buffer.reset(); return; }

		updates->uploadStaticBuffer(cuboid_vertex_buffer.get(), 0, vertex_buffer_size, staged_cuboid_vertices.data());
		updates->uploadStaticBuffer(cuboid_face_index_buffer.get(), 0, face_index_buffer_size, staged_face_indices.data());
		updates->uploadStaticBuffer(cuboid_edge_index_buffer.get(), 0, edge_index_buffer_size, staged_edge_indices.data());

		cuboid_face_count = static_cast<quint32>(staged_face_indices.size());
		cuboid_edge_count = static_cast<quint32>(staged_edge_indices.size());
		cuboid_upload_pending = false;
	}

	compute_matrices();

	if (point_cloud_uniform && active_point_count > 0) {
		point_cloud_uniform_data ubo{};
		std::memcpy(ubo.mvp, mvp_matrix.constData(), 64);
		ubo.min_intensity = active_min_intensity;
		ubo.max_intensity = active_max_intensity;
		ubo.point_size = 4.0f;
		ubo.color_mode = 0.0f;
		updates->updateDynamicBuffer(point_cloud_uniform.get(), 0, point_cloud_uniform_size, &ubo);
	}

	if (cuboid_uniform && cuboid_face_count > 0) {
		cuboid_uniform_data ubo{};
		std::memcpy(ubo.mvp, mvp_matrix.constData(), 64);
		std::memset(ubo.padding, 0, sizeof(ubo.padding));
		updates->updateDynamicBuffer(cuboid_uniform.get(), 0, cuboid_uniform_size, &ubo);
	}

	command_buffer->beginPass(renderTarget(),
		QColor::fromRgbF(0.11f, 0.11f, 0.11f, 1.0f),
		{1.0f, 0}, updates);

	if (has_data && point_cloud_pipeline && point_vertex_buffer && active_point_count > 0) {
		command_buffer->setGraphicsPipeline(point_cloud_pipeline.get());
		command_buffer->setViewport(QRhiViewport(0, 0, pixel.width(), pixel.height()));
		command_buffer->setShaderResources(point_cloud_bindings.get());
		const QRhiCommandBuffer::VertexInput vb_binding(point_vertex_buffer.get(), 0);
		command_buffer->setVertexInput(0, 1, &vb_binding);
		command_buffer->draw(active_point_count);
	}

	if (has_data && cuboid_face_pipeline && cuboid_vertex_buffer && cuboid_face_index_buffer && cuboid_face_count > 0) {
		command_buffer->setGraphicsPipeline(cuboid_face_pipeline.get());
		command_buffer->setViewport(QRhiViewport(0, 0, pixel.width(), pixel.height()));
		command_buffer->setShaderResources(cuboid_bindings.get());
		const QRhiCommandBuffer::VertexInput cvb(cuboid_vertex_buffer.get(), 0);
		command_buffer->setVertexInput(0, 1, &cvb, cuboid_face_index_buffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
		command_buffer->drawIndexed(cuboid_face_count);
	}

	if (has_data && cuboid_edge_pipeline && cuboid_vertex_buffer && cuboid_edge_index_buffer && cuboid_edge_count > 0) {
		command_buffer->setGraphicsPipeline(cuboid_edge_pipeline.get());
		command_buffer->setViewport(QRhiViewport(0, 0, pixel.width(), pixel.height()));
		command_buffer->setShaderResources(cuboid_bindings.get());
		const QRhiCommandBuffer::VertexInput cvb(cuboid_vertex_buffer.get(), 0);
		command_buffer->setVertexInput(0, 1, &cvb, cuboid_edge_index_buffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
		command_buffer->drawIndexed(cuboid_edge_count);
	}

	command_buffer->endPass();
}

void orthographic_viewport::releaseResources() {
	point_cloud_pipeline.reset();
	cuboid_face_pipeline.reset();
	cuboid_edge_pipeline.reset();
	point_vertex_buffer.reset();
	cuboid_vertex_buffer.reset();
	cuboid_face_index_buffer.reset();
	cuboid_edge_index_buffer.reset();
	point_cloud_uniform.reset();
	point_cloud_bindings.reset();
	cuboid_uniform.reset();
	cuboid_bindings.reset();
	pipelines_initialized = false;
}

void orthographic_viewport::resizeEvent(QResizeEvent *event) {
	QRhiWidget::resizeEvent(event);

	if (no_data_label) {
		no_data_label->setGeometry(rect());
	}
}

void orthographic_viewport::build_point_cloud_pipeline(quint32 stride) {
	QRhi *rhi_ptr = rhi();
	QRhiRenderPassDescriptor *render_pass = renderTarget()->renderPassDescriptor();
	if (!rhi_ptr || !render_pass || !point_cloud_bindings) {
		return;
	}

	const QShader vert = load_shader_from_resource(QStringLiteral(":/shaders/point_cloud.vert.qsb"));
	const QShader frag = load_shader_from_resource(QStringLiteral(":/shaders/point_cloud.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		return;
	}

	point_cloud_pipeline.reset(rhi_ptr->newGraphicsPipeline());
	point_cloud_pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});

	QRhiVertexInputLayout layout;
	layout.setBindings({{stride}});
	layout.setAttributes({
		{0, 0, QRhiVertexInputAttribute::Float3, 0},
		{0, 1, QRhiVertexInputAttribute::Float, 3 * sizeof(float)}
	});

	point_cloud_pipeline->setVertexInputLayout(layout);
	point_cloud_pipeline->setTopology(QRhiGraphicsPipeline::Points);
	point_cloud_pipeline->setDepthTest(false);
	point_cloud_pipeline->setDepthWrite(false);
	point_cloud_pipeline->setCullMode(QRhiGraphicsPipeline::None);

	QRhiGraphicsPipeline::TargetBlend blend;
	blend.enable = false;
	point_cloud_pipeline->setTargetBlends({blend});

	point_cloud_pipeline->setShaderResourceBindings(point_cloud_bindings.get());
	point_cloud_pipeline->setRenderPassDescriptor(render_pass);

	if (!point_cloud_pipeline->create()) {
		point_cloud_pipeline.reset();
	}

	active_stride = stride;
}

void orthographic_viewport::build_cuboid_pipeline() {
	QRhi *rhi_ptr = rhi();
	QRhiRenderPassDescriptor *render_pass = renderTarget()->renderPassDescriptor();
	if (!rhi_ptr || !render_pass || !cuboid_bindings) {
		return;
	}

	const QShader vert = load_shader_from_resource(QStringLiteral(":/shaders/cuboid.vert.qsb"));
	const QShader frag = load_shader_from_resource(QStringLiteral(":/shaders/cuboid.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		return;
	}

	constexpr quint32 stride = sizeof(cuboid_vertex);
	QRhiVertexInputLayout input_layout;
	input_layout.setBindings({{stride}});
	input_layout.setAttributes({
		{0, 0, QRhiVertexInputAttribute::Float3, 0},
		{0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)},
		{0, 2, QRhiVertexInputAttribute::Float4, 6 * sizeof(float)}
	});

	cuboid_face_pipeline.reset(rhi_ptr->newGraphicsPipeline());
	cuboid_face_pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});
	cuboid_face_pipeline->setVertexInputLayout(input_layout);
	cuboid_face_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
	cuboid_face_pipeline->setDepthTest(false);
	cuboid_face_pipeline->setDepthWrite(false);
	cuboid_face_pipeline->setCullMode(QRhiGraphicsPipeline::None);

	QRhiGraphicsPipeline::TargetBlend face_blend;
	face_blend.enable = true;
	face_blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
	face_blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	face_blend.srcAlpha = QRhiGraphicsPipeline::One;
	face_blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	cuboid_face_pipeline->setTargetBlends({face_blend});

	cuboid_face_pipeline->setShaderResourceBindings(cuboid_bindings.get());
	cuboid_face_pipeline->setRenderPassDescriptor(render_pass);

	if (!cuboid_face_pipeline->create()) {
		cuboid_face_pipeline.reset();
	}

	cuboid_edge_pipeline.reset(rhi_ptr->newGraphicsPipeline());
	cuboid_edge_pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});
	cuboid_edge_pipeline->setVertexInputLayout(input_layout);
	cuboid_edge_pipeline->setTopology(QRhiGraphicsPipeline::Lines);
	cuboid_edge_pipeline->setDepthTest(false);
	cuboid_edge_pipeline->setDepthWrite(false);
	cuboid_edge_pipeline->setCullMode(QRhiGraphicsPipeline::None);
	cuboid_edge_pipeline->setLineWidth(2.0f);

	QRhiGraphicsPipeline::TargetBlend edge_blend;
	edge_blend.enable = false;
	cuboid_edge_pipeline->setTargetBlends({edge_blend});

	cuboid_edge_pipeline->setShaderResourceBindings(cuboid_bindings.get());
	cuboid_edge_pipeline->setRenderPassDescriptor(render_pass);

	if (!cuboid_edge_pipeline->create()) {
		cuboid_edge_pipeline.reset();
	}
}

void orthographic_viewport::compute_matrices() {
	const QSize sz = size();
	if (sz.isEmpty()) {
		return;
	}

	const float aspect = static_cast<float>(sz.width()) / static_cast<float>(sz.height());
	const QVector3D extent = (aabb_max - aabb_min) * 0.5f;

	float view_w = 0.0f;
	float view_h = 0.0f;
	QVector3D eye;
	QVector3D up;
	const float cam_distance = 1000.0f;

	switch (current_axis) {
	case view_axis::top:
		view_w = extent.x();
		view_h = extent.y();
		eye = aabb_center + QVector3D(0, 0, -cam_distance);
		up = QVector3D(0, 1, 0);
		break;
	case view_axis::front:
		view_w = extent.y();
		view_h = extent.z();
		eye = aabb_center + QVector3D(cam_distance, 0, 0);
		up = QVector3D(0, 0, 1);
		break;
	case view_axis::side:
		view_w = extent.x();
		view_h = extent.z();
		eye = aabb_center + QVector3D(0, cam_distance, 0);
		up = QVector3D(0, 0, 1);
		break;
	}

	if (view_w < 0.01f) view_w = 1.0f;
	if (view_h < 0.01f) view_h = 1.0f;

	const float data_aspect = view_w / view_h;
	if (aspect > data_aspect) {
		view_w = view_h * aspect;
	} else {
		view_h = view_w / aspect;
	}

	const float margin = 1.1f;
	view_w *= margin;
	view_h *= margin;

	QMatrix4x4 projection;
	projection.ortho(-view_w, view_w, -view_h, view_h, 0.1f, cam_distance * 2.0f);

	QMatrix4x4 view;
	view.lookAt(eye, aabb_center, up);

	mvp_matrix = rhi()->clipSpaceCorrMatrix() * projection * view;
}

void orthographic_viewport::set_data(const std::vector<float> &points, quint32 point_count, quint32 stride,
	float min_intensity, float max_intensity,
	const cuboid &selected_cuboid) {

	staged_points = points;
	staged_point_count = point_count;
	staged_stride = stride;
	staged_min_intensity = min_intensity;
	staged_max_intensity = max_intensity;
	point_cloud_upload_pending = true;

	aabb_center = selected_cuboid.position;
	aabb_min = aabb_center - selected_cuboid.dimension;
	aabb_max = aabb_center + selected_cuboid.dimension;

	staged_cuboid_vertices.clear();
	staged_face_indices.clear();
	staged_edge_indices.clear();

	const QMatrix4x4 rotation(selected_cuboid.rotation.toRotationMatrix());
	const float alpha = 0.25f;
	const float r = 1.0f;
	const float g = 0.85f;
	const float b = 0.0f;

	for (int face = 0; face < 6; ++face) {
		const QVector3D normal(
			rotation(0, 0) * face_normals[face].x() + rotation(0, 1) * face_normals[face].y() + rotation(0, 2) * face_normals[face].z(),
			rotation(1, 0) * face_normals[face].x() + rotation(1, 1) * face_normals[face].y() + rotation(1, 2) * face_normals[face].z(),
			rotation(2, 0) * face_normals[face].x() + rotation(2, 1) * face_normals[face].y() + rotation(2, 2) * face_normals[face].z()
		);

		for (int v = 0; v < 4; ++v) {
			const int vertex_index = face_vertex_map[face][v];
			QVector3D position = vertex_positions[vertex_index];
			position = QVector3D(
				position.x() * selected_cuboid.dimension.x(),
				position.y() * selected_cuboid.dimension.y(),
				position.z() * selected_cuboid.dimension.z()
			);
			position = rotation.map(position) + selected_cuboid.position;

			cuboid_vertex cv{};
			cv.position[0] = position.x();
			cv.position[1] = position.y();
			cv.position[2] = position.z();
			cv.normal[0] = normal.x();
			cv.normal[1] = normal.y();
			cv.normal[2] = normal.z();
			cv.color[0] = r;
			cv.color[1] = g;
			cv.color[2] = b;
			cv.color[3] = alpha;
			staged_cuboid_vertices.push_back(cv);
		}
	}

	for (uint32_t index : face_index_template) {
		staged_face_indices.push_back(index);
	}

	for (uint32_t index : edge_index_template) {
		staged_edge_indices.push_back(index);
	}

	cuboid_upload_pending = true;
	has_data = true;

	if (no_data_label) {
		no_data_label->hide();
	}

	update();
}
