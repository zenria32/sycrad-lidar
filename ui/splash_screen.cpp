#include "splash_screen.h"

#include <QApplication>
#include <QPainterPath>

spinner::spinner(QWidget *parent) : QWidget(parent) {
	timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]() {
		angle += 10.0;
		if (angle >= 360.0) {
			angle = 0.0;
		}
		update();
	});
	timer->start(20);
}

void spinner::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	int side = qMin(width(), height());
	painter.translate(width() / 2, height() / 2);
	painter.rotate(angle);

	QPen pen(Qt::white);
	pen.setWidth(4);
	pen.setCapStyle(Qt::RoundCap);
	painter.setPen(pen);

	int r = side / 2 - 5;
	painter.drawArc(-r, -r, 2 * r, 2 * r, 0, 210 * 16);
}

splash_screen::splash_screen(const QPixmap &pixmap)
	: QSplashScreen(pixmap) {
	Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;

#ifdef Q_OS_MACOS
	flags |= Qt::Tool;
	setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif

	setWindowFlags(flags);
	setAttribute(Qt::WA_TranslucentBackground, true);
	setup_ui();
}

void splash_screen::setup_ui() {
	spinner_widget = new spinner(this);
	spinner_widget->resize(25, 25);

	constexpr int margin = 15;

	spinner_widget->move(
		this->width() - spinner_widget->width() - margin,
		this->height() - spinner_widget->height() - margin);
}

void splash_screen::mousePressEvent(QMouseEvent *event) {
	Q_UNUSED(event);
}
