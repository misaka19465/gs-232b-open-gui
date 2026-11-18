#include "GreatCircleMap.h"
#include "MaidenheadLocator.h"

#include <QBuffer>
#include <QDir>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPdfDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

namespace {

constexpr int MapSize = 1024;
constexpr qint64 MaximumResponseBytes = 16 * 1024 * 1024;

QImage renderMap(QByteArray &pdf)
{
    if (!pdf.startsWith("%PDF-"))
        return {};

    QBuffer buffer(&pdf);
    buffer.open(QIODevice::ReadOnly);
    QPdfDocument document;
    document.load(&buffer);
    // NOTE: NS6T 的 SQUARE + pstrotator + noheadingfooting 输出一页 4 英寸正方形。
    // NOTE: 地图半径为 103 pt，外侧 5 pt 为刻度圈；显式裁掉刻度，保持地图中心和方位不变。
    if (document.status() != QPdfDocument::Status::Ready || document.pageCount() != 1 ||
        document.pageSize(0) != QSizeF(288, 288))
        return {};

    const QImage page = document.render(0, QSize(1536, 1536));
    if (page.isNull())
        return {};

    const qreal diameter = page.width() * 206.0 / 288;
    const QRectF source((page.width() - diameter) / 2, (page.height() - diameter) / 2,
                        diameter, diameter);
    QImage image(MapSize, MapSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPainterPath circle;
    circle.addEllipse(QRectF(0, 0, MapSize, MapSize));
    painter.setClipPath(circle);
    painter.drawImage(QRectF(0, 0, MapSize, MapSize), page, source);
    return image;
}

} // namespace

GreatCircleMap::GreatCircleMap(QObject *parent)
    : GreatCircleMap(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                         QStringLiteral("/great-circle-maps"),
                     QUrl(QStringLiteral("https://ns6t.net/azimuth/code/azimuth.fcgi")), parent)
{
}

GreatCircleMap::GreatCircleMap(const QString &cacheDirectory, const QUrl &serviceUrl,
                             QObject *parent)
    : QObject(parent), m_cacheDirectory(cacheDirectory), m_serviceUrl(serviceUrl)
{
}

GreatCircleMap::~GreatCircleMap()
{
    cancelRequest();
}

void GreatCircleMap::load(const QString &grid)
{
    const QString normalized = grid.trimmed().toUpper();
    if (m_grid == normalized && (m_reply || !m_image.isNull()))
        return;

    clear();
    const auto center = MaidenheadLocator::center(grid);
    if (!center)
        return;
    m_grid = normalized;

    QImageReader reader(cacheFileName(), "PNG");
    // NOTE: 先验证尺寸再解码，损坏或过期的缓存只触发重新生成，不作为有效地图展示。
    if (reader.size() == QSize(MapSize, MapSize)) {
        m_image = reader.read();
        if (!m_image.isNull()) {
            emit changed();
            return;
        }
    }

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("title"), QStringLiteral("Azimuthal Map"));
    // NOTE: 用网格几何中心的经纬度避免服务只支持六位网格时丢失八位网格的精度。
    form.addQueryItem(QStringLiteral("location"),
                      QStringLiteral("%1,%2").arg(center->y(), 0, 'f', 8)
                          .arg(center->x(), 0, 'f', 8));
    form.addQueryItem(QStringLiteral("distance"), QStringLiteral("20000"));
    form.addQueryItem(QStringLiteral("paper"), QStringLiteral("SQUARE"));
    for (const auto *key : {"bluefill", "noheadingfooting", "pstrotator", "view"})
        form.addQueryItem(QString::fromLatin1(key), QStringLiteral("on"));

    QNetworkRequest request(m_serviceUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", "GS-232B-Open-Control/0.3.0");
    request.setTransferTimeout(60000);
    m_reply = m_network.post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_response.size() + m_reply->bytesAvailable() > MaximumResponseBytes) {
            m_responseTooLarge = true;
            m_reply->abort();
            return;
        }
        m_response.append(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, &GreatCircleMap::finishRequest);
    // NOTE: 总时限独立于传输空闲超时，持续慢速响应也不会永久占用请求。
    QTimer::singleShot(65000, m_reply, &QNetworkReply::abort);
    emit changed();
}

void GreatCircleMap::clear()
{
    cancelRequest();
    m_grid.clear();
    m_image = {};
    m_error.clear();
    emit changed();
}

const QImage &GreatCircleMap::image() const { return m_image; }
bool GreatCircleMap::isLoading() const { return m_reply != nullptr; }
QString GreatCircleMap::errorString() const { return m_error; }

void GreatCircleMap::cancelRequest()
{
    if (m_reply) {
        // NOTE: 先断开回调再取消，旧网格的完成信号不能覆盖新网格或已关闭的地图。
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_response.clear();
    m_responseTooLarge = false;
}

void GreatCircleMap::finishRequest()
{
    auto *reply = m_reply;
    m_reply = nullptr;
    if (m_responseTooLarge) {
        m_error = tr("The map service returned too much data.");
    } else if (reply->error() != QNetworkReply::NoError) {
        m_error = tr("Could not generate the great-circle map: %1").arg(reply->errorString());
    } else {
        m_image = renderMap(m_response);
        if (m_image.isNull()) {
            m_error = tr("The map service returned an invalid map.");
        } else {
            QSaveFile file(cacheFileName());
            if (!QDir().mkpath(m_cacheDirectory) || !file.open(QIODevice::WriteOnly) ||
                !m_image.save(&file, "PNG") || !file.commit())
                m_error = tr("The map is ready, but could not be saved for offline use.");
        }
    }
    reply->deleteLater();
    m_response.clear();
    emit changed();
}

QString GreatCircleMap::cacheFileName() const
{
    // NOTE: 提供方、裁剪契约与渲染版本进入键名，后续格式调整不会误用已有 PNG。
    return QDir(m_cacheDirectory).filePath(QStringLiteral("ns6t-v1-%1.png").arg(m_grid));
}
