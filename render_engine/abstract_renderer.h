#pragma once

#include <QSize>
#include <QString>
#include <qrhi.h>
#include <qshader.h>

#include <functional>
#include <memory>

class orbital_camera;

using report_error = std::function<void(const QString &message)>;

class abstract_renderer {
	public:
	virtual ~abstract_renderer();

	virtual void initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) = 0;
	virtual void render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) = 0;
	virtual void release_resources();

	void set_visible(bool visible) { is_data_visible = visible; }
	bool is_visible() const { return is_data_visible; }

	void set_reporter(report_error error_handler) { reporter = std::move(error_handler); }

	protected:
	abstract_renderer() = default;
	abstract_renderer(const abstract_renderer &) = delete;
	abstract_renderer &operator=(const abstract_renderer &) = delete;

	bool initialize_abstract(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size);
	bool create_uniform_buffer(quint32 byte_size);
	bool create_shader_bindings(QRhiShaderResourceBinding::StageFlags stages);

	QShader load_shader(const QString &resource_path);

	bool is_ready() const;

	void report(const QString &message) const;

	QRhi *rhi = nullptr;
	QRhiRenderPassDescriptor *render_pass = nullptr;
	QSize viewport_size;

	std::unique_ptr<QRhiGraphicsPipeline> pipeline;
	std::unique_ptr<QRhiBuffer> uniform_buffer;
	std::unique_ptr<QRhiShaderResourceBindings> shader_bindings;

	QRhiResourceUpdateBatch *pending_updates = nullptr;

	private:
	bool is_data_visible = true;
	report_error reporter;
};
