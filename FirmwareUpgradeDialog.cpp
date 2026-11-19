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

#include "FirmwareUpgradeDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QVBoxLayout>

FirmwareUpgradeDialog::FirmwareUpgradeDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("firmwareUpgradeDialog"));
    setWindowTitle(tr("Firmware upgrade"));
    setWindowModality(Qt::WindowModal);
    resize(520, 345);
    auto *layout = new QVBoxLayout(this);
    m_port = new QComboBox(this);
    m_port->setObjectName(QStringLiteral("flashPort"));
    for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
        const QString description = port.description().isEmpty()
                                        ? port.portName()
                                        : QStringLiteral("%1 - %2").arg(port.portName(), port.description());
        m_port->addItem(description, port.portName());
    }
    if (m_port->count() == 0)
        m_port->addItem(tr("No serial ports found"), QString());
    auto *portLabel = new QLabel(tr("Bootloader serial port"), this);
    portLabel->setBuddy(m_port);
    layout->addWidget(portLabel);
    layout->addWidget(m_port);
    auto *fileLabel = new QLabel(tr("Firmware image"), this);
    layout->addWidget(fileLabel);
    auto *fileRow = new QHBoxLayout;
    m_file = new QLineEdit(this);
    m_file->setObjectName(QStringLiteral("firmwareFile"));
    m_file->setReadOnly(true);
    m_file->setPlaceholderText(tr("Select a .bin image"));
    fileLabel->setBuddy(m_file);
    auto *browse = new QPushButton(tr("Browse..."), this);
    browse->setObjectName(QStringLiteral("browseFirmware"));
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(browse);
    layout->addLayout(fileRow);
    connect(browse, &QPushButton::clicked, this, &FirmwareUpgradeDialog::selectFirmware);

    auto *parameters = new QFormLayout;
    parameters->addRow(tr("Flash size"), new QLabel(QStringLiteral("4 MB"), this));
    parameters->addRow(tr("Application offset"), new QLabel(QStringLiteral("0x10000"), this));
    parameters->addRow(tr("Application partition"),
                       new QLabel(QStringLiteral("0x300000 (3 MB)"), this));
    parameters->addRow(tr("Upload speed"), new QLabel(QStringLiteral("460800 baud"), this));
    layout->addLayout(parameters);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_flash = buttons->addButton(tr("Flash..."), QDialogButtonBox::ActionRole);
    m_flash->setObjectName(QStringLiteral("flashButton"));
    m_flash->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_file, &QLineEdit::textChanged, this, [this](const QString &file) {
        m_flash->setEnabled(!file.isEmpty() && !m_port->currentData().toString().isEmpty());
    });
    connect(m_port, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        m_flash->setEnabled(!m_file->text().isEmpty() &&
                            !m_port->currentData().toString().isEmpty());
    });
    connect(m_flash, &QPushButton::clicked, this, &FirmwareUpgradeDialog::confirmFlash);
    // NOTE: 参数来自固件 platformio.ini 与 partitions.csv，应用起址 0x10000、容量 0x300000。
    // TODO: 接入烧录工具前校验镜像类型、大小及偏移，沿用固件的 4 MB Flash 和 460800 速率。
}

void FirmwareUpgradeDialog::selectFirmware()
{
    auto *picker = new QFileDialog(this, tr("Select firmware image"));
    picker->setObjectName(QStringLiteral("firmwareFileDialog"));
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->setOption(QFileDialog::DontUseNativeDialog);
    picker->setFileMode(QFileDialog::ExistingFile);
    picker->setNameFilter(tr("Firmware images (*.bin)"));
    picker->setWindowModality(Qt::WindowModal);
    connect(picker, &QFileDialog::fileSelected, m_file, &QLineEdit::setText);
    picker->open();
}

void FirmwareUpgradeDialog::confirmFlash()
{
    const QFileInfo image(m_file->text());
    constexpr qint64 applicationPartitionSize = 0x300000;
    if (!image.exists() || !image.isFile() || image.size() <= 0 ||
        image.size() > applicationPartitionSize) {
        QMessageBox::warning(
            this, tr("Invalid firmware image"),
            tr("Firmware image must be a non-empty .bin file no larger than 3 MB."));
        return;
    }

    auto *confirmation = new QDialog(this);
    confirmation->setObjectName(QStringLiteral("flashConfirmationDialog"));
    confirmation->setAttribute(Qt::WA_DeleteOnClose);
    confirmation->setWindowTitle(tr("Confirm firmware flash"));
    confirmation->setWindowModality(Qt::WindowModal);
    confirmation->resize(430, 175);
    auto *layout = new QVBoxLayout(confirmation);
    auto *question = new QLabel(tr("Write the selected firmware image to %1?")
                                   .arg(m_port->currentText()), confirmation);
    question->setWordWrap(true);
    layout->addWidget(question);
    auto *file = new QLineEdit(m_file->text(), confirmation);
    file->setReadOnly(true);
    layout->addWidget(file);
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(confirmation);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *flash = buttons->addButton(tr("Flash"), QDialogButtonBox::AcceptRole);
    flash->setObjectName(QStringLiteral("confirmFlash"));
    flash->setAutoDefault(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, confirmation, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, confirmation, [this, confirmation] {
        emit flashRequested(m_port->currentData().toString(), m_file->text());
        confirmation->accept();
        accept();
    });
    confirmation->open();
}
