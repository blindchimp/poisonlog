#include "appstore.h"
#include "rectanglelistmodel.h"

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class TestAppStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void requiresMapAndRectangleToCommit();
    void commitThenPersistDraftAcrossNavigationAndRestart();
    void selectingRecordDoesNotDiscardDraft();
    void savesCommittedRecordEdits();
    void cancelsRecordEditWithoutPersisting();
    void rectangleColorsArePerRectangle();
    void deletesCommittedRecordAndKeepsDraft();

private:
    void createMap(AppStore &store);

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_mapPath;
    QString m_databasePath;
};

void TestAppStore::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("poisonlog-test"));
    QCoreApplication::setOrganizationName(QStringLiteral("PoisonLog"));
}

void TestAppStore::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_mapPath = m_dir->filePath(QStringLiteral("property.png"));
    m_databasePath = m_dir->filePath(QStringLiteral("poisonlog.db"));

    QImage image(768, 768, QImage::Format_RGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(m_mapPath));
}

void TestAppStore::cleanup()
{
    m_dir.reset();
}

void TestAppStore::createMap(AppStore &store)
{
    store.finishMapSetup(QStringLiteral("1 Test Lane"), 45.0, -93.0, 70.0,
                         m_mapPath, 768, 768);
    QVERIFY2(store.errorMessage().isEmpty(), qPrintable(store.errorMessage()));
    QVERIFY(store.hasMap());
    QCOMPARE(store.mode(), QStringLiteral("draft"));
}

void TestAppStore::requiresMapAndRectangleToCommit()
{
    AppStore store(nullptr, m_databasePath);
    QCOMPARE(store.mode(), QStringLiteral("setup"));
    QVERIFY(!store.hasMap());

    store.commitDraft();
    QVERIFY(!store.errorMessage().isEmpty());

    createMap(store);
    QVERIFY(!store.canCommit());

    store.setDraftTag(QStringLiteral("herbicide"));
    QVERIFY(!store.canCommit());

    store.setDraftDate(QStringLiteral("not-a-date"));
    store.addDraftRectangle(0.1, 0.1, 0.2, 0.2);
    QVERIFY(!store.canCommit());

    store.setDraftDate(QStringLiteral("2026-04-01"));
    QVERIFY(store.canCommit());
    QCOMPARE(store.rectangleCount(), 1);
    QCOMPARE(store.draftHasContent(), true);
}

void TestAppStore::commitThenPersistDraftAcrossNavigationAndRestart()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-01"));
    store->setDraftTag(QStringLiteral("herbicide"));
    store->setDraftNotes(QStringLiteral("Sprayed the north fence"));
    store->setDraftColor(QStringLiteral("#c62828"));
    store->addDraftRectangle(0.1, 0.1, 0.25, 0.25);
    QVERIFY(store->canCommit());

    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->recordCount(), 1);
    QCOMPARE(store->draftNotes(), QString());
    QCOMPARE(store->rectangleCount(), 1);

    const int committedId = store->viewedRecordId();
    QVERIFY(committedId > 0);
    QCOMPARE(store->viewedRecord().value(QStringLiteral("tag")).toString(),
             QStringLiteral("herbicide"));

    store->resumeDraft();
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->rectangleCount(), 0);
    QVERIFY(!store->draftHasContent());

    store->setDraftNotes(QStringLiteral("Unfinished field notes"));
    store->addDraftRectangle(0.4, 0.4, 0.1, 0.1);
    QVERIFY(store->draftHasContent());
    store->selectRecord(committedId);
    QCOMPARE(store->mode(), QStringLiteral("record"));
    store->resumeDraft();
    QCOMPARE(store->draftNotes(), QStringLiteral("Unfinished field notes"));
    QCOMPARE(store->rectangleCount(), 1);

    store->selectRecord(committedId);
    QCOMPARE(store->mode(), QStringLiteral("record"));

    store.reset();
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->draftNotes(), QStringLiteral("Unfinished field notes"));
    QCOMPARE(store->rectangleCount(), 1);
    QCOMPARE(store->recordCount(), 1);
}

void TestAppStore::selectingRecordDoesNotDiscardDraft()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-05-05"));
    store->setDraftTag(QStringLiteral("fertilizer"));
    store->setDraftNotes(QStringLiteral("Committed note"));
    store->addDraftRectangle(0.2, 0.2, 0.2, 0.2);
    store->commitDraft();
    QCOMPARE(store->mode(), QStringLiteral("record"));
    const int recordId = store->viewedRecordId();

    store->resumeDraft();
    store->setDraftNotes(QStringLiteral("Half-written note"));
    store->addDraftRectangle(0.5, 0.5, 0.15, 0.15);

    store->selectRecord(recordId);
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->rectangleCount(), 1);

    store->resumeDraft();
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->draftNotes(), QStringLiteral("Half-written note"));
    QCOMPARE(store->rectangleCount(), 1);

    store->clearDraft();
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->draftNotes(), QString());
    QCOMPARE(store->rectangleCount(), 0);
    QVERIFY(!store->draftHasContent());
    QCOMPARE(store->recordCount(), 1);
}

