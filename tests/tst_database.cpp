#include "database.h"

#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void createsSchemaAndDraft();
    void commitsDraftAndOrdersRecords();
    void clearsDraft();
    void updatesCommittedRecord();
    void migratesLegacyRectangles();
    void deletesCommittedRecord();

private:
    Database::MapInfo makeMapInfo() const;
    void seedMap();

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

void TestDatabase::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_db = std::make_unique<Database>(m_dir->filePath(QStringLiteral("test.db")));
    QString error;
    QVERIFY2(m_db->open(&error), qPrintable(error));
}

void TestDatabase::cleanup()
{
    m_db.reset();
    m_dir.reset();
}

Database::MapInfo TestDatabase::makeMapInfo() const
{
    Database::MapInfo info;
    info.address = QStringLiteral("1 Test Lane");
    info.centerLat = 45.0;
    info.centerLon = -93.0;
    info.groundSpanMeters = 70.0;
    info.imagePath = m_dir->filePath(QStringLiteral("map.png"));
    info.imageWidth = 768;
    info.imageHeight = 768;
    info.attribution = QStringLiteral("© OpenStreetMap contributors");
    info.createdAt = QStringLiteral("2026-01-01T00:00:00.000Z");
    return info;
}

void TestDatabase::seedMap()
{
    QString error;
    QVERIFY2(m_db->saveMap(makeMapInfo(), &error), qPrintable(error));
    QVERIFY2(m_db->ensureDraft(&error), qPrintable(error));
}

void TestDatabase::createsSchemaAndDraft()
{
    QString error;
    QVERIFY(!m_db->map(&error).has_value());
    QVERIFY(error.isEmpty());

    seedMap();
    const auto map = m_db->map(&error);
    QVERIFY2(map.has_value(), qPrintable(error));
    QCOMPARE(map->address, QStringLiteral("1 Test Lane"));
    QCOMPARE(map->imageWidth, 768);

    const auto draft = m_db->draft(&error);
    QVERIFY2(draft.has_value(), qPrintable(error));
    QVERIFY(!draft->date.isEmpty());
    QCOMPARE(draft->tag, QString());
    QCOMPARE(draft->color, QString::fromLatin1(Database::DefaultColor));

    Database::Record fields;
    fields.date = QStringLiteral("2026-03-04");
    fields.tag = QStringLiteral("herbicide");
    fields.color = QStringLiteral("#c62828");
    fields.notes = QStringLiteral("Applied along the fence");
    QVERIFY2(m_db->saveDraftFields(draft->id, fields, &error), qPrintable(error));

    Database::Rect rect;
    rect.x = 0.1;
    rect.y = 0.2;
    rect.w = 0.3;
    rect.h = 0.4;
    rect.rotation = 45.0;
    QVERIFY2(m_db->replaceRectangles(draft->id, { rect }, &error), qPrintable(error));

    const auto reloaded = m_db->draft(&error);
    QVERIFY(reloaded.has_value());
    QCOMPARE(reloaded->date, fields.date);
    QCOMPARE(reloaded->tag, fields.tag);
    QCOMPARE(reloaded->notes, fields.notes);

    const auto rectangles = m_db->rectangles(draft->id, &error);
    QCOMPARE(rectangles.size(), 1);
    QCOMPARE(rectangles.first().w, 0.3);
    QCOMPARE(rectangles.first().rotation, 45.0);
}

