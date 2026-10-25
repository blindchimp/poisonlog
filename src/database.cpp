// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "database.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString dateToday()
{
    return QDate::currentDate().toString(Qt::ISODate);
}

QString nowUtc()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString nonNull(const QString &value)
{
    return value.isNull() ? QString::fromLatin1("") : value;
}

bool failQuery(const QSqlQuery &query, QString *error, const QString &context)
{
    setError(error, QStringLiteral("%1: %2").arg(context, query.lastError().text()));
    return false;
}

} // namespace

Database::Database(const QString &path)
    : m_path(path)
    , m_connectionName(QStringLiteral("poisonlog_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Database::~Database()
{
    if (m_db) {
        m_db->close();
        delete m_db;
        m_db = nullptr;
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::open(QString *error)
{
    const QFileInfo info(m_path);
    QDir().mkpath(info.absolutePath());

    m_db = new QSqlDatabase(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName));
    m_db->setDatabaseName(m_path);
    if (!m_db->open()) {
        setError(error, QStringLiteral("Could not open the database: %1").arg(m_db->lastError().text()));
        return false;
    }

    QSqlQuery query(*m_db);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        return failQuery(query, error, QStringLiteral("Could not enable foreign keys"));
    }
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));

    return ensureSchema(error);
}

bool Database::isOpen() const
{
    return m_db && m_db->isOpen();
}

