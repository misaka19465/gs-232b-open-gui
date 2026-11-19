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
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "ManagementKeyDialog.h"
#include "RotorModelDialog.h"

#include <QAction>
#include <QJsonDocument>
#include <QTimer>

namespace {

int modelIdFromJson(const QJsonValue &value)
{
    const int id = value.toInt(-1);
    return value.isDouble() && value.toDouble() == id ? id : 0;
}

bool modelSupportsElevation(int modelId)
{
    // NOTE: 与固件 rotor_config.c 的 16 个型号一致；使用已保存型号决定界面轴区。
    return (modelId >= 1 && modelId <= 4) || modelId == 6 || modelId == 7 ||
           modelId == 15 || modelId == 16;
}

} // namespace

void MainWindow::setupRotorModel()
{
    m_rotorModelDialog = new RotorModelDialog(this);
    m_rotorModelTimeoutTimer = new QTimer(this);
    m_rotorModelTimeoutTimer->setObjectName(QStringLiteral("rotorModelTimeoutTimer"));
    m_rotorModelTimeoutTimer->setSingleShot(true);
    m_rotorModelTimeoutTimer->setInterval(10000);
    connect(m_rotorModelDialog, &RotorModelDialog::modelApplyRequested,
            this, &MainWindow::applyRotorModel);
    connect(m_rotorModelDialog, &QDialog::finished, this, [this] {
        // NOTE: 关闭窗口可取消尚未发送的写入；已经发出的命令仍须等设备回读。
        if (m_pendingRotorModelId > 0 && !m_rotorModelWriteSent) {
            finishRotorModelRequest();
            if (m_rotorModelAuthDialog)
                m_rotorModelAuthDialog->reject();
        }
    });
    connect(m_rotorModelTimeoutTimer, &QTimer::timeout, this, [this] {
        const bool writeSent = m_rotorModelWriteSent;
        finishRotorModelRequest(tr("The device did not respond to the model request. Try again."));
        if (writeSent) {
            // NOTE: 超时不代表未保存；先丢弃缓存并回读，避免按旧型号再次写入。
            m_rotorModelDialog->clearDeviceModel();
            setElevationSupported(false);
            m_rotorModelDialog->setStatus(tr("Model write timed out. Reading the saved model again."));
            sendControlCommand(QStringLiteral("!ROTOR GET"));
        }
    });
}

void MainWindow::openRotorModel()
{
    if (!m_rotorModelAction->isEnabled())
        return;
    m_rotorModelDialog->resetSelection();
    m_rotorModelDialog->open();
    if (m_pendingRotorModelId == 0)
        sendControlCommand(QStringLiteral("!ROTOR GET"));
}

void MainWindow::updateRotorModelState()
{
    const bool enabled = m_transport->isOpen() && m_connection->isConnected() &&
                         !m_connection->isVirtualComEnabled();
    m_rotorModelAction->setEnabled(enabled);
    m_rotorModelDialog->setCommandEnabled(enabled);
    if (!enabled) {
        finishRotorModelRequest();
        if (m_rotorModelAuthDialog)
            m_rotorModelAuthDialog->reject();
        m_rotorModelDialog->clearDeviceModel();
        setElevationSupported(false);
        m_rotorModelDialog->setStatus(tr("Connect to the controller to change its rotor model."));
    }
}

void MainWindow::applyRotorModel(int modelId)
{
    if (!m_rotorModelAction->isEnabled() || m_pendingRotorModelId > 0 ||
        !m_rotorModelDialog->isVisible() || m_rotorModelDialog->savedModelId() == 0 ||
        modelId != m_rotorModelDialog->selectedModelId() || modelId < 1 || modelId > 16 ||
        modelId == m_rotorModelDialog->savedModelId())
        return;

    const quint64 request = ++m_rotorModelRequest;
    m_pendingRotorModelId = modelId;
    m_rotorModelDialog->setBusy(true);
    const auto write = [this, request, modelId] {
        // NOTE: 认证响应可能晚于取消或重连；仅当前请求可继续向当前设备写入。
        if (request != m_rotorModelRequest || !m_rotorModelAction->isEnabled())
            return;
        m_rotorModelWriteSent = true;
        m_rotorModelDialog->setStatus(tr("Writing rotor model..."));
        m_rotorModelTimeoutTimer->start();
        const QJsonObject payload{{QStringLiteral("model_id"), modelId}};
        sendControlCommand(QStringLiteral("!ROTOR SET %1")
                               .arg(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact))));
    };
    if (!m_connection->isNetworkMode()) {
        write();
        return;
    }

    m_rotorModelDialog->setStatus(tr("Waiting for management authentication..."));
    m_rotorModelAuthDialog = openManagementKey(write, [this, request] {
        if (request == m_rotorModelRequest)
            finishRotorModelRequest();
    }, m_rotorModelDialog);
    if (m_rotorModelAuthDialog) {
        connect(m_rotorModelAuthDialog, &QDialog::accepted, this, [this, request] {
            // NOTE: 等待用户输入密码不计时；提交后才限制设备认证响应等待时间。
            if (request == m_rotorModelRequest && !m_rotorModelWriteSent)
                m_rotorModelTimeoutTimer->start();
        });
    }
}

