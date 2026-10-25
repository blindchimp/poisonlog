// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>

class QThread;
class TileNetwork;

class TileImageProvider : public QQuickAsyncImageProvider
{
public:
    TileImageProvider();
    ~TileImageProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    QString cacheFile(const QString &id) const;
    QImage memoryImage(const QString &id) const;
    void cacheImage(const QString &id, const QImage &image);

    TileNetwork *network() const;
    void fetchTile(quint64 token, const QString &id);
    void cancelTile(quint64 token);
    quint64 nextToken();

private:
    mutable QMutex m_memoryMutex;
    mutable QHash<QString, QImage> m_memory;
    QString m_cacheDir;
    QThread *m_networkThread = nullptr;
    TileNetwork *m_network = nullptr;
    quint64 m_nextToken = 0;
};