bool Database::ensureSchema(QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map ("
            " id INTEGER PRIMARY KEY CHECK (id = 1),"
            " address TEXT NOT NULL,"
            " center_lat REAL NOT NULL,"
            " center_lon REAL NOT NULL,"
            " ground_span_m REAL NOT NULL,"
            " image_path TEXT NOT NULL,"
            " image_width INTEGER NOT NULL,"
            " image_height INTEGER NOT NULL,"
            " attribution TEXT NOT NULL,"
            " created_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS records ("
            " id INTEGER PRIMARY KEY,"
            " map_id INTEGER NOT NULL DEFAULT 1 REFERENCES map(id) ON DELETE CASCADE,"
            " status TEXT NOT NULL CHECK (status IN ('draft', 'committed')),"
            " date TEXT NOT NULL,"
            " tag TEXT NOT NULL,"
            " color TEXT NOT NULL,"
            " notes TEXT NOT NULL,"
            " created_at TEXT NOT NULL,"
            " updated_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS rectangles ("
            " id INTEGER PRIMARY KEY,"
            " record_id INTEGER NOT NULL REFERENCES records(id) ON DELETE CASCADE,"
            " x REAL NOT NULL,"
            " y REAL NOT NULL,"
            " w REAL NOT NULL,"
            " h REAL NOT NULL,"
            " rotation REAL NOT NULL DEFAULT 0.0,"
            " sort INTEGER NOT NULL,"
            " color TEXT NOT NULL DEFAULT ''"
            ")"),
        QStringLiteral(
            "CREATE UNIQUE INDEX IF NOT EXISTS records_one_draft "
            "ON records(status) WHERE status = 'draft'"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS records_committed_date "
            "ON records(date DESC, created_at DESC) WHERE status = 'committed'"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS rectangles_record "
            "ON rectangles(record_id, sort)"),
    };

    if (!m_db->transaction()) {
        setError(error, QStringLiteral("Could not start the schema transaction: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    for (const QString &sql : statements) {
        QSqlQuery query(*m_db);
        if (!query.exec(sql)) {
            failQuery(query, error, QStringLiteral("Could not create the database schema"));
            m_db->rollback();
            return false;
        }
    }

    int version = 1;
    {
        QSqlQuery versionRead(*m_db);
        if (versionRead.exec(QStringLiteral("PRAGMA user_version")) && versionRead.next())
            version = versionRead.value(0).toInt();
    }

    if (version < 2) {
        bool hasColorColumn = false;
        {
            QSqlQuery info(*m_db);
            if (info.exec(QStringLiteral("PRAGMA table_info(rectangles)"))) {
                while (info.next()) {
                    if (info.value(1).toString() == QStringLiteral("color")) {
                        hasColorColumn = true;
                        break;
                    }
                }
            }
        }

        if (!hasColorColumn) {
            QSqlQuery migrate(*m_db);
            if (!migrate.exec(QStringLiteral(
                    "ALTER TABLE rectangles ADD COLUMN color TEXT NOT NULL DEFAULT ''"))) {
                failQuery(migrate, error,
                          QStringLiteral("Could not upgrade the database schema"));
                m_db->rollback();
                return false;
            }
            QSqlQuery backfill(*m_db);
            if (!backfill.exec(QStringLiteral(
                    "UPDATE rectangles SET color ="
                    " (SELECT color FROM records WHERE records.id = rectangles.record_id)"))) {
                failQuery(backfill, error,
                          QStringLiteral("Could not upgrade the database schema"));
                m_db->rollback();
                return false;
            }
        }
    }

    if (version < 3) {
        bool hasRotationColumn = false;
        {
            QSqlQuery info(*m_db);
            if (info.exec(QStringLiteral("PRAGMA table_info(rectangles)"))) {
                while (info.next()) {
                    if (info.value(1).toString() == QStringLiteral("rotation")) {
                        hasRotationColumn = true;
                        break;
                    }
                }
            }
        }

        if (!hasRotationColumn) {
            QSqlQuery migrate(*m_db);
            if (!migrate.exec(QStringLiteral(
                    "ALTER TABLE rectangles ADD COLUMN rotation REAL NOT NULL DEFAULT 0.0"))) {
                failQuery(migrate, error,
                          QStringLiteral("Could not upgrade the database schema"));
                m_db->rollback();
                return false;
            }
        }
    }

    QSqlQuery versionQuery(*m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version = 3"))) {
        failQuery(versionQuery, error, QStringLiteral("Could not set the schema version"));
        m_db->rollback();
        return false;
    }

    if (!m_db->commit()) {
        setError(error, QStringLiteral("Could not commit the schema: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    return true;
}

std::optional<Database::MapInfo> Database::map(QString *error) const
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return std::nullopt;
    }

    QSqlQuery query(*m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, address, center_lat, center_lon, ground_span_m,"
            " image_path, image_width, image_height, attribution, created_at"
            " FROM map WHERE id = 1"))) {
        failQuery(query, error, QStringLiteral("Could not load the map"));
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;

    MapInfo info;
    info.id = query.value(0).toInt();
    info.address = query.value(1).toString();
    info.centerLat = query.value(2).toDouble();
    info.centerLon = query.value(3).toDouble();
    info.groundSpanMeters = query.value(4).toDouble();
    info.imagePath = query.value(5).toString();
    info.imageWidth = query.value(6).toInt();
    info.imageHeight = query.value(7).toInt();
    info.attribution = query.value(8).toString();
    info.createdAt = query.value(9).toString();
    return info;
}

bool Database::saveMap(const MapInfo &info, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO map (id, address, center_lat, center_lon, ground_span_m,"
        " image_path, image_width, image_height, attribution, created_at)"
        " VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(info.address);
    query.addBindValue(info.centerLat);
    query.addBindValue(info.centerLon);
    query.addBindValue(info.groundSpanMeters);
    query.addBindValue(info.imagePath);
    query.addBindValue(info.imageWidth);
    query.addBindValue(info.imageHeight);
    query.addBindValue(info.attribution);
    query.addBindValue(info.createdAt.isEmpty() ? nowUtc() : info.createdAt);

    if (!query.exec()) {
        if (query.lastError().nativeErrorCode() == QStringLiteral("19")) {
            setError(error, QStringLiteral("A property map has already been saved."));
        } else {
            failQuery(query, error, QStringLiteral("Could not save the property map"));
        }
        return false;
    }
    return true;
}

bool Database::saveMapImagePath(const QString &imagePath, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral("UPDATE map SET image_path = ? WHERE id = 1"));
    query.addBindValue(imagePath);
    if (!query.exec()) {
        failQuery(query, error, QStringLiteral("Could not save the map image path"));
        return false;
    }
    return true;
}

bool Database::backupTo(const QString &destinationFile, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }

    // Produces a self-contained snapshot of the database (WAL included) that
    // can also be executed while other write activity is in progress.
    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(destinationFile);
    if (!query.exec()) {
        failQuery(query, error, QStringLiteral("Could not write the database snapshot"));
        return false;
    }
    return true;
}

