#pragma once

#include <QFrame>

class QLabel;
class loading_indicator;

class loading_bar final : public QFrame {
	Q_OBJECT

	public:
	explicit loading_bar(QWidget *parent = nullptr);

	void show_loading(const QString &message);
	void set_progress(int percent);
	void set_message(const QString &message);
	void hide_loading();

	private:
	void sync_to_parent();

	QWidget *panel = nullptr;
	QLabel *status_label = nullptr;
	loading_indicator *indicator = nullptr;
};
