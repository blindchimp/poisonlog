// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "tileimageprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQuickTextureFactory>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVector>

namespace {

QString tileUrlForId(const QString &id)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 3)
        return {};

    bool zOk = false;
    bool xOk = false;
    bool yOk = false;
    const int z = parts.at(0).toInt(&zOk);
    const int x = parts.at(1).toInt(&xOk);
    const int y = parts.at(2).toInt(&yOk);
    if (!zOk || !xOk || !yOk || z < 0 || z > 22)
        return {};

    constexpr double worldSize = 40075016.685578488;
    constexpr double origin = 20037508.342789244;
    const double span = worldSize / double(quint64(1) << z);
    const double minX = -origin + x * span;
    const double maxX = minX + span;
    const double maxY = origin - y * span;
    const double minY = maxY - span;

    return QStringLiteral(
               "https://imagery.nationalmap.gov/arcgis/rest/services/USGSNAIPPlus/"
               "ImageServer/exportImage?bbox=%1,%2,%3,%4&bboxSR=3857&imageSR=3857"
               "&size=256,256&format=jpg&interpolation=RSP_BilinearInterpolation&f=image")
        .arg(minX, 0, 'f', 4)
        .arg(minY, 0, 'f', 4)
        .arg(maxX, 0, 'f', 4)
        .arg(maxY, 0, 'f', 4);
}

} // namespace

class TileNetwork : public QObject
{
    Q_OBJECT

public:
    TileNetwork() = default;

    void fetch(quint64 token, const QString &id)
    {
        if (!m_network)
            m_network = new QNetworkAccessManager(this);

        m_tokenIds.insert(token, id);

        auto waiters = m_waiters.find(id);
        if (waiters != m_waiters.end()) {
            waiters->append(token);
            return;
        }

        m_waiters.insert(id, {token});

        QNetworkRequest request(QUrl(tileUrlForId(id)));
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("PoisonLog/0.1 (Qt desktop application)"));
        request.setRawHeader("Accept", "image/png,image/*;q=0.8");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *reply = m_network->get(request);
        m_inFlight.insert(id, reply);

        connect(reply, &QNetworkReply::finished, this, [this, id, reply]() {
            m_inFlight.remove(id);

            QByteArray data;
            QString error;
            if (reply->error() != QNetworkReply::NoError) {
                error = QStringLiteral("Map tile download failed: %1").arg(reply->errorString());
            } else {
                const int status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status != 200) {
                    error = QStringLiteral("Map tile download failed (HTTP %1).").arg(status);
                } else {
                    data = reply->readAll();
                }
            }

            const QVector<quint64> tokens = m_waiters.take(id);
            for (quint64 token : tokens) {
                m_tokenIds.remove(token);
                emit fetched(token, data, error);
            }

            reply->deleteLater();
        });
    }

    void cancel(quint64 token)
    {
        const QString id = m_tokenIds.take(token);
        if (id.isEmpty())
            return;

        auto waiters = m_waiters.find(id);
        if (waiters == m_waiters.end())
            return;

        waiters->removeAll(token);
        if (!waiters->isEmpty())
            return;

        m_waiters.erase(waiters);
        if (QNetworkReply *reply = m_inFlight.take(id)) {
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }

signals:
    void fetched(quint64 token, const QByteArray &data, const QString &error);

private:
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, QNetworkReply *> m_inFlight;
    QHash<QString, QVector<quint64>> m_waiters;
    QHash<quint64, QString> m_tokenIds;
};

namespace {

class TileImageResponse : public QQuickImageResponse
{
public:
    TileImageResponse(TileImageProvider *provider, const QString &id, quint64 token)
        : m_provider(provider)
        , m_id(id)
        , m_token(token)
    {
        connect(m_provider->network(), &TileNetwork::fetched, this,
                [this](quint64 token, const QByteArray &data, const QString &error) {
                    onFetched(token, data, error);
                },
                Qt::QueuedConnection);
    }

    ~TileImageResponse() override
    {
        if (!m_done)
            m_provider->cancelTile(m_token);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_error;
    }

    void cancel() override
    {
        if (m_done)
            return;
        m_cancelled = true;
        m_provider->cancelTile(m_token);
    }

