// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "appstore.h"
#include "tileimageprovider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqml.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PoisonLog"));
    QCoreApplication::setApplicationName(QStringLiteral("poisonlog"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Poison Log"));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    AppStore appStore;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("tiles"), new TileImageProvider);
    qmlRegisterSingletonInstance("PoisonLog", 1, 0, "App", &appStore);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &appStore, &AppStore::createBackup);

    engine.loadFromModule("PoisonLog", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
