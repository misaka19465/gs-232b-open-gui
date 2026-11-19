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

#include "StationGridDialog.h"
#include "AppSettings.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

StationGridDialog::StationGridDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("stationGridDialog"));
    setWindowTitle(tr("My grid"));
    setWindowModality(Qt::WindowModal);
    resize(420, 190);

    auto *layout = new QVBoxLayout(this);
    m_grid = new QLineEdit(this);
    m_grid->setObjectName(QStringLiteral("stationGrid"));
    m_grid->setPlaceholderText(tr("e.g. PM95AA"));
    m_grid->setClearButtonEnabled(true);
    m_grid->setText(AppSettings().stationGrid());
    m_grid->selectAll();
    auto *label = new QLabel(tr("Maidenhead grid locator"), this);
    label->setBuddy(m_grid);
    layout->addWidget(label);
    layout->addWidget(m_grid);

    auto *hint = new QLabel(
        tr("Enter a 4, 6 or 8 character grid locator. Leave blank to clear."), this);
    hint->setWordWrap(true);
    m_grid->setAccessibleDescription(hint->text());
    layout->addWidget(hint);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    m_save->setObjectName(QStringLiteral("saveStationGrid"));
    m_save->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &StationGridDialog::saveGrid);
    connect(m_grid, &QLineEdit::textChanged, this, &StationGridDialog::updateControls);
    updateControls();
}

void StationGridDialog::updateControls()
{
    m_save->setEnabled(m_grid->text().trimmed().isEmpty() ||
                       AppSettings::isValidStationGrid(m_grid->text()));
}

void StationGridDialog::saveGrid()
{
    const QString grid = m_grid->text();
    if (!grid.trimmed().isEmpty() && !AppSettings::isValidStationGrid(grid))
        return;

    if (!AppSettings().setStationGrid(grid)) {
        QMessageBox::warning(this, tr("My grid"), tr("Could not save your grid."));
        return;
    }
    accept();
}