std::optional<Database::Record> Database::draft(QString *error) const
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return std::nullopt;
    }

    QSqlQuery query(*m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, map_id, status, date, tag, color, notes, created_at, updated_at"
            " FROM records WHERE status = 'draft' LIMIT 1"))) {
        failQuery(query, error, QStringLiteral("Could not load the draft"));
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;

    Record record;
    record.id = query.value(0).toInt();
    record.mapId = query.value(1).toInt();
    record.status = query.value(2).toString();
    record.date = query.value(3).toString();
    record.tag = query.value(4).toString();
    record.color = query.value(5).toString();
    record.notes = query.value(6).toString();
    record.createdAt = query.value(7).toString();
    record.updatedAt = query.value(8).toString();
    return record;
}

bool Database::ensureDraft(QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }
    if (draft(error))
        return true;
    if (error && !error->isEmpty())
        return false;
    if (!map(error))
        return false;

    const QString now = nowUtc();
    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO records (map_id, status, date, tag, color, notes, created_at, updated_at)"
        " VALUES (1, 'draft', ?, '', ?, '', ?, ?)"));
    query.addBindValue(dateToday());
    query.addBindValue(QString::fromLatin1(DefaultColor));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec())
        return failQuery(query, error, QStringLiteral("Could not create the draft"));
    return true;
}

bool Database::saveDraftFields(int draftId, const Record &fields, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "UPDATE records SET date = ?, tag = ?, color = ?, notes = ?, updated_at = ?"
        " WHERE id = ? AND status = 'draft'"));
    query.addBindValue(nonNull(fields.date));
    query.addBindValue(nonNull(fields.tag));
    query.addBindValue(nonNull(fields.color));
    query.addBindValue(nonNull(fields.notes));
    query.addBindValue(nowUtc());
    query.addBindValue(draftId);
    if (!query.exec())
        return failQuery(query, error, QStringLiteral("Could not save the draft"));
    if (query.numRowsAffected() != 1) {
        setError(error, QStringLiteral("The draft no longer exists."));
        return false;
    }
    return true;
}

QVector<Database::Record> Database::committedRecords(QString *error) const
{
    QVector<Record> records;
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return records;
    }

    QSqlQuery query(*m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, map_id, status, date, tag, color, notes, created_at, updated_at"
            " FROM records WHERE status = 'committed'"
            " ORDER BY date DESC, created_at DESC, id DESC"))) {
        failQuery(query, error, QStringLiteral("Could not load records"));
        return records;
    }

    while (query.next()) {
        Record record;
        record.id = query.value(0).toInt();
        record.mapId = query.value(1).toInt();
        record.status = query.value(2).toString();
        record.date = query.value(3).toString();
        record.tag = query.value(4).toString();
        record.color = query.value(5).toString();
        record.notes = query.value(6).toString();
        record.createdAt = query.value(7).toString();
        record.updatedAt = query.value(8).toString();
        records.append(record);
    }
    return records;
}

std::optional<Database::Record> Database::record(int id, QString *error) const
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return std::nullopt;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "SELECT id, map_id, status, date, tag, color, notes, created_at, updated_at"
        " FROM records WHERE id = ? AND status = 'committed'"));
    query.addBindValue(id);
    if (!query.exec()) {
        failQuery(query, error, QStringLiteral("Could not load the record"));
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;

    Record record;
    record.id = query.value(0).toInt();
    record.mapId = query.value(1).toInt();
    record.status = query.value(2).toString();
    record.date = query.value(3).toString();
    record.tag = query.value(4).toString();
    record.color = query.value(5).toString();
    record.notes = query.value(6).toString();
    record.createdAt = query.value(7).toString();
    record.updatedAt = query.value(8).toString();
    return record;
}

