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

#include "ConnectionDialog.h"
#include "AppSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSerialPortInfo>

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("connectionDialog"));
    setWindowTitle(tr("Connection"));
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(420);
    const AppSettings settings;
    auto *layout = new QGridLayout(this);
    m_serialMode = new QRadioButton(tr("Serial"), this);
    m_serialMode->setObjectName(QStringLiteral("serialMode"));
    m_serialMode->setChecked(true);
    m_serialPort = new QComboBox(this);
    m_serialPort->setObjectName(QStringLiteral("serialPort"));
    m_serialPort->setEditable(true);
    m_serialPort->setMinimumContentsLength(8);
    m_serialPort->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    const auto availablePorts = QSerialPortInfo::availablePorts();
    QStringList availablePortNames;
    for (const QSerialPortInfo &port : availablePorts) {
        availablePortNames.append(port.portName());
        m_serialPort->addItem(port.portName(), port.portName());
    }
    if (m_serialPort->count() == 0)
        m_serialPort->addItem(tr("No serial ports found"), QString());
    else
        m_serialPort->setCurrentIndex(m_serialPort->findData(settings.serialPort(availablePortNames)));
    layout->addWidget(m_serialMode, 0, 0);
    layout->addWidget(m_serialPort, 0, 1);

    m_baudRate = new QComboBox(this);
    m_baudRate->setObjectName(QStringLiteral("baudRate"));
    m_baudRate->addItems({QStringLiteral("1200"), QStringLiteral("2400"),
                          QStringLiteral("4800"), QStringLiteral("9600")});
    m_baudRate->setCurrentText(QString::number(settings.baudRate()));
    m_baudRate->setAccessibleName(tr("Baud rate"));
    m_baudRate->setToolTip(tr("Baud rate"));
    layout->addWidget(m_baudRate, 0, 2);

    m_networkMode = new QRadioButton(tr("Network"), this);
    m_networkMode->setObjectName(QStringLiteral("networkMode"));
    m_networkMode->setChecked(settings.networkMode());
    m_address = new QLineEdit(settings.connectionAddress(), this);
    m_address->setObjectName(QStringLiteral("websocketAddress"));
    m_address->setMinimumWidth(140);
    m_address->setAccessibleName(tr("Network address"));
    layout->addWidget(m_networkMode, 1, 0);
    layout->addWidget(m_address, 1, 1, 1, 2);

    m_virtualCom = new QCheckBox(tr("Virtual COM"), this);
    m_virtualCom->setObjectName(QStringLiteral("virtualCom"));
    layout->addWidget(m_virtualCom, 2, 1, 1, 2);
#ifdef Q_OS_WIN
    m_virtualPortOptions = new QWidget(this);
    auto *virtualLayout = new QFormLayout(m_virtualPortOptions);
    virtualLayout->setContentsMargins(0, 0, 0, 0);
    m_virtualApplicationPort = new QComboBox(m_virtualPortOptions);
    m_virtualApplicationPort->setObjectName(QStringLiteral("virtualApplicationPort"));
    m_virtualClientPort = new QComboBox(m_virtualPortOptions);
    m_virtualClientPort->setObjectName(QStringLiteral("virtualClientPort"));
    for (QComboBox *port : {m_virtualApplicationPort, m_virtualClientPort}) {
        port->setEditable(true);
        for (const QSerialPortInfo &info : availablePorts)
            port->addItem(info.portName());
        port->setCurrentIndex(-1);
        port->lineEdit()->setPlaceholderText(tr("Select a COM port"));
    }
    virtualLayout->addRow(tr("Bridge port"), m_virtualApplicationPort);
    virtualLayout->addRow(tr("Client port"), m_virtualClientPort);
    auto *pairHint = new QLabel(tr("Select the two ports of an existing virtual COM pair."),
                               m_virtualPortOptions);
    pairHint->setWordWrap(true);
    virtualLayout->addRow(pairHint);
    layout->addWidget(m_virtualPortOptions, 3, 1, 1, 2);