void TestDatabase::commitsDraftAndOrdersRecords()
{
    seedMap();
    QString error;

    auto draft = m_db->draft(&error);
    QVERIFY(draft.has_value());

    Database::Record fields;
    fields.date = QStringLiteral("2026-01-10");
    fields.tag = QStringLiteral("first");
    fields.color = QStringLiteral("#1565c0");
    fields.notes = QStringLiteral("First record");
    Database::Rect rect;
    rect.w = 0.2;
    rect.h = 0.2;
    QVERIFY(m_db->replaceRectangles(draft->id, { rect }, &error));
    QVERIFY2(m_db->commitDraft(draft->id, fields, nullptr, &error), qPrintable(error));

    draft = m_db->draft(&error);
    QVERIFY(draft.has_value());
    QVERIFY(draft->id != 0);
    QCOMPARE(draft->tag, QString());
    QCOMPARE(m_db->rectangles(draft->id, &error).size(), 0);

    fields.date = QStringLiteral("2026-02-20");
    fields.tag = QStringLiteral("second");
    fields.color = QStringLiteral("#2e7d32");
    fields.notes = QStringLiteral("Second record");
    QVERIFY(m_db->replaceRectangles(draft->id, { rect }, &error));
    QVERIFY2(m_db->commitDraft(draft->id, fields, nullptr, &error), qPrintable(error));

    const auto records = m_db->committedRecords(&error);
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0).tag, QStringLiteral("second"));
    QCOMPARE(records.at(1).tag, QStringLiteral("first"));
    QCOMPARE(m_db->rectangles(records.at(0).id, &error).size(), 1);

    draft = m_db->draft(&error);
    QVERIFY(draft.has_value());
    QCOMPARE(m_db->rectangles(draft->id, &error).size(), 0);
}

void TestDatabase::clearsDraft()
{
    seedMap();
    QString error;
    const auto draft = m_db->draft(&error);
    QVERIFY(draft.has_value());

    Database::Record fields;
    fields.date = QStringLiteral("2026-03-04");
    fields.tag = QStringLiteral("draft tag");
    fields.color = QStringLiteral("#f9a825");
    fields.notes = QStringLiteral("draft notes");
    QVERIFY(m_db->saveDraftFields(draft->id, fields, &error));
    Database::Rect rect;
    rect.w = 0.1;
    rect.h = 0.1;
    QVERIFY(m_db->replaceRectangles(draft->id, { rect }, &error));

    Database::Record defaults;
    defaults.date = QDate::currentDate().toString(Qt::ISODate);
    QVERIFY2(m_db->clearDraft(draft->id, defaults, &error), qPrintable(error));

    const auto cleared = m_db->draft(&error);
    QVERIFY(cleared.has_value());
    QCOMPARE(cleared->tag, QString());
    QCOMPARE(cleared->notes, QString());
    QCOMPARE(cleared->date, defaults.date);
    QCOMPARE(m_db->rectangles(cleared->id, &error).size(), 0);
}

