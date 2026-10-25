// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "database.h"

#include <QAbstractListModel>
#include <QVector>

class RecordListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        RecordIdRole = Qt::UserRole + 1,
        DateRole,
        TagRole,
        NotesRole,
        ColorRole,
        CreatedAtRole,
    };

    explicit RecordListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    void setRecords(const QVector<Database::Record> &records);
    void clear();

signals:
    void countChanged();

private:
    QVector<Database::Record> m_records;
};
