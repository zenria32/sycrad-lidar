#include "abstract_renderer.h"

#include <QFile>

abstract_renderer::~abstract_renderer() { release_resources(); }

void abstract_renderer::release_resources() {
	pipeline.reset();
	shader_bindings.reset();
	uniform_buffer.reset();
	rhi = nullptr;
	render_pass = nullptr;

	if (pending_updates) {
		pending_updates->release();
		pending_updates = nullptr;
	}
}

bool abstract_renderer::initialize_abstract(QRhi *rhi_instance,
	QRhiRenderPassDescriptor *pass,
	const QSize &size) {
	if (!rhi_instance || !pass) {
		report(QStringLiteral("Renderer initialization failed. The graphics backend (QRhi) or render "
				"pass descriptor is unavailable. "
				"Verify that your GPU driver supports the selected graphics API."));
		return false;
	}

	rhi = rhi_instance;
	render_pass = pass;
	viewport_size = size;
	return true;
}

bool abstract_renderer::is_ready() const {
	return (rhi != nullptr) && (render_pass != nullptr);
}

bool abstract_renderer::create_uniform_buffer(quint32 byte_size) {
	if (!rhi) {
		report(QStringLiteral("Unable to allocate uniform buffer. The graphics "
				"backend has not been initialized. "
				"Ensure the rendering context is active before "
				"creating GPU resources."));
		return false;
	}

	uniform_buffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer, byte_size));

	if (!uniform_buffer->create()) {
		report(QString("Failed to allocate uniform buffer (%1 bytes). "
					   "The GPU may have insufficient available memory or the "
					   "requested size exceeds device limits.").arg(byte_size));
		uniform_buffer.reset();
		return false;
	}

	return true;
}

bool abstract_renderer::create_shader_bindings(QRhiShaderResourceBinding::StageFlags stages) {
	if (!rhi || !uniform_buffer) {
		report(QStringLiteral("Unable to create shader resource bindings: the graphics backend or "
				"uniform buffer is not yet available. "
				"Ensure GPU initialization completed successfully before binding "
				"shader resources."));
		return false;
	}

	shader_bindings.reset(rhi->newShaderResourceBindings());
	shader_bindings->setBindings({QRhiShaderResourceBinding::uniformBuffer(
		0, stages, uniform_buffer.get())});

	if (!shader_bindings->create()) {
		report(QStringLiteral("Failed to create shader resource bindings. "
				"This may indicate a driver compatibility issue or "
				"an invalid binding configuration."));
		shader_bindings.reset();
		return false;
	}

	return true;
}

QShader abstract_renderer::load_shader(const QString &resource_path) {
	QFile file(resource_path);

	if (!file.open(QIODevice::ReadOnly)) {
		report(QString("Cannot open shader file: %1. "
					   "Ensure the shader resources are correctly embedded in the "
					   "application binary.").arg(resource_path));
		return {};
	}

	const QShader shader = QShader::fromSerialized(file.readAll());

	if (!shader.isValid()) {
		report(QString("Invalid or corrupted shader data in: %1. "
					   "The precompiled shader may be incompatible with the "
					   "current graphics API.").arg(resource_path));
	}

	return shader;
}

void abstract_renderer::report(const QString &message) const {
	if (reporter) {
		reporter(message);
	}
}
