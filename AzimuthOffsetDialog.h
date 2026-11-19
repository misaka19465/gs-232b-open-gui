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

class QLabel;
class QSpinBox;

/** @brief 编辑应用本地的方位角补偿；成功保存后通知宿主立即刷新方位显示与输入范围。 */
class AzimuthOffsetDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 载入已保存的整数补偿，离线时也可设置。 */
    explicit AzimuthOffsetDialog(QWidget *parent = nullptr);

private:
    void updateRange();
    void saveOffset();

    QSpinBox *m_offset;
    QLabel *m_range;
};