#endif

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("connectionMessage"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status, 4, 0, 1, 3);

    auto *buttons = new QDialogButtonBox(this);
    m_connect = buttons->addButton(tr("Connect"), QDialogButtonBox::AcceptRole);
    m_connect->setObjectName(QStringLiteral("connectButton"));
    m_connect->setDefault(true);
    auto *cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setAutoDefault(false);
    layout->addWidget(buttons, 5, 0, 1, 3);
    layout->setColumnStretch(1, 1);

    // NOTE: 先恢复全部偏好再绑定保存，自动回退串口时保留原选择，等待设备重新接入。
    connect(m_serialPort, &QComboBox::currentTextChanged, this, [this] {
        reportSettingsResult(AppSettings().setSerialPort(connectionOptions().serialPort));
    });
    connect(m_baudRate, &QComboBox::currentTextChanged, this, [this] {
        reportSettingsResult(AppSettings().setBaudRate(m_baudRate->currentText().toInt()));
    });
    connect(m_address, &QLineEdit::textChanged, this, [this](const QString &address) {
        reportSettingsResult(AppSettings().setConnectionAddress(address));
    });
    connect(m_networkMode, &QRadioButton::toggled, this, [this](bool enabled) {
        reportSettingsResult(AppSettings().setNetworkMode(enabled));
    });
    connect(this, &ConnectionDialog::connectRequested, this, [this] {
        const auto options = connectionOptions();
        if (!isNetworkMode() && !options.serialPort.isEmpty())
            reportSettingsResult(AppSettings().setSerialPort(options.serialPort));
    });
    connect(m_networkMode, &QRadioButton::toggled, this, &ConnectionDialog::updateControls);
    connect(m_virtualCom, &QCheckBox::toggled, this, &ConnectionDialog::updateControls);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::connectRequested);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // NOTE: 连接状态只由真实传输后端回写，连接按钮不伪造成功状态。
    updateControls();
}

void ConnectionDialog::reportSettingsResult(bool saved)
{
    if (!saved)
        QMessageBox::warning(this, tr("Application settings"),
                             tr("Could not save application settings."));
}

bool ConnectionDialog::isConnected() const { return m_connected; }
bool ConnectionDialog::isConnecting() const { return m_connecting; }
bool ConnectionDialog::isNetworkMode() const { return m_networkMode->isChecked(); }
bool ConnectionDialog::isVirtualComEnabled() const
{
    return isNetworkMode() && m_virtualCom->isChecked();
}

Communication::ConnectionOptions ConnectionDialog::connectionOptions(
    Communication::WebSocketEndpoint endpoint) const
{
    Communication::ConnectionOptions options;
    options.kind = isNetworkMode() ? Communication::TransportKind::WebSocket
                                   : Communication::TransportKind::Serial;
    const bool noPortPlaceholder = m_serialPort->currentIndex() == 0 &&
                                   m_serialPort->itemData(0).toString().isEmpty() &&
                                   m_serialPort->currentText() == m_serialPort->itemText(0);
    if (!noPortPlaceholder)
        options.serialPort = m_serialPort->currentText().trimmed();
    options.baudRate = m_baudRate->currentText().toInt();
    options.address = m_address->text();
    options.endpoint = endpoint;
    return options;
}

VirtualSerialOptions ConnectionDialog::virtualSerialOptions() const
{
    VirtualSerialOptions options;
#ifdef Q_OS_WIN
    options.applicationPort = m_virtualApplicationPort->currentText().trimmed();
    options.clientPort = m_virtualClientPort->currentText().trimmed();
    options.baudRate = m_baudRate->currentText().toInt();
#endif
    return options;
}

void ConnectionDialog::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connecting = false;
    m_connected = connected;
    updateControls();
    if (connected)
        accept();
}

void ConnectionDialog::setConnecting(bool connecting)
{
    if (m_connecting == connecting)
        return;
    m_connecting = connecting;
    if (connecting)
        m_error.clear();
    updateControls();
}

void ConnectionDialog::setError(const QString &error)
{
    m_error = error;
    updateControls();
}

void ConnectionDialog::updateControls()
{
    const bool network = isNetworkMode();
    const bool locked = m_connected || m_connecting;
    m_serialMode->setEnabled(!locked);
    m_networkMode->setEnabled(!locked);
    m_serialPort->setEnabled(!locked && !network);
    bool baudRateEnabled = !network;
#ifdef Q_OS_WIN
    baudRateEnabled = baudRateEnabled || isVirtualComEnabled();
    m_virtualApplicationPort->setEnabled(!locked);
    m_virtualClientPort->setEnabled(!locked);
    m_virtualPortOptions->setVisible(isVirtualComEnabled());
#endif
    m_baudRate->setEnabled(!locked && baudRateEnabled);
    m_address->setEnabled(!locked && network);
    m_virtualCom->setEnabled(!locked && network);
    m_connect->setEnabled(!locked);
    m_status->setText(m_connecting ? tr("Connecting...") : m_error);
    m_status->setVisible(!m_status->text().isEmpty());
    emit stateChanged();
}
