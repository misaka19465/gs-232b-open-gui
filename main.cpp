/*
 * GS-232B Open Control - rotator controller GUI.
 * Copyright (C) 2026 BG5GEB <bg5geb@misakanet.team>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QString>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // NOTE: Qt 5 默认关闭高 DPI；启动前启用并保留分数缩放，延续现有界面的缩放行为。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationVersion(QString::fromLatin1(GS_232B_OPEN_GUI_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/branding/logo.png")));
    QTranslator qtTranslator;
    const QString qtTranslations = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    // NOTE: Qt 5.15.2 的简体中文基础翻译使用旧的 qt 名称，后续发行包使用 qtbase。
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"), qtTranslations) ||
        qtTranslator.load(QLocale::system(), QStringLiteral("qt"), QStringLiteral("_"), qtTranslations))
        app.installTranslator(&qtTranslator);

    QTranslator translator;
    if (QLocale::system().language() == QLocale::Chinese &&
        translator.load(QStringLiteral(":/i18n/gs_232b_open_gui_zh_CN.qm")))
        app.installTranslator(&translator);

    MainWindow window;
    window.show();
    return app.exec();
}