void TestDatabase::updatesCommittedRecord()
{
    seedMap();
    QString error;
    auto draft = m_db->draft(&error);
    QVERIFY(draft.has_value());

    Database::Record fields;
    fields.date = QStringLiteral("2026-01-10");
    fields.tag = QStringLiteral("first");
    fields.color = QStringLiteral("#1565c0");
    fields.notes = QStringLiteral("Original notes");
    Database::Rect rect;
    rect.x = 0.1;
    rect.y = 0.1;
    rect.w = 0.2;
    rect.h = 0.2;
    QVERIFY(m_db->replaceRectangles(draft->id, { rect }, &error));

    const int committedId = draft->id;
    draft.reset();
    QVERIFY2(m_db->commitDraft(committedId, fields, nullptr, &error), qPrintable(error));
    const auto record = m_db->record(committedId, &error);
    QVERIFY2(record.has_value(), qPrintable(error));

    Database::Record newFields;
    newFields.date = QStringLiteral("2026-02-02");
    newFields.tag = QStringLiteral("updated");
    newFields.color = QStringLiteral("#c62828");
    newFields.notes = QStringLiteral("New notes");
    Database::Rect r1;
    r1.x = 0.0;
    r1.y = 0.0;
    r1.w = 0.1;
    r1.h = 0.1;
    r1.rotation = 45.0;
    r1.color = QStringLiteral("#2e7d32");
    Database::Rect r2;
    r2.x = 0.8;
    r2.y = 0.8;
    r2.w = 0.15;
    r2.h = 0.15;
    r2.rotation = 300.0;
    r2.color = QStringLiteral("#f9a825");
    QVERIFY2(m_db->updateRecord(committedId, newFields, { r1, r2 }, &error), qPrintable(error));

    const auto reloaded = m_db->record(committedId, &error);
    QVERIFY(reloaded.has_value());
    QCOMPARE(reloaded->date, newFields.date);
    QCOMPARE(reloaded->tag, newFields.tag);
    QCOMPARE(reloaded->color, newFields.color);
    QCOMPARE(reloaded->notes, newFields.notes);

    const auto rectangles = m_db->rectangles(committedId, &error);
    QCOMPARE(rectangles.size(), 2);
    QCOMPARE(rectangles.at(0).x, r1.x);
    QCOMPARE(rectangles.at(0).w, r1.w);
    QCOMPARE(rectangles.at(0).rotation, 45.0);
    QCOMPARE(rectangles.at(0).color, QStringLiteral("#2e7d32"));
    QCOMPARE(rectangles.at(1).x, r2.x);
    QCOMPARE(rectangles.at(1).w, r2.w);
    QCOMPARE(rectangles.at(1).rotation, 300.0);
    QCOMPARE(rectangles.at(1).color, QStringLiteral("#f9a825"));

    // Updating a draft record is not allowed.
    const auto currentDraft = m_db->draft(&error);
    QVERIFY(currentDraft.has_value());
    QVERIFY(currentDraft->id != committedId);
    error.clear();
    QVERIFY(!m_db->updateRecord(currentDraft->id, newFields, { r1 }, &error));
    QVERIFY(!error.isEmpty());
}

void TestDatabase::migratesLegacyRectangles()
{
    // Simulate a database created before per-rectangle colors existed:
    // legacy schema (user_version 1) with rectangles lacking a color column.
    const QString legacyPath = m_dir->filePath(QStringLiteral("legacy.db"));
    {
        const QString connection = QStringLiteral("legacy");
        QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        legacy.setDatabaseName(legacyPath);
        QVERIFY(legacy.open());
        {
            QSqlQuery query(legacy);

            const QStringList statements = {
                QStringLiteral(
                    "CREATE TABLE map ("
                    " id INTEGER PRIMARY KEY CHECK (id = 1),"
                    " address TEXT NOT NULL,"
                    " center_lat REAL NOT NULL,"
                    " center_lon REAL NOT NULL,"
                    " ground_span_m REAL NOT NULL,"
                    " image_path TEXT NOT NULL,"
                    " image_width INTEGER NOT NULL,"
                    " image_height INTEGER NOT NULL,"
                    " attribution TEXT NOT NULL,"
                    " created_at TEXT NOT NULL)"),
                QStringLiteral(
                    "CREATE TABLE records ("
                    " id INTEGER PRIMARY KEY,"
                    " map_id INTEGER NOT NULL DEFAULT 1 REFERENCES map(id) ON DELETE CASCADE,"
                    " status TEXT NOT NULL CHECK (status IN ('draft', 'committed')),"
                    " date TEXT NOT NULL,"
                    " tag TEXT NOT NULL,"
                    " color TEXT NOT NULL,"
                    " notes TEXT NOT NULL,"
                    " created_at TEXT NOT NULL,"
                    " updated_at TEXT NOT NULL)"),
                QStringLiteral(
                    "CREATE TABLE rectangles ("
                    " id INTEGER PRIMARY KEY,"
                    " record_id INTEGER NOT NULL REFERENCES records(id) ON DELETE CASCADE,"
                    " x REAL NOT NULL,"
                    " y REAL NOT NULL,"
                    " w REAL NOT NULL,"
                    " h REAL NOT NULL,"
                    " sort INTEGER NOT NULL)"),
                QStringLiteral(
                    "INSERT INTO map VALUES (1, '1 Test Lane', 45.0, -93.0, 70.0,"
                    " 'map.png', 768, 768, 'attribution', '2026-01-01T00:00:00.000Z')"),
                QStringLiteral(
                    "INSERT INTO records (status, date, tag, color, notes, created_at, updated_at)"
                    " VALUES ('committed', '2026-01-10', 'legacy tag', '#1565c0',"
                    " 'legacy notes', '2026-01-10T00:00:00.000Z', '2026-01-10T00:00:00.000Z')"),
                QStringLiteral("INSERT INTO rectangles (record_id, x, y, w, h, sort)"
                               " VALUES (1, 0.1, 0.1, 0.2, 0.2, 0)"),
                QStringLiteral("PRAGMA user_version = 1"),
            };
            for (const QString &sql : statements)
                QVERIFY2(query.exec(sql), qPrintable(sql));
        }
        legacy.close();
        QSqlDatabase::removeDatabase(connection);
    }

    m_db = std::make_unique<Database>(legacyPath);
    QString error;
    QVERIFY2(m_db->open(&error), qPrintable(error));

    // The draft is absent, but a new one can be created now that migration succeeded.
    QVERIFY2(m_db->ensureDraft(&error), qPrintable(error));
    const auto draft = m_db->draft(&error);
    QVERIFY(draft.has_value());

    // Legacy rectangles inherit the color of their owning record.
    const auto rectangles = m_db->rectangles(1, &error);
    QCOMPARE(rectangles.size(), 1);
    QCOMPARE(rectangles.first().color, QStringLiteral("#1565c0"));
    QCOMPARE(rectangles.first().rotation, 0.0);

    // New rectangles round-trip the rotation column added by the migration.
    Database::Rect rect;
    rect.w = 0.1;
    rect.h = 0.1;
    rect.rotation = 90.0;
    QVERIFY2(m_db->replaceRectangles(1, { rect }, &error), qPrintable(error));
    const auto migrated = m_db->rectangles(1, &error);
    QCOMPARE(migrated.size(), 1);
    QCOMPARE(migrated.first().rotation, 90.0);
}

