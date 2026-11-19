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
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class QNetworkReply;

/** @brief 按 Maidenhead 网格获取 NS6T 等距方位投影地图，并原子保存可离线复用的 PNG。 */
class GreatCircleMap final : public QObject
{
    Q_OBJECT

public:
    /** @brief 使用当前用户的应用缓存目录和 NS6T HTTPS 地图服务。 */
    explicit GreatCircleMap(QObject *parent = nullptr);
    /** @brief 使用独立缓存目录和生成接口，供隔离存储及接口集成验证使用。 */
    GreatCircleMap(const QString &cacheDirectory, const QUrl &serviceUrl,
                   QObject *parent = nullptr);
    /** @brief 在状态成员销毁前取消请求，避免网络析构回调访问已释放的状态。 */
    ~GreatCircleMap() override;
    /** @brief 先读取缓存，缺失或损坏时异步生成；相同网格的有效图像或在途请求会复用。 */
    void load(const QString &grid);
    /** @brief 取消在途请求并清除内存图像，不删除已保存的地图。 */
    void clear();
    /** @return 当前网格的地图；未就绪时为空，调用方不得替换为其他网格的图像。 */
    const QImage &image() const;
    /** @return 当前是否正在请求地图。 */
    bool isLoading() const;
    /** @return 当前生成或缓存错误；新请求开始时清除。 */
    QString errorString() const;

signals:
    /** @brief 地图、加载状态或错误变化；接收方可重新读取当前状态。 */
    void changed();

private:
    void cancelRequest();
    void finishRequest();
    QString cacheFileName() const;

    QString m_cacheDirectory;
    QUrl m_serviceUrl;
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;
    QByteArray m_response;
    QString m_grid;
    QImage m_image;
    QString m_error;
    bool m_responseTooLarge = false;
};
