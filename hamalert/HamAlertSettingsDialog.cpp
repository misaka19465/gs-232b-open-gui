#include "HamAlertSettingsDialog.h"

#include "AppSettings.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

HamAlertSettingsDialog::HamAlertSettingsDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("hamAlertSettingsDialog"));
    setWindowTitle(tr("HamAlert settings"));
    setWindowModality(Qt::WindowModal);
    resize(460, 240);

    const AppSettings preferences;
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_username = new QLineEdit(preferences.hamAlertUsername(), this);
    m_username->setObjectName(QStringLiteral("hamAlertUsername"));
    m_username->setMaxLength(64);
    m_username->setClearButtonEnabled(true);
    form->addRow(tr("HamAlert username"), m_username);

    m_password = new QLineEdit(preferences.hamAlertTelnetPassword(), this);
    m_password->setObjectName(QStringLiteral("hamAlertTelnetPassword"));
    m_password->setMaxLength(256);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Telnet password"), m_password);
    layout->addLayout(form);

    auto *warning = new QLabel(
        tr("Enable the Telnet action on the HamAlert triggers you want to receive. Telnet is unencrypted; use a dedicated password. Credentials are saved in local application settings."),
        this);
    warning->setObjectName(QStringLiteral("hamAlertCredentialWarning"));
    warning->setWordWrap(true);
    layout->addWidget(warning);
    m_password->setAccessibleDescription(warning->text());
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    m_save->setDefault(true);
    m_save->setObjectName(QStringLiteral("saveHamAlertCredentials"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &HamAlertSettingsDialog::saveCredentials);
    const auto updateEnabled = [this] {
        m_save->setEnabled(!m_username->text().trimmed().isEmpty() &&
                           !m_password->text().trimmed().isEmpty());
    };
    connect(m_username, &QLineEdit::textChanged, this, updateEnabled);
    connect(m_password, &QLineEdit::textChanged, this, updateEnabled);
    updateEnabled();
}

void HamAlertSettingsDialog::saveCredentials()
{
    if (!AppSettings().setHamAlertCredentials(m_username->text(), m_password->text())) {
        QMessageBox::warning(this, tr("HamAlert settings"),
                             tr("Could not save the HamAlert credentials."));
        return;
    }
    accept();
}
