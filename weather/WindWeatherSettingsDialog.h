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

#pragma once

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QWidget;

/** @brief 选择风向数据来源并按需配置和风天气凭据。 */
class WindWeatherSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit WindWeatherSettingsDialog(QWidget *parent = nullptr);

private:
    void updateProviderDetails();
    void saveSettings();

    QComboBox *m_provider;
    QLabel *m_description;
    QCheckBox *m_termsAccepted;
    QWidget *m_qWeatherFields;
    QLineEdit *m_apiHost;
    QLineEdit *m_apiKey;
};