void TestAppStore::savesCommittedRecordEdits()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-01"));
    store->setDraftTag(QStringLiteral("herbicide"));
    store->setDraftNotes(QStringLiteral("Original notes"));
    store->addDraftRectangle(0.1, 0.1, 0.2, 0.2);
    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    const int recordId = store->viewedRecordId();
    QCOMPARE(store->mode(), QStringLiteral("record"));

    // Leave uncommitted draft content that must not be touched by the edit.
    store->resumeDraft();
    store->setDraftTag(QStringLiteral("draft tag"));
    store->setDraftNotes(QStringLiteral("Draft note"));
    store->addDraftRectangle(0.6, 0.6, 0.1, 0.1);
    store->selectRecord(recordId);
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->rectangleCount(), 1);

    store->startRecordEdit();
    QCOMPARE(store->mode(), QStringLiteral("record-edit"));
    QCOMPARE(store->editTag(), QStringLiteral("herbicide"));
    QCOMPARE(store->editDate(), QStringLiteral("2026-04-01"));
    QCOMPARE(store->editNotes(), QStringLiteral("Original notes"));
    QCOMPARE(store->rectangleCount(), 1);
    QVERIFY(store->canSaveEdit());

    store->setEditTag(QStringLiteral("fungicide"));
    store->setEditNotes(QStringLiteral("Updated notes"));
    store->addDraftRectangle(0.5, 0.5, 0.2, 0.2);
    QCOMPARE(store->rectangleCount(), 2);

    // Editing a record must not modify the separate draft.
    QCOMPARE(store->draftTag(), QStringLiteral("draft tag"));
    QCOMPARE(store->draftNotes(), QStringLiteral("Draft note"));
    QCOMPARE(store->draftHasContent(), true);

    store->saveRecordEdit();
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->viewedRecord().value(QStringLiteral("tag")).toString(),
             QStringLiteral("fungicide"));
    QCOMPARE(store->viewedRecord().value(QStringLiteral("notes")).toString(),
             QStringLiteral("Updated notes"));
    QCOMPARE(store->rectangleCount(), 2);
    QCOMPARE(store->recordCount(), 1);

    store.reset();
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->draftNotes(), QStringLiteral("Draft note"));
    QCOMPARE(store->rectangleCount(), 1);

    store->selectRecord(recordId);
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->viewedRecord().value(QStringLiteral("tag")).toString(),
             QStringLiteral("fungicide"));
    QCOMPARE(store->rectangleCount(), 2);
}

void TestAppStore::cancelsRecordEditWithoutPersisting()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-01"));
    store->setDraftTag(QStringLiteral("herbicide"));
    store->setDraftNotes(QStringLiteral("Original notes"));
    store->addDraftRectangle(0.1, 0.1, 0.2, 0.2);
    store->commitDraft();
    const int recordId = store->viewedRecordId();

    store->startRecordEdit();
    store->setEditNotes(QStringLiteral("Temporary edit"));
    store->addDraftRectangle(0.7, 0.7, 0.1, 0.1);
    store->removeDraftRectangle(1);
    QCOMPARE(store->rectangleCount(), 1);

    store->cancelRecordEdit();
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->viewedRecord().value(QStringLiteral("notes")).toString(),
             QStringLiteral("Original notes"));
    QCOMPARE(store->rectangleCount(), 1);

    store.reset();
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    store->selectRecord(recordId);
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->viewedRecord().value(QStringLiteral("notes")).toString(),
             QStringLiteral("Original notes"));
    QCOMPARE(store->rectangleCount(), 1);
}

void TestAppStore::rectangleColorsArePerRectangle()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-01"));
    store->setDraftTag(QStringLiteral("herbicide"));

    store->addDraftRectangle(0.1, 0.1, 0.2, 0.2);
    store->setDraftColor(QStringLiteral("#c62828"));
    store->addDraftRectangle(0.4, 0.4, 0.2, 0.2);
    store->setDraftColor(QStringLiteral("#1565c0"));
    store->addDraftRectangle(0.7, 0.7, 0.2, 0.2);

    auto *rects = qobject_cast<RectangleListModel *>(store->rectanglesModel());
    Q_CHECK_PTR(rects);
    QCOMPARE(rects->rectangles().at(0).color, QString::fromLatin1(Database::DefaultColor));
    QCOMPARE(rects->rectangles().at(1).color, QStringLiteral("#c62828"));
    QCOMPARE(rects->rectangles().at(2).color, QStringLiteral("#1565c0"));

    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    const int recordId = store->viewedRecordId();

    store.reset();
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    store->selectRecord(recordId);
    QCOMPARE(store->mode(), QStringLiteral("record"));

    auto *reloaded = qobject_cast<RectangleListModel *>(store->rectanglesModel());
    Q_CHECK_PTR(reloaded);
    QCOMPARE(reloaded->rectangles().at(0).color, QString::fromLatin1(Database::DefaultColor));
    QCOMPARE(reloaded->rectangles().at(1).color, QStringLiteral("#c62828"));
    QCOMPARE(reloaded->rectangles().at(2).color, QStringLiteral("#1565c0"));
}

void TestAppStore::deletesCommittedRecordAndKeepsDraft()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-01"));
    store->setDraftTag(QStringLiteral("herbicide"));
    store->addDraftRectangle(0.1, 0.1, 0.2, 0.2);
    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    const int recordId = store->viewedRecordId();

    store->resumeDraft();
    store->setDraftNotes(QStringLiteral("Draft note"));
    store->addDraftRectangle(0.6, 0.6, 0.1, 0.1);

    store->selectRecord(recordId);
    QCOMPARE(store->mode(), QStringLiteral("record"));
    QCOMPARE(store->recordCount(), 1);

    store->deleteViewedRecord();
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->recordCount(), 0);
    QCOMPARE(store->viewedRecordId(), -1);
    QCOMPARE(store->draftNotes(), QStringLiteral("Draft note"));
    QCOMPARE(store->rectangleCount(), 1);

    store.reset();
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    QCOMPARE(store->mode(), QStringLiteral("draft"));
    QCOMPARE(store->recordCount(), 0);
    QCOMPARE(store->draftNotes(), QStringLiteral("Draft note"));
    QCOMPARE(store->rectangleCount(), 1);
}

QTEST_GUILESS_MAIN(TestAppStore)

#include "tst_appstore.moc"
