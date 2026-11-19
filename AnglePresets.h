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

#include "AppSettings.h"

#include <QString>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class QPushButton;
class QLineEdit;
class QStackedLayout;

/** @brief 按机械角度保存预设，按方位补偿及最短路径模式显示和编辑。 */
class AnglePresets final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 目标解析结果；bearing 是未应用本站补偿的 0–359° 方位。 */
    struct TargetInputResult {
        std::optional<int> bearing;
        QString error;
    };
    using TargetResolver = std::function<TargetInputResult(const QString &)>;

    /** @brief 载入指定轴的预设，离线时仍可编辑。 */
    explicit AnglePresets(AppSettings::Axis axis, QWidget *parent = nullptr);
    /** @brief 为方位预设启用网格、呼号及前缀输入；结果是未经补偿的圆周方位。 */
    void setTargetResolver(TargetResolver resolver);
    /** @brief 更新方位补偿及编辑范围，不改写已保存的机械角度；仰角忽略此设置。 */
    void setAzimuthOffset(int degrees);
    /** @brief 切换方位预设的取模显示和编辑，不改写存储；仰角忽略此设置。 */
    void setShortestPathEnabled(bool enabled);
    /** @brief 优先采用能完整容纳角度的紧凑尺寸，仍允许布局拉伸按钮。 */
    QSize sizeHint() const override;

signals:
    /** @brief 请求转到预设的机械角度，调用方不得再次应用补偿。 */
    void angleRequested(int mechanicalAngle);
    /** @brief 预设输入无效，未改写已保存角度。 */
    void inputRejected(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void beginEditing(int index);
    void cancelEditing();
    void finishEditing();
    void submitEditing();
    QString editingHint() const;
    void updateAngles();

    struct Preset {
        int angle;
        QWidget *cell;
        QPushButton *button;
        QLineEdit *editor;
        QStackedLayout *stack;
    };
    AppSettings::Axis m_axis;
    int m_offset = 0;
    bool m_shortestPathEnabled = false;
    int m_activeIndex = -1;
    TargetResolver m_targetResolver;
    std::vector<Preset> m_presets;
};