void TestDatabase::deletesCommittedRecord()
{
    seedMap();
    QString error;

    Database::Record fields;
    fields.date = QStringLiteral("2026-01-10");
    fields.tag = QStringLiteral("first");
    Database::Rect rect;
    rect.w = 0.2;
    rect.h = 0.2;
    rect.color = QStringLiteral("#2e7d32");

    auto draft = m_db->draft(&error);
    QVERIFY(draft.has_value());
    QVERIFY(m_db->replaceRectangles(draft->id, { rect }, &error));
    const int firstId = draft->id;
    draft.reset();
    QVERIFY2(m_db->commitDraft(firstId, fields, nullptr, &error), qPrintable(error));

    draft = m_db->draft(&error);
    QVERIFY(draft.has_value());
    fields.tag = QStringLiteral("second");
    QVERIFY(m_db->replaceRectangles(draft->id, { rect }, &error));
    const int secondDraftId = draft->id;
    draft.reset();
    QVERIFY2(m_db->commitDraft(secondDraftId, fields, nullptr, &error), qPrintable(error));

    QCOMPARE(m_db->committedRecords(&error).size(), 2);
    QVERIFY2(m_db->deleteRecord(firstId, &error), qPrintable(error));
    QCOMPARE(m_db->committedRecords(&error).size(), 1);
    QCOMPARE(m_db->committedRecords(&error).first().tag, QStringLiteral("second"));
    QCOMPARE(m_db->rectangles(firstId, &error).size(), 0);
    QVERIFY(!m_db->record(firstId, &error).has_value());

    // Draft records cannot be deleted through this path.
    draft = m_db->draft(&error);
    QVERIFY(draft.has_value());
    error.clear();
    QVERIFY(!m_db->deleteRecord(draft->id, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(m_db->committedRecords(&error).size(), 1);
}

QTEST_GUILESS_MAIN(TestDatabase)

#include "tst_database.moc"
