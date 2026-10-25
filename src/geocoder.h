// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

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
