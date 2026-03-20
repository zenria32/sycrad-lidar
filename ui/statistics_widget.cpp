#include "statistics_widget.h"
#include "lidar_viewport.h"

#include <QFontDatabase>
#include <QLocale>
#include <QPainter>
#include <QPen>
#include <QtMath>

statistics_widget::statistics_widget(QWidget *parent) : QWidget(parent) {
	setObjectName("statistics_widget");
	setFixedSize(widget_width, widget_height);

	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_NoSystemBackground);

	variable_font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	variable_font.setPixelSize(9);
	variable_font.setWeight(QFont::Medium);
	variable_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);

	value_font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	value_font.setPixelSize(10);
	value_font.setWeight(QFont::Bold);

	auto prepare_variables = [this](QStaticText &qstatic) {
		qstatic.setTextFormat(Qt::PlainText);
		qstatic.prepare(QTransform(), variable_font);
	};
	auto prepare_values = [this](QStaticText &qstatic) {
		qstatic.setTextFormat(Qt::PlainText);
		qstatic.prepare(QTransform(), value_font);
	};

	prepare_variables(api_variable);
	prepare_variables(fps_variable);
	prepare_variables(frametime_variable);
	prepare_variables(resolution_variable);
	prepare_variables(points_variable);
	prepare_variables(memory_label);
	prepare_variables(vram_variable);
	prepare_variables(ram_variable);
	prepare_variables(total_variable);

	prepare_values(api_value);
	prepare_values(fps_value);
	prepare_values(frametime_value);
	prepare_values(resolution_value);
	prepare_values(points_value);
	prepare_values(vram_value);
	prepare_values(ram_value);
	prepare_values(total_value);

	last_api_value = api_value.text();
	last_fps_value = fps_value.text();
	last_frametime_value = frametime_value.text();
	last_resolution_value = resolution_value.text();
	last_points_value = points_value.text();
	last_vram_value = vram_value.text();
	last_ram_value = ram_value.text();
	last_total_value = total_value.text();

	timer.setInterval(update_interval);
	connect(&timer, &QTimer::timeout, this, &statistics_widget::update_stats);
	timer.start();
}

void statistics_widget::get_viewport(lidar_viewport *viewport) {
	viewport_ptr = viewport;
}

void statistics_widget::get_graphics_api(const QString &api) {
	const QString display_api = api.isEmpty() ? QStringLiteral("\u2014") : api;
	if (display_api == last_api_value) {
		return;
	}
	last_api_value = display_api;
	api_value.setText(display_api);
	api_value.prepare(QTransform(), value_font);

	update();
}

void statistics_widget::get_point_count(quint64 count) {
	const QString display_points =
		count == 0 ? QStringLiteral("0") : QLocale(QLocale::English).toString(count);
	if (display_points == last_points_value) {
		return;
	}
	last_points_value = display_points;
	points_value.setText(display_points);
	points_value.prepare(QTransform(), value_font);

	update();
}

QString statistics_widget::format_bytes(quint64 bytes) {
	if (bytes == 0) {
		return QStringLiteral("\u2014");
	}
	if (bytes < 1024) {
		return QString::number(bytes) + QStringLiteral(" Byte");
	}
	if (bytes < 1024 * 1024) {
		return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
	}
	if (bytes < 1024ULL * 1024 * 1024) {
		return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
	}
	return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
}

void statistics_widget::update_stats() {
	if (!viewport_ptr) {
		return;
	}

	get_graphics_api(viewport_ptr->get_graphics_api());

	const QString display_fps = QString::number(qRound(viewport_ptr->average_fps()));
	if (display_fps != last_fps_value) {
		last_fps_value = display_fps;
		fps_value.setText(display_fps);
		fps_value.prepare(QTransform(), value_font);
	}

	const double frame_time_ms = 1000.0 / qMax(1.0, viewport_ptr->average_fps());
	const QString display_frametime = QString::number(frame_time_ms, 'f', 1) + QStringLiteral(" ms");
	if (display_frametime != last_frametime_value) {
		last_frametime_value = display_frametime;
		frametime_value.setText(display_frametime);
		frametime_value.prepare(QTransform(), value_font);
	}

	const QSize resolution = viewport_ptr->get_render_resolution();
	const QString display_resolution = resolution.isEmpty() ? QStringLiteral("\u2014") : QStringLiteral("%1 \u00d7 %2").arg(resolution.width()).arg(resolution.height());
	if (display_resolution != last_resolution_value) {
		last_resolution_value = display_resolution;
		resolution_value.setText(display_resolution);
		resolution_value.prepare(QTransform(), value_font);
	}

	const auto mem_info = viewport_ptr->get_memory_info();

	auto update_value = [this](const QString &value, QString &last, QStaticText &qstatic) {
		if (value != last) {
			last = value;
			qstatic.setText(value);
			qstatic.prepare(QTransform(), value_font);
		}
	};

	const QString vram_str = format_bytes(mem_info.vram_size);
	update_value(vram_str, last_vram_value, vram_value);

	const QString ram_str = format_bytes(mem_info.ram_size);
	update_value(ram_str, last_ram_value, ram_value);

	const QString total_str = format_bytes(mem_info.vram_size + mem_info.ram_size);
	update_value(total_str, last_total_value, total_value);

	update();
}

void statistics_widget::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QRectF rectangle(0.5, 0.5, width() - 1.0, height() - 1.0);

	painter.setPen(QPen(border_color, 1.0));
	painter.setBrush(background_color);
	painter.drawRoundedRect(rectangle, border_radius, border_radius);

	const int content_right = width() - padding_right;

	auto draw_row = [&](int y_offset, const QStaticText &variable, const QStaticText &value) {
		const int variable_y = y_offset + (row_height - qCeil(variable.size().height())) / 2;
		painter.setFont(variable_font);
		painter.setPen(variable_color);
		painter.drawStaticText(padding_left, variable_y, variable);

		const int value_y = y_offset + (row_height - qCeil(value.size().height())) / 2;
		const int value_x = content_right - qCeil(value.size().width());
		painter.setFont(value_font);
		painter.setPen(value_color);
		painter.drawStaticText(value_x, value_y, value);
	};

	int y = padding_top;

	draw_row(y, api_variable, api_value);
	y += row_height;
	draw_row(y, fps_variable, fps_value);
	y += row_height;
	draw_row(y, frametime_variable, frametime_value);
	y += row_height;
	draw_row(y, resolution_variable, resolution_value);
	y += row_height;
	draw_row(y, points_variable, points_value);
	y += row_height;

	y += section_gap / 2;
	painter.setPen(QPen(border_color, 0.5));
	painter.drawLine(padding_left, y, content_right, y);
	y += section_gap;

	painter.setFont(variable_font);
	painter.setPen(section_color);
	painter.drawStaticText(padding_left, y + 1, memory_label);
	y += row_height;

	draw_row(y, vram_variable, vram_value);
	y += row_height;
	draw_row(y, ram_variable, ram_value);
	y += row_height;
	draw_row(y, total_variable, total_value);
}
