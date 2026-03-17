#include "main_window.h"
#include "splash_screen.h"

#include <QApplication>
#include <QEventLoop>
#include <QFontDatabase>
#include <QPixmap>
#include <QTimer>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	app.setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));

#ifdef Q_OS_MACOS
	QPixmap pixmap(":/splash/1200x750.png");
#else
	QPixmap pixmap(":/splash/800x500.png");
	app.setWindowIcon(QIcon(":/icons/win.ico"));
#endif

	pixmap.setDevicePixelRatio(app.devicePixelRatio());
	splash_screen splash(pixmap);

	splash.show();
	splash.raise();
	splash.activateWindow();

	QEventLoop loop;
	QTimer::singleShot(500, &loop, &QEventLoop::quit);
	loop.exec();

	main_window window;
	splash.finish(&window);
	window.show();

	return app.exec();
}
