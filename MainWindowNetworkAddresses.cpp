#include "MainWindow.h"
#include "CommunicationProtocol.h"
#include "ConnectionDialog.h"
#include "NetworkAddresses.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QStringList>

void MainWindow::getIpAddresses()
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    // NOTE: NET GET 与 PROV STATUS 均为只读查询，网络连接也无需管理认证。
    sendManagementCommand(
        QStringLiteral("!NET GET"), QStringLiteral("NET GET"),
        [this](const QJsonObject &network) {
            sendManagementCommand(
                QStringLiteral("!PROV STATUS"), QStringLiteral("PROV STATUS"),
                [this, network](const QJsonObject &provisioning) {
                    const auto addresses = NetworkAddresses::fromResponses(network, provisioning);
                    QStringList wireless;
                    if (!addresses.station.isEmpty())
                        wireless.append(addresses.station);
                    if (!addresses.accessPoint.isEmpty())
                        wireless.append(tr("%1 (AP provisioning)").arg(addresses.accessPoint));

                    auto *dialog = new QMessageBox(
                        QMessageBox::Information, tr("Get IP"),
                        tr("Wired IP: %1\nWireless IP: %2")
                            .arg(addresses.ethernet.isEmpty() ? tr("Not connected")
                                                             : addresses.ethernet,
                                 wireless.isEmpty() ? tr("Not connected")
                                                    : wireless.join(QLatin1Char('/'))),
                        QMessageBox::Ok, this);
                    dialog->setObjectName(QStringLiteral("networkAddressesDialog"));
                    dialog->button(QMessageBox::Ok)->setText(tr("OK"));
                    dialog->setTextFormat(Qt::PlainText);
                    dialog->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                                    Qt::TextSelectableByKeyboard);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->setWindowModality(Qt::WindowModal);
                    dialog->open();
                });
        });
}
