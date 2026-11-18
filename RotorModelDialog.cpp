#include "RotorModelDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

RotorModelDialog::RotorModelDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("rotorModelDialog"));
    setWindowTitle(tr("Rotor model"));
    setWindowModality(Qt::WindowModal);
    resize(420, 210);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_saved = new QLabel(this);
    m_saved->setObjectName(QStringLiteral("savedRotorModel"));
    m_saved->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Saved model"), m_saved);
    m_model = new QComboBox(this);
    m_model->setObjectName(QStringLiteral("rotorModel"));
    const QStringList models = {
        QStringLiteral("G-5500"), QStringLiteral("G-5400B"), QStringLiteral("G-5600B"),
        QStringLiteral("G-5500DC"), QStringLiteral("G-400"), QStringLiteral("G-500A"),
        QStringLiteral("G-550"), QStringLiteral("G-800DXA"), QStringLiteral("G-1000DXA"),
        QStringLiteral("G-2800DXA"), QStringLiteral("G-800DXC"), QStringLiteral("G-1000DXC"),
        QStringLiteral("G-2800DXC"), tr("SDX series"), QStringLiteral("G-400 + G-500A"),
        QStringLiteral("G-400 + G-550")
    };
    for (int index = 0; index < models.size(); ++index)
        m_model->addItem(models.at(index), index + 1);
    form->addRow(tr("Select model"), m_model);
    layout->addLayout(form);
    m_restartHint = new QLabel(this);
    m_restartHint->setObjectName(QStringLiteral("rotorModelRestartHint"));
    m_restartHint->setWordWrap(true);
    layout->addWidget(m_restartHint);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("rotorModelStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_apply = buttons->addButton(tr("Apply model"), QDialogButtonBox::AcceptRole);
    m_apply->setObjectName(QStringLiteral("applyModel"));
    m_apply->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, [this] {
        emit modelApplyRequested(selectedModelId());
    });
    connect(m_model, qOverload<int>(&QComboBox::currentIndexChanged), this, &RotorModelDialog::updateControls);
    clearDeviceModel();
}

int RotorModelDialog::savedModelId() const
{
    return m_savedModelId;
}

int RotorModelDialog::selectedModelId() const
{
    return m_model->currentData().toInt();
}

bool RotorModelDialog::setDeviceModel(int savedModelId, int activeModelId, bool restartRequired)
{
    const int index = m_model->findData(savedModelId);
    if (index < 0)
        return false;
    const bool followSavedModel = selectedModelId() == 0 || selectedModelId() == m_savedModelId;
    m_savedModelId = savedModelId;
    m_saved->setText(m_model->itemText(index));
    if (followSavedModel)
        m_model->setCurrentIndex(index);

    const int activeIndex = m_model->findData(activeModelId);
    const bool differentActiveModel = activeIndex >= 0 && activeModelId != savedModelId;
    if (differentActiveModel) {
        m_restartHint->setText(tr("Active model: %1. Restart the controller to use the saved model.")
                                   .arg(m_model->itemText(activeIndex)));
    } else if (restartRequired) {
        m_restartHint->setText(tr("Restart the controller to use the saved model."));
    }
    m_restartHint->setVisible(differentActiveModel || restartRequired);
    updateControls();
    return true;
}

void RotorModelDialog::clearDeviceModel()
{
    m_savedModelId = 0;
    m_saved->setText(tr("Not read"));
    m_model->setCurrentIndex(-1);
    m_restartHint->hide();
    setStatus({});
    updateControls();
}

void RotorModelDialog::resetSelection()
{
    if (m_busy)
        return;
    m_model->setCurrentIndex(m_model->findData(m_savedModelId));
    setStatus({});
}

void RotorModelDialog::setCommandEnabled(bool enabled)
{
    m_commandEnabled = enabled;
    updateControls();
}

void RotorModelDialog::setBusy(bool busy)
{
    m_busy = busy;
    updateControls();
}

void RotorModelDialog::setStatus(const QString &message)
{
    m_status->setText(message);
    m_status->setVisible(!message.isEmpty());
}

void RotorModelDialog::updateControls()
{
    m_model->setEnabled(m_commandEnabled && !m_busy && m_savedModelId > 0);
    m_apply->setEnabled(m_model->isEnabled() && selectedModelId() > 0 &&
                        selectedModelId() != m_savedModelId);
}
