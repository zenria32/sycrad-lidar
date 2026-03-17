#include "loading_bar.h"

#include <QBasicTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimerEvent>
#include <QVBoxLayout>

class loading_indicator : public QWidget {
	public:
	explicit loading_indicator(QWidget *parent = nullptr) : QWidget(parent) {
		setObjectName(QStringLiteral("loading_indicator"));
		setAttribute(Qt::WA_TranslucentBackground, true);
		setAttribute(Qt::WA_NoSystemBackground, true);
		setAutoFillBackground(false);
		setFixedSize(18, 18);
	}

	void start() {
		if (!timer.isActive()) {
			timer.start(33, this);
		}
	}

	void stop() {
		if (timer.isActive()) {
			timer.stop();
		}
		angle = 0;
		update();
	}

	protected:
	void timerEvent(QTimerEvent *event) override {
		if (event->timerId() != timer.timerId()) {
			QWidget::timerEvent(event);
			return;
		}
		angle = (angle + 12) % 360;
		update();
	}

	void paintEvent(QPaintEvent *event) override {
		Q_UNUSED(event);

		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setBrush(Qt::NoBrush);

		const QRectF rectangle(2.0, 2.0, width() - 4.0, height() - 4.0);

		QPen background_pen(QColor(255, 255, 255, 28), 2.0);
		painter.setPen(background_pen);
		painter.drawArc(rectangle, 0, 360 * 16);

		QPen foreground_pen(QColor(0, 200, 200, 220), 2.0);
		foreground_pen.setCapStyle(Qt::FlatCap);
		painter.setPen(foreground_pen);
		painter.drawArc(rectangle, -angle * 16, 104 * 16);
	}

	private:
	QBasicTimer timer;
	int angle = 0;
};

loading_bar::loading_bar(QWidget *parent) : QFrame(parent) {
	setObjectName(QStringLiteral("loading_overlay"));
	setAttribute(Qt::WA_TranslucentBackground, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setAutoFillBackground(false);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(16, 16, 16, 16);

	panel = new QWidget(this);
	panel->setObjectName(QStringLiteral("loading_panel"));
	panel->setAttribute(Qt::WA_StyledBackground, true);
	panel->setFixedHeight(44);

	auto *panel_layout = new QHBoxLayout(panel);
	panel_layout->setContentsMargins(14, 11, 14, 11);
	panel_layout->setSpacing(10);

	indicator = new loading_indicator(panel);

	status_label = new QLabel(panel);
	status_label->setObjectName(QStringLiteral("loading_status"));
	status_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	status_label->setWordWrap(false);

	panel_layout->addWidget(indicator, 0, Qt::AlignVCenter);
	panel_layout->addWidget(status_label, 0, Qt::AlignVCenter);

	layout->addWidget(panel, 0, Qt::AlignBottom | Qt::AlignLeft);

	sync_to_parent();
	hide();
}

void loading_bar::sync_to_parent() {
	if (parentWidget()) {
		setGeometry(parentWidget()->rect());
	}
}

void loading_bar::show_loading(const QString &message) {
	sync_to_parent();
	set_message(message);
	indicator->start();
	show();
	raise();
}

void loading_bar::set_progress(int percent) {
	Q_UNUSED(percent);
}

void loading_bar::set_message(const QString &message) {
	if (status_label && status_label->text() != message) {
		status_label->setText(message);
	}
}

void loading_bar::hide_loading() {
	if (!isVisible()) {
		return;
	}

	indicator->stop();
	hide();
}
