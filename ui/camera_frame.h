#pragma once

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class camera_frame : public QWidget {
	Q_OBJECT
	public:
	explicit camera_frame(const QString &title, QWidget *parent = nullptr);

	void set_title(const QString &title);
	QWidget *content_widget() const { return content; }

	private:
	QWidget *title_bar;
	QLabel *title_name;
	QWidget *content;
};