    void start()
    {
        m_provider->fetchTile(m_token, m_id);
    }

private:
    void onFetched(quint64 token, const QByteArray &data, const QString &error)
    {
        if (token != m_token || m_done || m_cancelled)
            return;

        if (!error.isEmpty() || data.isEmpty()) {
            if (m_retries < 2) {
                ++m_retries;
                QTimer::singleShot(500, this, [this]() {
                    if (!m_cancelled && !m_done)
                        m_provider->fetchTile(m_token, m_id);
                });
                return;
            }
            finishWithError(error.isEmpty() ? QStringLiteral("Map tile data was empty.") : error);
            return;
        }

        QImage image;
        if (!image.loadFromData(data)) {
            finishWithError(QStringLiteral("Map tile data could not be decoded."));
            return;
        }

        m_image = image;
        m_provider->cacheImage(m_id, image);
        const QString path = m_provider->cacheFile(m_id);
        QDir().mkpath(QFileInfo(path).absolutePath());
        image.save(path, "PNG");

        m_done = true;
        emit finished();
    }

    void finishWithError(const QString &error)
    {
        m_error = error;
        if (!m_cancelled && !m_done) {
            m_done = true;
            emit finished();
        }
    }

    TileImageProvider *m_provider = nullptr;
    QString m_id;
    quint64 m_token = 0;
    QImage m_image;
    QString m_error;
    int m_retries = 0;
    bool m_cancelled = false;
    bool m_done = false;
};

class ImmediateTileResponse : public QQuickImageResponse
{
public:
    explicit ImmediateTileResponse(const QImage &image, const QString &error = {})
        : m_image(image)
        , m_error(error)
    {
        QTimer::singleShot(0, this, [this]() { emit finished(); });
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_error;
    }

private:
    QImage m_image;
    QString m_error;
};

} // namespace

TileImageProvider::TileImageProvider()
    : QQuickAsyncImageProvider()
    , m_cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                 + QStringLiteral("/usgs-naip-plus"))
{
    m_networkThread = new QThread;
    m_network = new TileNetwork;
    m_network->moveToThread(m_networkThread);
    connect(m_networkThread, &QThread::finished, m_network, &QObject::deleteLater);
    m_networkThread->start();
}

TileImageProvider::~TileImageProvider()
{
    if (!m_networkThread)
        return;
    m_networkThread->quit();
    m_networkThread->wait();
    delete m_networkThread;
}

QQuickImageResponse *TileImageProvider::requestImageResponse(const QString &id,
                                                             const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)

    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 3) {
        return new ImmediateTileResponse({}, QStringLiteral("Invalid map tile request."));
    }
    bool zOk = false;
    bool xOk = false;
    bool yOk = false;
    const int z = parts.at(0).toInt(&zOk);
    const int x = parts.at(1).toInt(&xOk);
    const int y = parts.at(2).toInt(&yOk);
    if (!zOk || !xOk || !yOk || z < 0 || z > 22) {
        return new ImmediateTileResponse({}, QStringLiteral("Invalid map tile coordinates."));
    }

    const QImage memory = memoryImage(id);
    if (!memory.isNull())
        return new ImmediateTileResponse(memory);

    const QString path = cacheFile(id);
    if (QFileInfo::exists(path)) {
        QImage cached;
        if (cached.load(path)) {
            cacheImage(id, cached);
            return new ImmediateTileResponse(cached);
        }
    }

    auto *response = new TileImageResponse(this, id, nextToken());
    response->start();
    return response;
}

QString TileImageProvider::cacheFile(const QString &id) const
{
    return m_cacheDir + QLatin1Char('/') + id + QStringLiteral(".png");
}

QImage TileImageProvider::memoryImage(const QString &id) const
{
    QMutexLocker locker(&m_memoryMutex);
    return m_memory.value(id);
}

void TileImageProvider::cacheImage(const QString &id, const QImage &image)
{
    QMutexLocker locker(&m_memoryMutex);
    if (m_memory.size() > 400)
        m_memory.clear();
    m_memory.insert(id, image);
}

TileNetwork *TileImageProvider::network() const
{
    return m_network;
}

void TileImageProvider::fetchTile(quint64 token, const QString &id)
{
    QMetaObject::invokeMethod(
        m_network, [network = m_network, token, id]() { network->fetch(token, id); },
        Qt::QueuedConnection);
}

void TileImageProvider::cancelTile(quint64 token)
{
    QMetaObject::invokeMethod(
        m_network, [network = m_network, token]() { network->cancel(token); },
        Qt::QueuedConnection);
}

quint64 TileImageProvider::nextToken()
{
    return m_nextToken++;
}

#include "tileimageprovider.moc"
