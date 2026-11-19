#include "WindDirectionOffsetDialog.h"

#include "AppSettings.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

WindDirectionOffsetDialog::WindDirectionOffsetDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("windDirectionOffsetDialog"));
    setWindowTitle(tr("Wind direction offset"));
    setWindowModality(Qt::WindowModal);
    resize(440, 240);

    auto *layout = new QVBoxLayout(this);
    m_offset = new QSpinBox(this);
    m_offset->setObjectName(QStringLiteral("windDirectionOffset"));
    m_offset->setRange(0, 359);
    m_offset->setSuffix(QStringLiteral("°"));
    m_offset->setValue(AppSettings().windDirectionOffset());
    auto *label = new QLabel(tr("Wind direction offset (°)"), this);
    label->setBuddy(m_offset);
    layout->addWidget(label);
    layout->addWidget(m_offset);

    auto *hint = new QLabel(
        tr("Target azimuth = wind-from direction + offset. To put the boom broadside to the wind and let wind pass along the elements, use 90° or 270°."),
        this);
    hint->setWordWrap(true);
    m_offset->setAccessibleDescription(hint->text());
    layout->addWidget(hint);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("saveWindDirectionOffset"));
    save->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &WindDirectionOffsetDialog::saveOffset);
}

void WindDirectionOffsetDialog::saveOffset()
{
    m_offset->interpretText();
    if (!AppSettings().setWindDirectionOffset(m_offset->value())) {
        QMessageBox::warning(this, tr("Wind direction offset"),
                             tr("Could not save the wind direction offset."));
        return;
    }
    accept();
}
