#include "MainWindow.h"
#include "AppSettings.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "wsjtx/WsjtXController.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>

void MainWindow::createWsjtXMenu(QMenu *bridge)
{
    m_wsjtX = new WsjtXController(this);
    const AppSettings preferences;
    m_wsjtX->setStationGrid(preferences.stationGrid());
    m_wsjtX->setAzimuthOffset(preferences.azimuthOffset());
    m_wsjtX->setShortestPathEnabled(preferences.shortestPathEnabled());
    m_wsjtX->setLongPathEnabled(m_control->longPathEnabled());
    connect(m_control, &ControlPanel::longPathEnabledChanged,
            m_wsjtX, &WsjtXController::setLongPathEnabled);
    connect(m_wsjtX, &WsjtXController::commandRequested, this, [this](const QString &command) {
        updateBridgeState();
        if (!m_transport->isOpen() || !m_connection->isConnected() ||
            m_connection->isVirtualComEnabled() || !m_pendingCalibrationCommand.isEmpty() ||
            !m_control->trackingEnabled())
            return;
        m_control->cancelPendingMotion();
        sendControlCommand(command);
    });
    connect(m_wsjtX, &WsjtXController::errorOccurred, this, [this](const QString &message) {
        m_console->appendError(message);
        statusBar()->showMessage(message, 15000);
    });

    auto *menu = bridge->addMenu(tr("WSJT-X / JTDX..."));
    menu->setObjectName(QStringLiteral("wsjtXMenu"));
    auto *enable = menu->addAction(tr("Enable"));
    enable->setObjectName(QStringLiteral("wsjtXEnableAction"));
    enable->setCheckable(true);
    enable->setToolTip(tr("Follow the selected DX station using WSJT-X / JTDX on 127.0.0.1:2237."));

    if (preferences.wsjtXEnabled()) {
        const bool enabled = m_wsjtX->setEnabled(true);
        enable->setChecked(enabled);
        if (!enabled) {
            const QString error = m_wsjtX->errorString();
            QTimer::singleShot(0, this, [this, error] {
                QMessageBox::warning(this, tr("WSJT-X / JTDX"), error);
            });
        }
    }
    connect(enable, &QAction::toggled, this, [this, enable](bool enabled) {
        if (!m_wsjtX->setEnabled(enabled)) {
            const QSignalBlocker blocker(enable);
            enable->setChecked(false);
            QMessageBox::warning(this, tr("WSJT-X / JTDX"), m_wsjtX->errorString());
            return;
        }
        if (!AppSettings().setWsjtXEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
    });
}