QVector<Database::Rect> Database::rectangles(int recordId, QString *error) const
{
    QVector<Rect> rectangles;
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return rectangles;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "SELECT x, y, w, h, rotation, sort, color FROM rectangles"
        " WHERE record_id = ? ORDER BY sort, id"));
    query.addBindValue(recordId);
    if (!query.exec()) {
        failQuery(query, error, QStringLiteral("Could not load rectangles"));
        return rectangles;
    }

    while (query.next()) {
        Rect rect;
        rect.x = query.value(0).toDouble();
        rect.y = query.value(1).toDouble();
        rect.w = query.value(2).toDouble();
        rect.h = query.value(3).toDouble();
        rect.rotation = query.value(4).toDouble();
        rect.sort = query.value(5).toInt();
        rect.color = query.value(6).toString();
        rectangles.append(rect);
    }
    return rectangles;
}

bool Database::replaceRectangles(int recordId, const QVector<Rect> &rectangles, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }
    if (!m_db->transaction()) {
        setError(error, QStringLiteral("Could not start the rectangle transaction: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    QSqlQuery del(*m_db);
    del.prepare(QStringLiteral("DELETE FROM rectangles WHERE record_id = ?"));
    del.addBindValue(recordId);
    if (!del.exec()) {
        failQuery(del, error, QStringLiteral("Could not replace rectangles"));
        m_db->rollback();
        return false;
    }

    QSqlQuery insert(*m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO rectangles (record_id, x, y, w, h, rotation, sort, color)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; i < rectangles.size(); ++i) {
        const Rect &rect = rectangles.at(i);
        insert.addBindValue(recordId);
        insert.addBindValue(rect.x);
        insert.addBindValue(rect.y);
        insert.addBindValue(rect.w);
        insert.addBindValue(rect.h);
        insert.addBindValue(rect.rotation);
        insert.addBindValue(i);
        insert.addBindValue(nonNull(rect.color));
        if (!insert.exec()) {
            failQuery(insert, error, QStringLiteral("Could not save rectangles"));
            m_db->rollback();
            return false;
        }
    }

    if (!m_db->commit()) {
        setError(error, QStringLiteral("Could not commit rectangles: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }
    return true;
}

bool Database::commitDraft(int draftId, const Record &fields, int *committedId, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }
    if (!m_db->transaction()) {
        setError(error, QStringLiteral("Could not start the commit transaction: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    const QString now = nowUtc();
    QSqlQuery update(*m_db);
    update.prepare(QStringLiteral(
        "UPDATE records SET status = 'committed', date = ?, tag = ?, color = ?, notes = ?,"
        " updated_at = ? WHERE id = ? AND status = 'draft'"));
    update.addBindValue(nonNull(fields.date));
    update.addBindValue(nonNull(fields.tag));
    update.addBindValue(nonNull(fields.color));
    update.addBindValue(nonNull(fields.notes));
    update.addBindValue(now);
    update.addBindValue(draftId);
    if (!update.exec()) {
        failQuery(update, error, QStringLiteral("Could not commit the draft"));
        m_db->rollback();
        return false;
    }
    if (update.numRowsAffected() != 1) {
        setError(error, QStringLiteral("The draft no longer exists."));
        m_db->rollback();
        return false;
    }

    QSqlQuery insert(*m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO records (map_id, status, date, tag, color, notes, created_at, updated_at)"
        " VALUES (1, 'draft', ?, '', ?, '', ?, ?)"));
    insert.addBindValue(dateToday());
    insert.addBindValue(QString::fromLatin1(DefaultColor));
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        failQuery(insert, error, QStringLiteral("Could not create a new draft"));
        m_db->rollback();
        return false;
    }

    if (!m_db->commit()) {
        setError(error, QStringLiteral("Could not commit the record: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    if (committedId)
        *committedId = draftId;
    return true;
}

bool Database::updateRecord(int recordId, const Record &fields,
                            const QVector<Rect> &rectangles, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }
    if (!m_db->transaction()) {
        setError(error, QStringLiteral("Could not start the update transaction: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    QSqlQuery update(*m_db);
    update.prepare(QStringLiteral(
        "UPDATE records SET date = ?, tag = ?, color = ?, notes = ?, updated_at = ?"
        " WHERE id = ? AND status = 'committed'"));
    update.addBindValue(nonNull(fields.date));
    update.addBindValue(nonNull(fields.tag));
    update.addBindValue(nonNull(fields.color));
    update.addBindValue(nonNull(fields.notes));
    update.addBindValue(nowUtc());
    update.addBindValue(recordId);
    if (!update.exec()) {
        failQuery(update, error, QStringLiteral("Could not update the record"));
        m_db->rollback();
        return false;
    }
    if (update.numRowsAffected() != 1) {
        setError(error, QStringLiteral("The record no longer exists."));
        m_db->rollback();
        return false;
    }

    QSqlQuery del(*m_db);
    del.prepare(QStringLiteral("DELETE FROM rectangles WHERE record_id = ?"));
    del.addBindValue(recordId);
    if (!del.exec()) {
        failQuery(del, error, QStringLiteral("Could not replace rectangles"));
        m_db->rollback();
        return false;
    }

    QSqlQuery insert(*m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO rectangles (record_id, x, y, w, h, rotation, sort, color)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; i < rectangles.size(); ++i) {
        const Rect &rect = rectangles.at(i);
        insert.addBindValue(recordId);
        insert.addBindValue(rect.x);
        insert.addBindValue(rect.y);
        insert.addBindValue(rect.w);
        insert.addBindValue(rect.h);
        insert.addBindValue(rect.rotation);
        insert.addBindValue(i);
        insert.addBindValue(nonNull(rect.color));
        if (!insert.exec()) {
            failQuery(insert, error, QStringLiteral("Could not save rectangles"));
            m_db->rollback();
            return false;
        }
    }

    if (!m_db->commit()) {
        setError(error, QStringLiteral("Could not update the record: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }
    return true;
}

bool Database::deleteRecord(int recordId, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "DELETE FROM records WHERE id = ? AND status = 'committed'"));
    query.addBindValue(recordId);
    if (!query.exec()) {
        failQuery(query, error, QStringLiteral("Could not delete the record"));
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setError(error, QStringLiteral("The record no longer exists."));
        return false;
    }
    return true;
}

bool Database::clearDraft(int draftId, const Record &defaults, QString *error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("The database is not open."));
        return false;
    }
    if (!m_db->transaction()) {
        setError(error, QStringLiteral("Could not start the clear transaction: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }

    QSqlQuery del(*m_db);
    del.prepare(QStringLiteral("DELETE FROM rectangles WHERE record_id = ?"));
    del.addBindValue(draftId);
    if (!del.exec()) {
        failQuery(del, error, QStringLiteral("Could not clear the draft rectangles"));
        m_db->rollback();
        return false;
    }

    QSqlQuery update(*m_db);
    update.prepare(QStringLiteral(
        "UPDATE records SET date = ?, tag = ?, color = ?, notes = ?, updated_at = ?"
        " WHERE id = ? AND status = 'draft'"));
    update.addBindValue(nonNull(defaults.date));
    update.addBindValue(QString::fromLatin1(""));
    update.addBindValue(QString::fromLatin1(DefaultColor));
    update.addBindValue(QString::fromLatin1(""));
    update.addBindValue(nowUtc());
    update.addBindValue(draftId);
    if (!update.exec()) {
        failQuery(update, error, QStringLiteral("Could not clear the draft"));
        m_db->rollback();
        return false;
    }

    if (!m_db->commit()) {
        setError(error, QStringLiteral("Could not clear the draft: %1")
                            .arg(m_db->lastError().text()));
        return false;
    }
    return true;
}
