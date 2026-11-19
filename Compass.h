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

#include <QImage>
#include <QWidget>

class GreatCircleMap;

/** @brief 使用系统调色板绘制的方位角罗盘。 */
class Compass final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 创建指向 0° 的罗盘。 */
    explicit Compass(QWidget *parent = nullptr);
    /** @brief 更新设备实测方位；允许保留超过 360° 的机械行程。 */
    void setAngle(int degrees);
    /** @brief 切换大圆图模式；关闭后恢复普通罗盘并取消地图请求。 */
    void setGreatCircleMapEnabled(bool enabled);
    /** @brief 设置地图中心网格；开启大圆图时立即加载对应缓存或请求生成。 */
    void setStationGrid(const QString &grid);
    /** @brief 设置仅用于大圆图参考显示的天线增益；0 dBi 隐藏波瓣。 */
    void setAntennaGain(double gain);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /** @brief 用户点击罗盘后请求定位到该方位。 */
    void angleRequested(int degrees);
    /** @brief 地图生成或本地缓存失败，供主窗口非模态提示。 */
    void mapError(const QString &message);

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    qreal radius() const;
    void updateMapStatus();

    GreatCircleMap *m_map;
    QImage m_themedMap;
    QString m_stationGrid;
    bool m_mapEnabled = false;
    double m_antennaGain = 0.0;
    int m_angle = 0;
};
