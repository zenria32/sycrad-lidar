#pragma once

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class notification_widget : public QWidget {
	Q_OBJECT

	public:
	enum class Type { Success,
		Info,
		Warning,
		Error };
	explicit notification_widget(QWidget *parent = nullptr);

	void notify(const QString &message, Type type = Type::Info);

	private:
	void fade_out();

	QLabel *icon_label;
	QLabel *message_label;
	QWidget *notification_color;
	QGraphicsOpacityEffect *opacity;
	QPropertyAnimation *fade_animation;
	QTimer timer;
};
