#pragma once

#include "database.h"
#include "geocoder.h"
#include "recordlistmodel.h"
#include "rectanglelistmodel.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class AppStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasMap READ hasMap NOTIFY mapChanged)
    Q_PROPERTY(QString mapAddress READ mapAddress NOTIFY mapChanged)
    Q_PROPERTY(QUrl mapImageUrl READ mapImageUrl NOTIFY mapChanged)
    Q_PROPERTY(int mapImageWidth READ mapImageWidth NOTIFY mapChanged)
    Q_PROPERTY(int mapImageHeight READ mapImageHeight NOTIFY mapChanged)
    Q_PROPERTY(QString mapAttribution READ mapAttribution NOTIFY mapChanged)

    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)

    Q_PROPERTY(QString draftDate READ draftDate WRITE setDraftDate NOTIFY draftDateChanged)
    Q_PROPERTY(QString draftTag READ draftTag WRITE setDraftTag NOTIFY draftTagChanged)
    Q_PROPERTY(QString draftNotes READ draftNotes WRITE setDraftNotes NOTIFY draftNotesChanged)
    Q_PROPERTY(QString draftColor READ draftColor WRITE setDraftColor NOTIFY draftColorChanged)

    Q_PROPERTY(bool draftHasContent READ draftHasContent NOTIFY draftContentChanged)
    Q_PROPERTY(bool canCommit READ canCommit NOTIFY canCommitChanged)

    Q_PROPERTY(int viewedRecordId READ viewedRecordId NOTIFY viewedRecordChanged)
    Q_PROPERTY(QVariantMap viewedRecord READ viewedRecord NOTIFY viewedRecordChanged)

    Q_PROPERTY(QString editDate READ editDate WRITE setEditDate NOTIFY editDateChanged)
    Q_PROPERTY(QString editTag READ editTag WRITE setEditTag NOTIFY editTagChanged)
    Q_PROPERTY(QString editNotes READ editNotes WRITE setEditNotes NOTIFY editNotesChanged)
    Q_PROPERTY(QString editColor READ editColor WRITE setEditColor NOTIFY editColorChanged)
    Q_PROPERTY(bool editingRecord READ editingRecord NOTIFY modeChanged)
    Q_PROPERTY(bool canSaveEdit READ canSaveEdit NOTIFY canSaveEditChanged)

    Q_PROPERTY(QObject *recordsModel READ recordsModel CONSTANT)
    Q_PROPERTY(QObject *rectanglesModel READ rectanglesModel CONSTANT)
    Q_PROPERTY(int recordCount READ recordCount NOTIFY recordCountChanged)
    Q_PROPERTY(int rectangleCount READ rectangleCount NOTIFY rectangleCountChanged)

    Q_PROPERTY(int selectedRectangleIndex READ selectedRectangleIndex NOTIFY selectedRectangleChanged)
    Q_PROPERTY(bool hasSelectedRectangle READ hasSelectedRectangle NOTIFY selectedRectangleChanged)
    Q_PROPERTY(QString currentRectangleColor READ currentRectangleColor NOTIFY currentRectangleColorChanged)

    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString backupFolder READ backupFolder WRITE setBackupFolder NOTIFY backupFolderChanged)

    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)

