#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

class Geocoder : public QObject
{
    Q_OBJECT

public:
    explicit Geocoder(QObject *parent = nullptr);

    void search(const QString &query);
    void cancel();

signals:
    void finished(const QVariantList &results, const QString &error);

private:
    void handleReply(QNetworkReply *reply);

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QElapsedTimer m_lastRequest;
};
