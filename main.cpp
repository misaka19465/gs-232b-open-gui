#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/branding/logo.png")));
    QTranslator qtTranslator;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString qtTranslationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          qtTranslationsPath))
        app.installTranslator(&qtTranslator);

    QTranslator translator;
    const QString appTranslation =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringLiteral(":/i18n/gs_232b_open_gui_zh_CN.qm");
#else
        QCoreApplication::applicationDirPath() +
        QStringLiteral("/gs_232b_open_gui_zh_CN.qm");
#endif
    if (QLocale::system().language() == QLocale::Chinese && translator.load(appTranslation))
        app.installTranslator(&translator);

    MainWindow window;
    window.show();
    return app.exec();
}
