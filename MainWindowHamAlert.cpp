/*
 * GS-232B Open Control - rotator controller GUI.
 * Copyright (C) 2026 BG5GEB <bg5geb@misakanet.team>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "MainWindow.h"

#include "AppSettings.h"
#include "ControlPanel.h"
#include "hamalert/HamAlertController.h"
#include "hamalert/HamAlertSettingsDialog.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDockWidget>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int MaximumVisibleAlerts = 200;
constexpr int CallSignColumn = 1;

QString alertKey(const HamAlert::Alert &alert)
{
    return QStringList{alert.fullCallsign, alert.callsign, alert.frequency, alert.band,
                       alert.mode, alert.time, alert.entity, alert.spotter, alert.source,
                       alert.triggerComment}
        .join(QChar(0x1f));
}

QString alertSourceLabel(const QString &source)
{
    if (source.compare(QStringLiteral("pskreporter"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("PSK");
    if (source.compare(QStringLiteral("cluster"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("DX");
    return source.left(12).toUpper();
}

QString alertDescription(const HamAlert::Alert &alert)
{
    QStringList details;
    if (!alert.band.isEmpty())
        details.append(QCoreApplication::translate("MainWindow", "Band: %1").arg(alert.band));
    if (!alert.entity.isEmpty())
        details.append(QCoreApplication::translate("MainWindow", "DXCC: %1").arg(alert.entity));
    if (!alert.spotter.isEmpty())
        details.append(QCoreApplication::translate("MainWindow", "Spotter: %1").arg(alert.spotter));
    if (!alert.source.isEmpty())
        details.append(QCoreApplication::translate("MainWindow", "Source: %1").arg(alert.source));
    if (!alert.triggerComment.isEmpty())
        details.append(QCoreApplication::translate("MainWindow", "Alert: %1")
                           .arg(alert.triggerComment));
    return details.join(QLatin1Char('\n'));
}

} // namespace

void MainWindow::createHamAlertMenu(QMenu *external)
{
    m_hamAlert = new HamAlertController(this);
    m_hamAlertTray = new QSystemTrayIcon(QIcon(QStringLiteral(":/branding/logo.png")), this);
    m_hamAlertTray->setToolTip(tr("HamAlert"));
    const auto activateMainWindow = [this] {
        if (isMinimized())
            showNormal();
        else
            show();
        raise();
        activateWindow();
    };
    auto *trayMenu = new QMenu(this);
    trayMenu->addAction(tr("Show main window"), this, activateMainWindow);
    trayMenu->addSeparator();
    trayMenu->addAction(tr("Quit"), this, &QWidget::close);
    m_hamAlertTray->setContextMenu(trayMenu);
    connect(m_hamAlertTray, &QSystemTrayIcon::messageClicked, this, activateMainWindow);
    connect(m_hamAlertTray, &QSystemTrayIcon::activated, this,
            [activateMainWindow](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                    activateMainWindow();
            });

    m_hamAlertDock = new QDockWidget(tr("HamAlert spots"), this);
    m_hamAlertDock->setObjectName(QStringLiteral("hamAlertDock"));
    m_hamAlertDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_hamAlertDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_hamAlertDock->setMinimumWidth(400);

    auto *panel = new QWidget(m_hamAlertDock);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    m_hamAlertStatus = new QLabel(tr("Disabled"), panel);
    m_hamAlertStatus->setObjectName(QStringLiteral("hamAlertStatus"));
    layout->addWidget(m_hamAlertStatus);

    m_hamAlertEmpty = new QLabel(tr("No HamAlert spots received."), panel);
    m_hamAlertEmpty->setObjectName(QStringLiteral("hamAlertEmpty"));
    m_hamAlertEmpty->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_hamAlertEmpty, 1);

    m_hamAlertTable = new QTableWidget(panel);
    m_hamAlertTable->setObjectName(QStringLiteral("hamAlertAlertsTable"));
    m_hamAlertTable->setColumnCount(5);
    m_hamAlertTable->setHorizontalHeaderLabels(
        {tr("Time (UTC)"), tr("Callsign"), tr("MHz"), tr("Mode"), tr("Source")});
    m_hamAlertTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_hamAlertTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_hamAlertTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_hamAlertTable->setAlternatingRowColors(true);
    m_hamAlertTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_hamAlertTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_hamAlertTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_hamAlertTable->verticalHeader()->hide();
    m_hamAlertTable->horizontalHeader()->setStretchLastSection(true);
    m_hamAlertTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_hamAlertTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_hamAlertTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_hamAlertTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_hamAlertTable->hide();
    layout->addWidget(m_hamAlertTable, 1);
    m_hamAlertDock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, m_hamAlertDock);
    m_hamAlertDock->hide();

    connect(m_hamAlert, &HamAlertController::statusChanged, this,
            [this](const QString &status) {
                m_hamAlertStatus->setText(status);
                m_hamAlertStatus->show();
            });
    connect(m_hamAlert, &HamAlertController::streamingChanged, this,
            [this](bool streaming) { m_hamAlertStatus->setVisible(!streaming); });
    connect(m_hamAlert, &HamAlertController::errorOccurred, this,
            [this](const QString &message) { statusBar()->showMessage(message, 15000); });
    connect(m_hamAlert, &HamAlertController::alertReceived,
            this, &MainWindow::appendHamAlert);
    connect(m_hamAlertTable, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
                const QTableWidgetItem *call = m_hamAlertTable->item(row, CallSignColumn);
                if (call)
                    m_control->requestTarget(call->data(Qt::UserRole).toString());
            });

    auto *menu = external->addMenu(tr("HamAlert..."));
    menu->setObjectName(QStringLiteral("hamAlertMenu"));
    m_hamAlertEnableAction = menu->addAction(tr("Enable"));
    m_hamAlertEnableAction->setObjectName(QStringLiteral("hamAlertEnableAction"));
    m_hamAlertEnableAction->setCheckable(true);
    m_hamAlertEnableAction->setToolTip(
        tr("Receive spots from HamAlert triggers with Telnet enabled. Double-click a spot to turn the rotator."));
    auto *configure = menu->addAction(tr("Configure credentials..."),
                                      this, &MainWindow::openHamAlertSettings);
    configure->setObjectName(QStringLiteral("hamAlertConfigureAction"));

    const AppSettings preferences;
    m_hamAlert->setCredentials(preferences.hamAlertUsername(),
                               preferences.hamAlertTelnetPassword());
    if (preferences.hamAlertEnabled()) {
        m_hamAlertTray->setVisible(true);
        const bool enabled = m_hamAlert->setEnabled(true);
        m_hamAlertEnableAction->setChecked(enabled);
        if (enabled) {
            setHamAlertPanelVisible(true);
        } else {
            m_hamAlertTray->setVisible(false);
            const QString error = m_hamAlert->errorString();
            QTimer::singleShot(0, this, [this, error] {
                QMessageBox::warning(this, tr("HamAlert"), error);
            });
        }
    }
    connect(m_hamAlertEnableAction, &QAction::toggled, this, [this](bool enabled) {
        if (!m_hamAlert->setEnabled(enabled)) {
            const QSignalBlocker blocker(m_hamAlertEnableAction);
            m_hamAlertEnableAction->setChecked(false);
            QMessageBox::warning(this, tr("HamAlert"), m_hamAlert->errorString());
            return;
        }
        if (!AppSettings().setHamAlertEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
        m_hamAlertTray->setVisible(enabled);
        setHamAlertPanelVisible(enabled);
    });
}

void MainWindow::openHamAlertSettings()
{
    auto *dialog = new HamAlertSettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this] {
        const AppSettings preferences;
        m_hamAlert->setCredentials(preferences.hamAlertUsername(),
                                   preferences.hamAlertTelnetPassword());
    });
    dialog->open();
}

void MainWindow::appendHamAlert(const HamAlert::Alert &alert)
{
    const QString key = alertKey(alert);
    if (m_hamAlertKeys.contains(key))
        return;

    m_hamAlertKeys.insert(key);
    const int row = 0;
    m_hamAlertTable->insertRow(row);
    const QString target = alert.fullCallsign.isEmpty() ? alert.callsign : alert.fullCallsign;
    const QStringList values{alert.time, target, alert.frequency, alert.mode,
                             alertSourceLabel(alert.source)};
    const QString description = alertDescription(alert);
    for (int column = 0; column < values.size(); ++column) {
        auto *item = new QTableWidgetItem(values.at(column));
        item->setToolTip(description);
        m_hamAlertTable->setItem(row, column, item);
    }
    m_hamAlertTable->item(row, CallSignColumn)->setData(Qt::UserRole, target);
    m_hamAlertTable->item(row, CallSignColumn)->setData(Qt::UserRole + 1, key);
    m_hamAlertTable->setRowHeight(row, 28);
    if (m_hamAlertTable->rowCount() > MaximumVisibleAlerts) {
        const int last = m_hamAlertTable->rowCount() - 1;
        const QTableWidgetItem *oldest = m_hamAlertTable->item(last, CallSignColumn);
        if (oldest) {
            m_hamAlertKeys.remove(oldest->data(Qt::UserRole + 1).toString());
        }
        m_hamAlertTable->removeRow(last);
    }
    m_hamAlertTable->show();
    m_hamAlertEmpty->hide();
    notifyHamAlert(alert);
}

void MainWindow::notifyHamAlert(const HamAlert::Alert &alert)
{
    if (!m_hamAlertTray || !m_hamAlert->isEnabled())
        return;

    if (!QSystemTrayIcon::isSystemTrayAvailable() || !QSystemTrayIcon::supportsMessages()) {
        if (!m_hamAlertNotificationWarningShown) {
            statusBar()->showMessage(
                tr("System notifications are unavailable; spots remain in the HamAlert list."),
                10000);
            m_hamAlertNotificationWarningShown = true;
        }
        return;
    }

    QStringList details;
    details.append(alert.fullCallsign.isEmpty() ? alert.callsign : alert.fullCallsign);
    if (!alert.frequency.isEmpty())
        details.append(tr("%1 MHz").arg(alert.frequency));
    if (!alert.mode.isEmpty())
        details.append(alert.mode);
    if (!alert.band.isEmpty())
        details.append(alert.band);

    m_hamAlertTray->showMessage(tr("New HamAlert spot"), details.join(QStringLiteral(", ")),
                                QSystemTrayIcon::Information);
}

void MainWindow::setHamAlertPanelVisible(bool visible)
{
    const bool wasVisible = m_hamAlertDock->isVisible();
    if (visible == wasVisible)
        return;

    if (visible) {
        m_hamAlertDock->show();
        if (m_windowSizeRestored && !isMaximized() && !isFullScreen())
            resize(width() + m_hamAlertDock->minimumWidth(), height());
    } else {
        const int dockWidth = m_hamAlertDock->width();
        m_hamAlertDock->hide();
        if (m_windowSizeRestored && !isMaximized() && !isFullScreen())
            resize(qMax(minimumWidth(), width() - dockWidth), height());
    }
}
