#include "MainWindow.h"
#include "AppSettings.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "n1mm/N1mmController.h"
#include "n1mm/N1mmNameDialog.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>

void MainWindow::createN1mmMenu(QMenu *bridge)
{
    m_n1mm = new N1mmController(this);
    const AppSettings preferences;
    m_n1mm->setRotorName(preferences.n1mmRotorName());
    m_n1mm->setAzimuthOffset(preferences.azimuthOffset());
    m_n1mm->setShortestPathEnabled(preferences.shortestPathEnabled());
    connect(m_n1mm, &N1mmController::commandRequested, this, [this](const QString &command) {
        updateBridgeState();
        if (!m_transport->isOpen() || !m_connection->isConnected() ||
            m_connection->isVirtualComEnabled() || !m_pendingCalibrationCommand.isEmpty() ||
            !m_control->trackingEnabled())
            return;
        m_control->cancelPendingMotion();
        sendControlCommand(command);
    });
    connect(m_n1mm, &N1mmController::errorOccurred, this, [this](const QString &message) {
        m_console->appendError(message);
        statusBar()->showMessage(message, 15000);
    });

    auto *menu = bridge->addMenu(tr("N1MM Logger+..."));
    menu->setObjectName(QStringLiteral("n1mmMenu"));
    m_n1mmEnableAction = menu->addAction(tr("Enable"));
    m_n1mmEnableAction->setObjectName(QStringLiteral("n1mmEnableAction"));
    m_n1mmEnableAction->setCheckable(true);
    auto *name = menu->addAction(tr("Configure name..."), this, &MainWindow::openN1mmName);
    name->setObjectName(QStringLiteral("n1mmNameAction"));

    // NOTE: 启动恢复不回写偏好；监听失败保持界面关闭，并在主窗口显示后提示。
    if (preferences.n1mmEnabled()) {
        const bool enabled = m_n1mm->setEnabled(true);
        m_n1mmEnableAction->setChecked(enabled);
        if (!enabled) {
            const QString error = m_n1mm->errorString();
            QTimer::singleShot(0, this, [this, error] {
                QMessageBox::warning(this, tr("N1MM Logger+"), error);
            });
        }
    }
    connect(m_n1mmEnableAction, &QAction::toggled, this, [this](bool enabled) {
        if (!m_n1mm->setEnabled(enabled)) {
            const QSignalBlocker blocker(m_n1mmEnableAction);
            m_n1mmEnableAction->setChecked(false);
            QMessageBox::warning(this, tr("N1MM Logger+"), m_n1mm->errorString());
            return;
        }
        if (!AppSettings().setN1mmEnabled(enabled))
            QMessageBox::warning(this, tr("Application settings"),
                                 tr("Could not save application settings."));
    });
}

void MainWindow::openN1mmName()
{
    auto *dialog = new N1mmNameDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this] {
        m_n1mm->setRotorName(AppSettings().n1mmRotorName());
    });
    dialog->open();
}
