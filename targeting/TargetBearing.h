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

#include <QPointF>
#include <QString>

#include <optional>

/** @brief 可供界面、网络接口及其他调用方共用的目标定位与短路径方位计算。 */
namespace TargetBearing {

enum class TargetType { Grid, Dxcc };
enum class Error { None, InvalidStationGrid, UnknownTarget, UndefinedBearing };

/** @brief coordinates 的 x 为东经、y 为北纬，单位为度；DXCC 字段仅对呼号目标有值。 */
struct Target {
    TargetType type;
    QString input;
    QPointF coordinates;
    QString entityName;
    QString primaryPrefix;
};

/** @brief 从本站指向目标的真北顺时针大圆初始方位，degrees 范围为 [0, 360)。 */
struct Solution {
    Target target;
    double degrees;
    /** @return 四舍五入到整数度并归一化到 0–359，可用于 GS-232B 的 M 命令。 */
    int roundedDegrees() const;
};

/** @brief 仅 error 为 None 时 solution 有值；失败时调用方不得发送旋转命令。 */
struct Result {
    Error error;
    std::optional<Solution> solution;
};

/** @brief 先识别 4/6/8 位 Maidenhead 网格，再查询呼号或前缀所属 DXCC；忽略大小写及首尾空白。 */
std::optional<Target> resolve(const QString &target);
/**
 * @brief 计算两坐标间的短路径初始方位，坐标为东经/北纬度数。
 * @return [0, 360) 度；非法坐标、本站位于极点、同点或对跖点时返回空值。
 */
std::optional<double> initialBearing(const QPointF &station, const QPointF &target);
/** @brief 用本站网格中心计算目标方位；只返回结果，不读取应用配置或控制设备。 */
Result calculate(const QString &stationGrid, const QString &target);

} // namespace TargetBearing
