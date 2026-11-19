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

class QComboBox;
class QLabel;
class QPushButton;

/** @brief 编辑旋转器型号草稿，分别显示设备已保存型号和重启前的生效状态。 */
class RotorModelDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建型号设置窗口；设备状态和写入结果由宿主同步。 */
    explicit RotorModelDialog(QWidget *parent = nullptr);
    /** @return 设备确认的已保存型号 ID；尚未读取时为 0。 */
    int savedModelId() const;
    /** @return 当前草稿的固件型号 ID；未选择时为 0。 */
    int selectedModelId() const;
    /** @brief 同步成功响应；无效的已保存 ID 返回 false 且保留原状态，未知生效 ID 使用 0。 */
    bool setDeviceModel(int savedModelId, int activeModelId, bool restartRequired);
    /** @brief 丢弃设备型号缓存和草稿，等待重新读取。 */
    void clearDeviceModel();
    /** @brief 开始编辑时恢复设备已保存型号；写入期间保留正在提交的草稿。 */
    void resetSelection();
    /** @brief 按连接和透传状态启用编辑；未知已保存型号时禁止写入。 */
    void setCommandEnabled(bool enabled);
    /** @brief 认证或写入期间锁定编辑，等待宿主报告结果。 */
    void setBusy(bool busy);
    /** @brief 显示宿主提供的已翻译进度或结果；空文本隐藏提示。 */
    void setStatus(const QString &message);

signals:
    /** @brief 请求写入草稿型号；宿主负责认证、发送和成功回读。 */
    void modelApplyRequested(int modelId);

private:
    void updateControls();

    int m_savedModelId = 0;
    bool m_commandEnabled = false;
    bool m_busy = false;
    QLabel *m_saved;
    QComboBox *m_model;
    QLabel *m_restartHint;
    QLabel *m_status;
    QPushButton *m_apply;
};
