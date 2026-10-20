#include "database.h"

#include <QDate>
#include <QTemporaryDir>
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
    QVERIFY2(m_db->replaceRectangles(draft->id, { rect }, &error), qPrintable(error));

    const auto reloaded = m_db->draft(&error);
    QVERIFY(reloaded.has_value());
    QCOMPARE(reloaded->date, fields.date);
    QCOMPARE(reloaded->tag, fields.tag);
    QCOMPARE(reloaded->notes, fields.notes);

    const auto rectangles = m_db->rectangles(draft->id, &error);
    QCOMPARE(rectangles.size(), 1);
    QCOMPARE(rectangles.first().w, 0.3);
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

QTEST_GUILESS_MAIN(TestDatabase)

#include "tst_database.moc"
