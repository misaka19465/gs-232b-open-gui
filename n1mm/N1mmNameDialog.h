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

class QLineEdit;
class QPushButton;

/** @brief 编辑并保存 N1MM 精确匹配的旋转器名；保存失败保留输入，取消不修改配置。 */
class N1mmNameDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 载入当前名称，离线时也可配置。 */
    explicit N1mmNameDialog(QWidget *parent = nullptr);

private:
    void saveName();

    QLineEdit *m_name;
    QPushButton *m_save;
};
