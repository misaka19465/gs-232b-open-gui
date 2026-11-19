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

#include "ControlPanel.h"
#include "MotionButtons.h"

#include <QTimer>

void ControlPanel::setupMotionButtons()
{
    const auto wireAxis = [this](MotionButtons *buttons, const QString &negative,
                                  const QString &positive, const QString &stop) {
        connect(buttons, &MotionButtons::motionStarted, this,
                [this, negative, positive](bool forward) {
                    m_elevationCommandTimer->stop();
                    emit commandRequested(forward ? positive : negative);
                });
        connect(buttons, &MotionButtons::motionStopped, this, [this, stop] {
            m_elevationCommandTimer->stop();
            emit commandRequested(stop);
        });
        connect(buttons, &MotionButtons::stopAllRequested, this, [this] {
            cancelPendingMotion();
            emit commandRequested(QStringLiteral("S"));
        });
    };
    wireAxis(m_azimuthMotion, QStringLiteral("L"), QStringLiteral("R"), QStringLiteral("A"));
    wireAxis(m_elevationMotion, QStringLiteral("D"), QStringLiteral("U"), QStringLiteral("E"));
}

void ControlPanel::requestAzimuth(int mechanical)
{
    if (!m_commandEnabled)
        return;
    cancelPendingMotion();
    emit commandRequested(QStringLiteral("M%1").arg(mechanical, 3, 10, QLatin1Char('0')));
}

void ControlPanel::requestElevation(int angle)
{
    if (!m_commandEnabled || !m_elevationSupported)
        return;
    cancelPendingMotion();
    // NOTE: 仰角单轴定位保留实测机械方位，不能应用 LP 或方位补偿。
    emit commandRequested(QStringLiteral("W%1 %2")
                              .arg(m_azimuthValue, 3, 10, QLatin1Char('0'))
                              .arg(angle, 3, 10, QLatin1Char('0')));
}

void ControlPanel::cancelPendingMotion()
{
    m_elevationCommandTimer->stop();
    m_azimuthMotion->cancelMotion();
    m_elevationMotion->cancelMotion();
}
