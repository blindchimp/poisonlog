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
    void rectangleRotationRoundTrips();
    void mapImagePathIsRelativeToDatabase();
    void legacyAbsoluteImagePathIsMigrated();
    void backupProducesPortableDataFolder();
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

void TestAppStore::rectangleRotationRoundTrips()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-02"));
    store->setDraftTag(QStringLiteral("fungicide"));
    store->addDraftRectangle(0.2, 0.2, 0.2, 0.1);

    auto *rects = qobject_cast<RectangleListModel *>(store->rectanglesModel());
    Q_CHECK_PTR(rects);
    QCOMPARE(rects->rectangles().at(0).rotation, 0.0);

    store->updateDraftRectangleRotation(0, 337.5, false);
    QCOMPARE(rects->rectangles().at(0).rotation, 337.5);

    store->updateDraftRectangleRotation(0, -90.0, false);
    QCOMPARE(rects->rectangles().at(0).rotation, 270.0);

    store->updateDraftRectangleRotation(0, 725.0, false);
    QCOMPARE(rects->rectangles().at(0).rotation, 5.0);

    store->updateDraftRectangleRotation(0, 24.0, true);
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));

    // Rotation survives an app restart and a commit.
    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    const int recordId = store->viewedRecordId();

    store.reset();
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    store->selectRecord(recordId);

    auto *reloaded = qobject_cast<RectangleListModel *>(store->rectanglesModel());
    Q_CHECK_PTR(reloaded);
    QCOMPARE(reloaded->rectangles().at(0).rotation, 24.0);
}

void TestAppStore::mapImagePathIsRelativeToDatabase()
{
    const QString mapsDir = m_dir->filePath(QStringLiteral("maps"));
    const QString imagePath = QDir(mapsDir).filePath(QStringLiteral("property.png"));
    QDir().mkpath(mapsDir);
    QImage image(768, 768, QImage::Format_RGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(imagePath));

    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    store->finishMapSetup(QStringLiteral("1 Test Lane"), 45.0, -93.0, 70.0, imagePath, 768, 768);
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    QVERIFY(store->hasMap());
    QCOMPARE(store->mapImageUrl().toLocalFile(), imagePath);

    store.reset();
    {
        Database db(m_databasePath);
        QString error;
        QVERIFY2(db.open(&error), qPrintable(error));
        const auto info = db.map(&error);
        QVERIFY2(info.has_value(), qPrintable(error));
        QCOMPARE(info->imagePath, QStringLiteral("maps/property.png"));
    }

    // The resolved URL is unchanged after an app restart.
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    QVERIFY(store->hasMap());
    QCOMPARE(store->mapImageUrl().toLocalFile(), imagePath);

    // Restoring the data folder somewhere else keeps working.
    const QString movedRoot = m_dir->filePath(QStringLiteral("moved"));
    QVERIFY(QDir().mkpath(QDir(movedRoot).filePath(QStringLiteral("maps"))));
    QVERIFY(QFile::copy(m_databasePath, QDir(movedRoot).filePath(QStringLiteral("poisonlog.db"))));
    QVERIFY(QFile::copy(imagePath, QDir(movedRoot).filePath(
                QStringLiteral("maps/property.png"))));
    auto moved = std::make_unique<AppStore>(
        nullptr, QDir(movedRoot).filePath(QStringLiteral("poisonlog.db")));
    QVERIFY(moved->hasMap());
    QCOMPARE(moved->mapImageUrl().toLocalFile(),
             QDir(QDir(movedRoot).filePath(QStringLiteral("maps")))
                 .filePath(QStringLiteral("property.png")));
}

void TestAppStore::legacyAbsoluteImagePathIsMigrated()
{
    const QString mapsDir = m_dir->filePath(QStringLiteral("maps"));
    const QString imagePath = QDir(mapsDir).filePath(QStringLiteral("property.png"));
    QDir().mkpath(mapsDir);
    QImage image(768, 768, QImage::Format_RGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(imagePath));

    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    store->finishMapSetup(QStringLiteral("1 Test Lane"), 45.0, -93.0, 70.0, imagePath, 768, 768);
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    store.reset();

    // Rewind the stored path to the pre-portability format.
    {
        Database db(m_databasePath);
        QString error;
        QVERIFY2(db.open(&error), qPrintable(error));
        QVERIFY2(db.saveMapImagePath(imagePath, &error), qPrintable(error));
    }

    // Loading the map migrates the absolute path back to the relative form.
    store = std::make_unique<AppStore>(nullptr, m_databasePath);
    QVERIFY(store->hasMap());
    QCOMPARE(store->mapImageUrl().toLocalFile(), imagePath);

    store.reset();
    Database db(m_databasePath);
    QString error;
    QVERIFY2(db.open(&error), qPrintable(error));
    const auto info = db.map(&error);
    QVERIFY2(info.has_value(), qPrintable(error));
    QCOMPARE(info->imagePath, QStringLiteral("maps/property.png"));
}

void TestAppStore::backupProducesPortableDataFolder()
{
    auto store = std::make_unique<AppStore>(nullptr, m_databasePath);
    createMap(*store);

    store->setDraftDate(QStringLiteral("2026-04-03"));
    store->setDraftTag(QStringLiteral("fumigant"));
    store->addDraftRectangle(0.3, 0.3, 0.2, 0.2);
    store->updateDraftRectangleRotation(0, 45.0, true);
    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));

    const QString backupRoot = m_dir->filePath(QStringLiteral("backup"));
    const QString backupDb = QDir(backupRoot).filePath(QStringLiteral("poisonlog.db"));
    // The source image sits directly next to the database, so the backup
    // mirrors that layout rather than forcing the maps/ layout.
    const QString backupImage = QDir(backupRoot).filePath(QStringLiteral("property.png"));

    store->setBackupFolder(backupRoot);
    QCOMPARE(store->backupFolder(), backupRoot);

    store->createBackup();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    QVERIFY(QFile::exists(backupDb));
    QVERIFY(QFile::exists(backupImage));

    // The backup contains the committed record and its rotated rectangle.
    {
        Database backup(backupDb);
        QString error;
        QVERIFY2(backup.open(&error), qPrintable(error));
        const auto info = backup.map(&error);
        QVERIFY2(info.has_value(), qPrintable(error));
        QCOMPARE(info->address, QStringLiteral("1 Test Lane"));
        QCOMPARE(info->imagePath, QStringLiteral("property.png"));
        const auto records = backup.committedRecords(&error);
        QCOMPARE(records.size(), 1);
        QVERIFY2(records.at(0).tag == QStringLiteral("fumigant"), qPrintable(records.at(0).tag));
        const auto rectangles = backup.rectangles(records.at(0).id, &error);
        QCOMPARE(rectangles.size(), 1);
        QCOMPARE(rectangles.at(0).rotation, 45.0);
    }

    // Running the backup again overwrites the previous set without error.
    store->resumeDraft();
    store->setDraftDate(QStringLiteral("2026-04-03"));
    store->setDraftTag(QStringLiteral("fumigant"));
    store->addDraftRectangle(0.1, 0.1, 0.1, 0.1);
    store->commitDraft();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));
    store->createBackup();
    QVERIFY2(store->errorMessage().isEmpty(), qPrintable(store->errorMessage()));

    // The backup folder is a complete, self-contained data folder.
    auto restored = std::make_unique<AppStore>(nullptr, backupDb);
    QVERIFY(restored->hasMap());
    QCOMPARE(restored->mapImageUrl().toLocalFile(), backupImage);
    QCOMPARE(restored->recordCount(), 2);
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
