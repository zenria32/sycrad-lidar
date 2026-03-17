#pragma once

#include <QPainter>
#include <QPixmap>
#include <QSplashScreen>
#include <QTimer>
#include <QWidget>

class spinner : public QWidget {
	Q_OBJECT

	public:
	explicit spinner(QWidget *parent = nullptr);

	protected:
	void paintEvent(QPaintEvent *event) override;

	private:
	qreal angle = 0.0;
	QTimer *timer = nullptr;
};

class splash_screen : public QSplashScreen {
	Q_OBJECT

	public:
	explicit splash_screen(const QPixmap &pixmap);
	~splash_screen() = default;

	protected:
	void mousePressEvent(QMouseEvent *event) override;

	private:
	void setup_ui();

	spinner *spinner_widget = nullptr;
};