public:
    explicit AppStore(QObject *parent = nullptr, const QString &databasePath = QString());
    ~AppStore() override;

    bool hasMap() const;
    QString mapAddress() const;
    QUrl mapImageUrl() const;
    int mapImageWidth() const;
    int mapImageHeight() const;
    QString mapAttribution() const;

    QString mode() const;

    QString draftDate() const;
    QString draftTag() const;
    QString draftNotes() const;
    QString draftColor() const;

    bool draftHasContent() const;
    bool canCommit() const;

    int viewedRecordId() const;
    QVariantMap viewedRecord() const;

    QString editDate() const;
    QString editTag() const;
    QString editNotes() const;
    QString editColor() const;
    bool editingRecord() const;
    bool canSaveEdit() const;

    QObject *recordsModel();
    QObject *rectanglesModel();
    int recordCount() const;
    int rectangleCount() const;

    int selectedRectangleIndex() const;
    bool hasSelectedRectangle() const;
    QString currentRectangleColor() const;

    QString statusMessage() const;
    QString errorMessage() const;
    QString backupFolder() const;

    bool searching() const;
    QVariantList searchResults() const;

    void setDraftDate(const QString &date);
    void setDraftTag(const QString &tag);
    void setDraftNotes(const QString &notes);
    void setDraftColor(const QString &color);

    void setBackupFolder(const QString &folder);

    void setEditDate(const QString &date);
    void setEditTag(const QString &tag);
    void setEditNotes(const QString &notes);
    void setEditColor(const QString &color);

    Q_INVOKABLE QString newMapImagePath();
    Q_INVOKABLE void finishMapSetup(const QString &address, double latitude, double longitude,
                                    double groundSpanMeters, const QString &imagePath,
                                    int imageWidth, int imageHeight);

    Q_INVOKABLE void searchAddress(const QString &query);

    Q_INVOKABLE void selectRecord(int recordId);
    Q_INVOKABLE void resumeDraft();
    Q_INVOKABLE void clearDraft();
    Q_INVOKABLE void commitDraft();

    Q_INVOKABLE void startRecordEdit();
    Q_INVOKABLE void cancelRecordEdit();
    Q_INVOKABLE void saveRecordEdit();
    Q_INVOKABLE void deleteViewedRecord();

    Q_INVOKABLE void addDraftRectangle(double x, double y, double width, double height);
    Q_INVOKABLE void updateDraftRectangle(int index, double x, double y, double width,
                                          double height, bool persist);
    Q_INVOKABLE void updateDraftRectangleRotation(int index, double degrees, bool persist);
    Q_INVOKABLE void saveDraftRectangles();
    Q_INVOKABLE void removeDraftRectangle(int index);
    Q_INVOKABLE void deleteSelectedRectangle();
    Q_INVOKABLE void selectRectangle(int index);

    Q_INVOKABLE void reportError(const QString &message);
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE void createBackup();

signals:
    void mapChanged();
    void modeChanged();
    void draftDateChanged();
    void draftTagChanged();
    void draftNotesChanged();
    void draftColorChanged();
    void draftContentChanged();
    void canCommitChanged();
    void viewedRecordChanged();
    void editDateChanged();
    void editTagChanged();
    void editNotesChanged();
    void editColorChanged();
    void canSaveEditChanged();
    void recordCountChanged();
    void rectangleCountChanged();
    void selectedRectangleChanged();
    void currentRectangleColorChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void backupFolderChanged();
    void searchingChanged();
    void searchResultsChanged();

private:
    void loadInitialState();
    bool loadMap();
    bool loadDraft(bool loadRectangles);
    void loadDraftRectangles();
    void loadRecordRectangles(int recordId);
    void refreshRecords();
    void setMode(const QString &mode);
    void persistDraftFields();
    void persistDraftRectangles();
    void persistMapImagePath();
    void setStatus(const QString &message);
    void setError(const QString &message);
    void resetViewedRecord();
    QVariantMap recordToMap(const Database::Record &record) const;
    QString resolveMapImagePath(const QString &stored) const;
    QString relativeImagePath(const QString &resolved) const;
    static QString today();
    static double clamp01(double value);

    Database *m_database = nullptr;
    QString m_dataRoot;
    RecordListModel m_records;
    RectangleListModel m_rectangles;
    Geocoder m_geocoder;

    Database::MapInfo m_map;
    bool m_hasMap = false;

    QString m_mode = QStringLiteral("setup");

    int m_draftId = 0;
    QString m_draftDate;
    QString m_draftTag;
    QString m_draftNotes;
    QString m_draftColor = QString::fromLatin1(Database::DefaultColor);
    int m_draftRectangleCount = 0;

    int m_viewedRecordId = -1;
    QVariantMap m_viewedRecord;

    QString m_editDate;
    QString m_editTag;
    QString m_editNotes;
    QString m_editColor;

    int m_selectedRectangleIndex = -1;

    QString m_statusMessage;
    QString m_errorMessage;
    QString m_backupFolder;

    bool m_searching = false;
    QVariantList m_searchResults;
};
