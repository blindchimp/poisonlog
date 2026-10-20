#include "geocoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

Geocoder::Geocoder(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_lastRequest.invalidate();
}

void Geocoder::search(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        emit finished({}, QStringLiteral("Enter an address to search."));
        return;
    }

    if (m_lastRequest.isValid() && m_lastRequest.elapsed() < 1000) {
        emit finished({}, QStringLiteral("Please wait a moment before searching again."));
        return;
    }

    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    QUrl url(QStringLiteral("https://nominatim.openstreetmap.org/search"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("format"), QStringLiteral("jsonv2"));
    params.addQueryItem(QStringLiteral("q"), trimmed);
    params.addQueryItem(QStringLiteral("limit"), QStringLiteral("5"));
    params.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("0"));
    url.setQuery(params);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PoisonLog/0.1 (Qt desktop application)"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_lastRequest.start();
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        if (m_reply)
            handleReply(m_reply);
    });
}

void Geocoder::cancel()
{
    if (!m_reply)
        return;
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
}

void Geocoder::handleReply(QNetworkReply *reply)
{
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit finished({}, QStringLiteral("Address search failed: %1").arg(reply->errorString()));
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200) {
        emit finished({}, QStringLiteral("Address search failed (HTTP %1).").arg(status));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        emit finished({}, QStringLiteral("Address search returned an unexpected response."));
        return;
    }

    QVariantList results;
    const QJsonArray array = document.array();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        const QString displayName = object.value(QStringLiteral("display_name")).toString();
        bool latOk = false;
        bool lonOk = false;
        const double lat = object.value(QStringLiteral("lat")).toString().toDouble(&latOk);
        const double lon = object.value(QStringLiteral("lon")).toString().toDouble(&lonOk);
        if (!latOk || !lonOk || displayName.isEmpty())
            continue;

        QVariantMap result;
        result.insert(QStringLiteral("displayName"), displayName);
        result.insert(QStringLiteral("lat"), lat);
        result.insert(QStringLiteral("lon"), lon);
        results.append(result);
    }

    if (results.isEmpty()) {
        emit finished({}, QStringLiteral("No matching address was found."));
        return;
    }

    emit finished(results, QString());
}
