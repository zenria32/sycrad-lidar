#include "notification_widget.h"

#include <QIcon>
#include <QStyle>

notification_widget::notification_widget(QWidget *parent) : QWidget(parent) {
	setObjectName("notification_widget");
	setAttribute(Qt::WA_TranslucentBackground, false);
	setAttribute(Qt::WA_StyledBackground, true);
	setFixedSize(300, 80);
	setVisible(false);

	opacity = new QGraphicsOpacityEffect(this);
	opacity->setOpacity(0.0);
	setGraphicsEffect(opacity);

	fade_animation = new QPropertyAnimation(opacity, "opacity", this);
	fade_animation->setDuration(300);
	fade_animation->setEasingCurve(QEasingCurve::InOutQuad);

	connect(fade_animation, &QPropertyAnimation::finished, this, [this]() {
		if (fade_animation->endValue().toDouble() < 0.1) {
			setVisible(false);
		}
	});

	auto *main_layout = new QHBoxLayout(this);
	main_layout->setContentsMargins(0, 0, 14, 0);
	main_layout->setSpacing(0);

	notification_color = new QWidget(this);
	notification_color->setObjectName("notification_color_bar");
	notification_color->setFixedWidth(4);
	notification_color->setAttribute(Qt::WA_StyledBackground, true);
	main_layout->addWidget(notification_color);

	main_layout->addSpacing(12);

	icon_label = new QLabel(this);
	icon_label->setObjectName("notification_icon");
	icon_label->setFixedSize(18, 18);
	icon_label->setAlignment(Qt::AlignCenter);
	icon_label->setScaledContents(false);
	main_layout->addWidget(icon_label);

	main_layout->addSpacing(10);

	message_label = new QLabel(this);
	message_label->setObjectName("notification_message");
	message_label->setWordWrap(true);
	main_layout->addWidget(message_label, 1);

	timer.setSingleShot(true);
	connect(&timer, &QTimer::timeout, this, &notification_widget::fade_out);
}

void notification_widget::notify(const QString &message, Type type) {
	message_label->setText(message);

	QString icon_path;
	QString type_name;

	switch (type) {
	case Type::Success:
		icon_path = ":/icons/success.svg";
		type_name = "success";
		break;
	case Type::Info:
		icon_path = ":/icons/info.svg";
		type_name = "info";
		break;
	case Type::Warning:
		icon_path = ":/icons/warning.svg";
		type_name = "warning";
		break;
	case Type::Error:
		icon_path = ":/icons/error.svg";
		type_name = "error";
		break;
	}

	QIcon icon(icon_path);
	icon_label->setPixmap(icon.pixmap(QSize(24, 24)));

	notification_color->setProperty("notification_type", type_name);
	notification_color->style()->unpolish(notification_color);
	notification_color->style()->polish(notification_color);

	timer.stop();
	setVisible(true);
	raise();
	fade_animation->stop();
	fade_animation->setStartValue(0.0);
	fade_animation->setEndValue(0.92);
	fade_animation->start();

	timer.start(2000);
}

void notification_widget::fade_out() {
	fade_animation->stop();
	fade_animation->setStartValue(0.92);
	fade_animation->setEndValue(0.0);
	fade_animation->start();
}