void MainWindow::finishRotorModelRequest(const QString &error)
{
    ++m_rotorModelRequest;
    m_pendingRotorModelId = 0;
    m_rotorModelWriteSent = false;
    m_rotorModelTimeoutTimer->stop();
    m_rotorModelDialog->setBusy(false);
    m_rotorModelDialog->setStatus(error);
    if (!error.isEmpty())
        m_console->appendError(error);
}

void MainWindow::handleRotorModelResponse(const QJsonObject &response)
{
    if (!m_rotorModelAction->isEnabled())
        return;
    const QString command = response.value(QStringLiteral("cmd")).toString().toUpper();
    const bool ok = response.value(QStringLiteral("ok")).toBool();
    if (command == QStringLiteral("SESSION AUTH")) {
        if (!ok && m_pendingRotorModelId > 0 && !m_rotorModelWriteSent)
            finishRotorModelRequest(tr("Management authentication failed. The model was not written."));
        return;
    }
    const bool setting = command == QStringLiteral("ROTOR SET");
    if (!setting && command != QStringLiteral("ROTOR GET"))
        return;
    if (!ok) {
        if (setting && m_pendingRotorModelId > 0 && m_rotorModelWriteSent) {
            const QString error = response.value(QStringLiteral("error")).toString();
            if (error == QStringLiteral("BUSY")) {
                finishRotorModelRequest(tr("Stop the rotator and wait for any sequence, upload or calibration to finish, then try again."));
            } else if (error == QStringLiteral("NOT_AUTHORIZED")) {
                finishRotorModelRequest(tr("Management authentication expired. Authenticate again to write the model."));
            } else {
                finishRotorModelRequest(tr("Could not write the rotor model. The saved model has not changed."));
            }
        } else if (!setting && m_pendingRotorModelId == 0) {
            m_rotorModelDialog->setStatus(tr("Could not read the rotor model. Reopen this dialog to try again."));
        }
        return;
    }

    // NOTE: 已保存型号与本次启动的生效型号是两个独立字段，不能用生效型号覆盖保存结果。
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    const int savedId = modelIdFromJson(data.value(QStringLiteral("model_id")));
    const int activeId = modelIdFromJson(data.value(QStringLiteral("active_model_id")));
    if (!m_rotorModelDialog->setDeviceModel(savedId, activeId,
                                           data.value(QStringLiteral("restart_required")).toBool())) {
        if (setting && m_pendingRotorModelId > 0 && m_rotorModelWriteSent) {
            finishRotorModelRequest(tr("The device returned an invalid rotor model. Read the saved model again before retrying."));
            m_rotorModelDialog->clearDeviceModel();
            setElevationSupported(false);
            m_rotorModelDialog->setStatus(tr("The device returned an invalid rotor model. Read the saved model again before retrying."));
            sendControlCommand(QStringLiteral("!ROTOR GET"));
        }
        return;
    }
    setElevationSupported(modelSupportsElevation(savedId));
    if (setting && m_pendingRotorModelId > 0 && m_rotorModelWriteSent) {
        if (savedId != m_pendingRotorModelId) {
            finishRotorModelRequest(tr("The device did not confirm the requested model. Check the saved model before retrying."));
            return;
        }
        finishRotorModelRequest();
        m_rotorModelDialog->setStatus(tr("Rotor model saved."));
        m_console->appendNotice(tr("Rotor model saved."));
    } else if (m_pendingRotorModelId == 0) {
        m_rotorModelDialog->setStatus({});
    }
}
