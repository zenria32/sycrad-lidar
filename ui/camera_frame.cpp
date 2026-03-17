#include "camera_frame.h"

camera_frame::camera_frame(const QString &title, QWidget *parent) : QWidget(parent) {
	setObjectName("camera_frame");
	setAttribute(Qt::WA_StyledBackground, true);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setMinimumSize(120, 80);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	title_bar = new QWidget(this);
	title_bar->setObjectName("camera_frame_title");
	title_bar->setFixedHeight(16);
	title_bar->setAttribute(Qt::WA_StyledBackground, true);

	auto *title_layout = new QHBoxLayout(title_bar);
	title_layout->setContentsMargins(0, 0, 2, 0);

	title_name = new QLabel(title, title_bar);
	title_name->setObjectName("camera_frame_title_label");
	title_layout->addWidget(title_name);

	layout->addWidget(title_bar);

	content = new QWidget(this);
	content->setObjectName("camera_frame_content");
	content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(content, 1);
}

void camera_frame::set_title(const QString &title) {
	title_name->setText(title);
}
