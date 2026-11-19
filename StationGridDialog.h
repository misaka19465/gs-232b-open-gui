/*
 * GS-232B Open GUI - rotator controller GUI.
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

class QLineEdit;
class QPushButton;

/** @brief 编辑并保存本站 Maidenhead 网格的本地设置窗口。 */
class StationGridDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 恢复已保存的网格；保存成功才关闭窗口，取消不修改配置。 */
    explicit StationGridDialog(QWidget *parent = nullptr);

private:
    void updateControls();
    void saveGrid();

    QLineEdit *m_grid;
    QPushButton *m_save;
};
