#include "appstore.h"

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace {

constexpr double MinimumRectangleSize = 0.005;
constexpr double DefaultGroundSpanMeters = 70.0;
const char *DefaultAttribution = "USGS, USDA — The National Map: Orthoimagery";

} // namespace

AppStore::AppStore(QObject *parent, const QString &databasePath)
    : QObject(parent)
    , m_records(this)
    , m_rectangles(this)
    , m_geocoder(this)
{
    auto rectanglesChanged = [this]() {
        emit rectangleCountChanged();
        emit canCommitChanged();
        emit canSaveEditChanged();
    };
    connect(&m_rectangles, &QAbstractItemModel::rowsInserted, this, rectanglesChanged);
    connect(&m_rectangles, &QAbstractItemModel::rowsRemoved, this, rectanglesChanged);
    connect(&m_rectangles, &QAbstractItemModel::modelReset, this, rectanglesChanged);
    connect(&m_rectangles, &QAbstractItemModel::dataChanged, this, rectanglesChanged);

    connect(&m_geocoder, &Geocoder::finished, this,
            [this](const QVariantList &results, const QString &error) {
                m_searching = false;
                emit searchingChanged();
                if (!error.isEmpty()) {
                    m_searchResults.clear();
                    emit searchResultsChanged();
                    setError(error);
                    return;
                }
                m_searchResults = results;
                emit searchResultsChanged();
                setStatus(QStringLiteral("Found %1 address candidate(s).").arg(results.size()));
            });

    QString path = databasePath;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/poisonlog.db");
    }

    m_database = new Database(path);
    QString error;
    if (!m_database->open(&error)) {
        setError(error);
        setMode(QStringLiteral("setup"));
        return;
    }

    loadInitialState();
}

AppStore::~AppStore()
{
    delete m_database;
    m_database = nullptr;
}

bool AppStore::hasMap() const
{
    return m_hasMap;
}

QString AppStore::mapAddress() const
{
    return m_map.address;
}

QUrl AppStore::mapImageUrl() const
{
    if (!m_hasMap || m_map.imagePath.isEmpty())
        return {};
    return QUrl::fromLocalFile(m_map.imagePath);
}

int AppStore::mapImageWidth() const
{
    return m_map.imageWidth;
}

int AppStore::mapImageHeight() const
{
    return m_map.imageHeight;
}

QString AppStore::mapAttribution() const
{
    return m_map.attribution.isEmpty() ? QString::fromUtf8(DefaultAttribution) : m_map.attribution;
}

QString AppStore::mode() const
{
    return m_mode;
}

QString AppStore::draftDate() const
{
    return m_draftDate;
}

QString AppStore::draftTag() const
{
    return m_draftTag;
}

QString AppStore::draftNotes() const
{
    return m_draftNotes;
}

QString AppStore::draftColor() const
{
    return m_draftColor;
}

bool AppStore::draftHasContent() const
{
    return m_draftRectangleCount > 0 || !m_draftTag.trimmed().isEmpty()
        || !m_draftNotes.trimmed().isEmpty();
}

bool AppStore::canCommit() const
{
    return m_mode == QStringLiteral("draft") && m_draftRectangleCount > 0
        && !m_draftTag.trimmed().isEmpty()
        && QDate::fromString(m_draftDate, QStringLiteral("yyyy-MM-dd")).isValid();
}

int AppStore::viewedRecordId() const
{
    return m_viewedRecordId;
}

QVariantMap AppStore::viewedRecord() const
{
    return m_viewedRecord;
}

QString AppStore::editDate() const
{
    return m_editDate;
}

QString AppStore::editTag() const
{
    return m_editTag;
}

QString AppStore::editNotes() const
{
    return m_editNotes;
}

QString AppStore::editColor() const
{
    return m_editColor;
}

bool AppStore::editingRecord() const
{
    return m_mode == QStringLiteral("record-edit");
}

bool AppStore::canSaveEdit() const
{
    return editingRecord() && m_rectangles.count() > 0
        && !m_editTag.trimmed().isEmpty()
        && QDate::fromString(m_editDate, QStringLiteral("yyyy-MM-dd")).isValid();
}

void AppStore::setEditDate(const QString &date)
{
    if (m_editDate == date)
        return;
    m_editDate = date;
    emit editDateChanged();
    emit canSaveEditChanged();
}

void AppStore::setEditTag(const QString &tag)
{
    if (m_editTag == tag)
        return;
    m_editTag = tag;
    emit editTagChanged();
    emit canSaveEditChanged();
}

void AppStore::setEditNotes(const QString &notes)
{
    if (m_editNotes == notes)
        return;
    m_editNotes = notes;
    emit editNotesChanged();
}

void AppStore::setEditColor(const QString &color)
{
    const QColor parsed(color);
    if (!parsed.isValid())
        return;
    const QString normalized = parsed.name();
    if (m_editColor == normalized)
        return;
    m_editColor = normalized;
    emit editColorChanged();
    emit currentRectangleColorChanged();
}

QObject *AppStore::recordsModel()
{
    return &m_records;
}

QObject *AppStore::rectanglesModel()
{
    return &m_rectangles;
}

int AppStore::recordCount() const
{
    return m_records.count();
}

int AppStore::rectangleCount() const
{
    return m_rectangles.count();
}

int AppStore::selectedRectangleIndex() const
{
    return m_selectedRectangleIndex;
}

bool AppStore::hasSelectedRectangle() const
{
    return m_selectedRectangleIndex >= 0
        && m_selectedRectangleIndex < m_rectangles.count();
}

QString AppStore::currentRectangleColor() const
{
    if (m_mode == QStringLiteral("record-edit"))
        return m_editColor;
    if (m_mode == QStringLiteral("record")) {
        const QString color = m_viewedRecord.value(QStringLiteral("color")).toString();
        if (!color.isEmpty())
            return color;
    }
    return m_draftColor;
}

QString AppStore::statusMessage() const
{
    return m_statusMessage;
}

QString AppStore::errorMessage() const
{
    return m_errorMessage;
}

bool AppStore::searching() const
{
    return m_searching;
}

QVariantList AppStore::searchResults() const
{
    return m_searchResults;
}

void AppStore::setDraftDate(const QString &date)
{
    if (m_draftDate == date)
        return;
    m_draftDate = date;
    persistDraftFields();
    emit draftDateChanged();
    emit draftContentChanged();
    emit canCommitChanged();
}

void AppStore::setDraftTag(const QString &tag)
{
    if (m_draftTag == tag)
        return;
    m_draftTag = tag;
    persistDraftFields();
    emit draftTagChanged();
    emit draftContentChanged();
    emit canCommitChanged();
}

void AppStore::setDraftNotes(const QString &notes)
{
    if (m_draftNotes == notes)
        return;
    m_draftNotes = notes;
    persistDraftFields();
    emit draftNotesChanged();
    emit draftContentChanged();
    emit canCommitChanged();
}

void AppStore::setDraftColor(const QString &color)
{
    const QColor parsed(color);
    if (!parsed.isValid())
        return;
    const QString normalized = parsed.name();
    if (m_draftColor == normalized)
        return;
    m_draftColor = normalized;
    persistDraftFields();
    emit draftColorChanged();
    emit draftContentChanged();
    emit currentRectangleColorChanged();
}

QString AppStore::newMapImagePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/maps");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/property.png");
    QFile::remove(path);
    return path;
}

void AppStore::finishMapSetup(const QString &address, double latitude, double longitude,
                              double groundSpanMeters, const QString &imagePath,
                              int imageWidth, int imageHeight)
{
    if (m_hasMap) {
        setError(QStringLiteral("A property map has already been saved."));
        return;
    }
    if (address.trimmed().isEmpty()) {
        setError(QStringLiteral("Choose an address before saving the map."));
        return;
    }
    if (!QFileInfo::exists(imagePath) || QFileInfo(imagePath).size() == 0) {
        setError(QStringLiteral("The map image could not be saved."));
        return;
    }
    if (latitude < -85.0 || latitude > 85.0 || longitude < -180.0 || longitude > 180.0) {
        setError(QStringLiteral("The map center is not valid."));
        return;
    }

    Database::MapInfo info;
    info.address = address.trimmed();
    info.centerLat = latitude;
    info.centerLon = longitude;
    info.groundSpanMeters = groundSpanMeters > 0.0 ? groundSpanMeters : DefaultGroundSpanMeters;
    info.imagePath = imagePath;
    info.imageWidth = imageWidth;
    info.imageHeight = imageHeight;
    info.attribution = QString::fromUtf8(DefaultAttribution);
    info.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QString error;
    if (!m_database->saveMap(info, &error)) {
        setError(error);
        return;
    }
    if (!loadMap()) {
        setError(QStringLiteral("The saved map could not be loaded."));
        return;
    }
    if (!m_database->ensureDraft(&error)) {
        setError(error);
        return;
    }

    refreshRecords();
    if (!loadDraft(true)) {
        setError(QStringLiteral("The draft could not be loaded."));
        return;
    }

    setMode(QStringLiteral("draft"));
    m_searchResults.clear();
    emit searchResultsChanged();
    emit mapChanged();
    emit currentRectangleColorChanged();
    setError(QString());
    setStatus(QStringLiteral("Property map saved."));
}

void AppStore::searchAddress(const QString &query)
{
    if (query.trimmed().isEmpty()) {
        setError(QStringLiteral("Enter an address to search."));
        return;
    }

    setError(QString());
    m_searchResults.clear();
    emit searchResultsChanged();
    m_searching = true;
    emit searchingChanged();
    m_geocoder.search(query);
}

void AppStore::selectRecord(int recordId)
{
    if (!m_hasMap)
        return;
    if (editingRecord()) {
        setError(QStringLiteral("Save or cancel your edits before selecting another record."));
        return;
    }

    if (m_mode == QStringLiteral("draft")) {
        persistDraftFields();
        persistDraftRectangles();
    }

    QString error;
    const auto record = m_database->record(recordId, &error);
    if (!record) {
        setError(error.isEmpty() ? QStringLiteral("The record could not be found.") : error);
        return;
    }

    m_viewedRecordId = record->id;
    m_viewedRecord = recordToMap(*record);
    loadRecordRectangles(record->id);
    setMode(QStringLiteral("record"));
    emit viewedRecordChanged();
    emit currentRectangleColorChanged();
    setError(QString());
    setStatus(QStringLiteral("Viewing saved application."));
}

void AppStore::resumeDraft()
{
    if (!m_hasMap)
        return;
    if (editingRecord()) {
        setError(QStringLiteral("Save or cancel your edits before editing the new/uncommitted application."));
        return;
    }
    if (!loadDraft(true)) {
        setError(QStringLiteral("The draft could not be loaded."));
        return;
    }
    resetViewedRecord();
    setMode(QStringLiteral("draft"));
    setError(QString());
    setStatus(QStringLiteral("Editing the new/uncommitted application."));
}

void AppStore::startRecordEdit()
{
    if (!m_hasMap)
        return;
    if (m_viewedRecordId < 0 || m_viewedRecord.isEmpty()) {
        setError(QStringLiteral("Select a record before editing it."));
        return;
    }

    QString error;
    const auto record = m_database->record(m_viewedRecordId, &error);
    if (!record) {
        setError(error.isEmpty() ? QStringLiteral("The record could not be found.") : error);
        return;
    }

    m_editDate = record->date;
    m_editTag = record->tag;
    m_editNotes = record->notes;
    m_editColor = record->color.isEmpty() ? QString::fromLatin1(Database::DefaultColor)
                                          : record->color;
    loadRecordRectangles(record->id);

    emit editDateChanged();
    emit editTagChanged();
    emit editNotesChanged();
    emit editColorChanged();
    emit canSaveEditChanged();

    setMode(QStringLiteral("record-edit"));
    setError(QString());
    setStatus(QStringLiteral("Editing the saved application."));
}

void AppStore::cancelRecordEdit()
{
    if (!editingRecord())
        return;

    const int recordId = m_viewedRecordId;
    QString error;
    const auto record = m_database->record(recordId, &error);
    if (!record) {
        setError(error.isEmpty() ? QStringLiteral("The record could not be reloaded.") : error);
        return;
    }

    m_viewedRecord = recordToMap(*record);
    loadRecordRectangles(recordId);
    emit viewedRecordChanged();

    setMode(QStringLiteral("record"));
    emit currentRectangleColorChanged();
    setError(QString());
    setStatus(QStringLiteral("Edits to the record were cancelled."));
}

void AppStore::saveRecordEdit()
{
    if (!editingRecord())
        return;
    if (!canSaveEdit()) {
        setError(QStringLiteral("Add at least one rectangle, a tag, and a valid date."));
        return;
    }

    const int recordId = m_viewedRecordId;

    Database::Record fields;
    fields.date = m_editDate;
    fields.tag = m_editTag.trimmed();
    fields.color = m_editColor;
    fields.notes = m_editNotes;

    const QVector<MapRectangle> rectangles = m_rectangles.rectangles();
    QVector<Database::Rect> stored;
    stored.reserve(rectangles.size());
    for (const MapRectangle &item : rectangles) {
        Database::Rect rect;
        rect.x = item.x;
        rect.y = item.y;
        rect.w = item.w;
        rect.h = item.h;
        stored.append(rect);
    }

    QString error;
    if (!m_database->updateRecord(recordId, fields, stored, &error)) {
        setError(error);
        return;
    }

    refreshRecords();

    const auto record = m_database->record(recordId, &error);
    if (!record) {
        setError(error.isEmpty() ? QStringLiteral("The record could not be reloaded.") : error);
        return;
    }

    m_viewedRecord = recordToMap(*record);
    loadRecordRectangles(recordId);
    emit viewedRecordChanged();

    setMode(QStringLiteral("record"));
    emit currentRectangleColorChanged();
    setError(QString());
    setStatus(QStringLiteral("The saved application was updated."));
}

void AppStore::clearDraft()
{
    if (m_draftId <= 0)
        return;

    Database::Record defaults;
    defaults.date = today();
    QString error;
    if (!m_database->clearDraft(m_draftId, defaults, &error)) {
        setError(error);
        return;
    }

    if (!loadDraft(true)) {
        setError(QStringLiteral("The draft could not be reset."));
        return;
    }
    resetViewedRecord();
    setMode(QStringLiteral("draft"));
    setError(QString());
    setStatus(QStringLiteral("The unfinished draft was discarded."));
}

void AppStore::commitDraft()
{
    if (m_mode != QStringLiteral("draft")) {
        setError(QStringLiteral("Edit the new/uncommitted application before committing."));
        return;
    }
    if (!canCommit()) {
        setError(QStringLiteral("Add at least one rectangle, a tag, and a valid date."));
        return;
    }

    persistDraftFields();
    persistDraftRectangles();

    Database::Record fields;
    fields.date = m_draftDate;
    fields.tag = m_draftTag.trimmed();
    fields.color = m_draftColor;
    fields.notes = m_draftNotes;

    QString error;
    int committedId = 0;
    if (!m_database->commitDraft(m_draftId, fields, &committedId, &error)) {
        setError(error);
        return;
    }

    if (!loadDraft(true))
        return;
    refreshRecords();

    const auto committed = m_database->record(committedId, &error);
    if (!committed) {
        setError(error.isEmpty() ? QStringLiteral("The saved record could not be reloaded.") : error);
        return;
    }

    m_viewedRecordId = committed->id;
    m_viewedRecord = recordToMap(*committed);
    loadRecordRectangles(committed->id);
    setMode(QStringLiteral("record"));
    emit viewedRecordChanged();
    emit currentRectangleColorChanged();
    setError(QString());
    setStatus(QStringLiteral("Application committed."));
}

void AppStore::addDraftRectangle(double x, double y, double width, double height)
{
    if (m_mode != QStringLiteral("draft") && !editingRecord())
        return;

    x = clamp01(x);
    y = clamp01(y);
    width = std::min(clamp01(width), 1.0 - x);
    height = std::min(clamp01(height), 1.0 - y);
    if (width < MinimumRectangleSize || height < MinimumRectangleSize)
        return;

    MapRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    m_rectangles.addRectangle(rect);
    if (m_mode == QStringLiteral("draft")) {
        ++m_draftRectangleCount;
        emit canCommitChanged();
    }
    m_selectedRectangleIndex = m_rectangles.count() - 1;
    emit selectedRectangleChanged();
    if (m_mode == QStringLiteral("draft")) {
        persistDraftRectangles();
        emit draftContentChanged();
    }
}

void AppStore::updateDraftRectangle(int index, double x, double y, double width,
                                    double height, bool persist)
{
    if (m_mode != QStringLiteral("draft") && !editingRecord())
        return;
    if (index < 0 || index >= m_rectangles.count())
        return;

    x = clamp01(x);
    y = clamp01(y);
    width = std::max(MinimumRectangleSize, std::min(clamp01(width), 1.0 - x));
    height = std::max(MinimumRectangleSize, std::min(clamp01(height), 1.0 - y));

    MapRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    m_rectangles.setRectangle(index, rect);
    if (persist && m_mode == QStringLiteral("draft"))
        persistDraftRectangles();
}

void AppStore::saveDraftRectangles()
{
    if (!m_rectangles.count())
        return;
    // In record-edit mode edits stay in memory until saveRecordEdit().
    if (editingRecord())
        return;
    if (m_mode != QStringLiteral("draft"))
        return;
    m_draftRectangleCount = m_rectangles.count();
    persistDraftRectangles();
    emit draftContentChanged();
    emit canCommitChanged();
}

void AppStore::removeDraftRectangle(int index)
{
    if (m_mode != QStringLiteral("draft") && !editingRecord())
        return;
    if (index < 0 || index >= m_rectangles.count())
        return;

    m_rectangles.removeRectangle(index);
    if (m_mode == QStringLiteral("draft")) {
        m_draftRectangleCount = m_rectangles.count();
        emit canCommitChanged();
    }
    if (m_selectedRectangleIndex == index) {
        m_selectedRectangleIndex = -1;
    } else if (m_selectedRectangleIndex > index) {
        --m_selectedRectangleIndex;
    }
    emit selectedRectangleChanged();
    if (m_mode == QStringLiteral("draft")) {
        persistDraftRectangles();
        emit draftContentChanged();
    }
}

void AppStore::deleteSelectedRectangle()
{
    if (hasSelectedRectangle())
        removeDraftRectangle(m_selectedRectangleIndex);
}

void AppStore::selectRectangle(int index)
{
    if (index < 0 || index >= m_rectangles.count())
        index = -1;
    if (index == m_selectedRectangleIndex)
        return;
    m_selectedRectangleIndex = index;
    emit selectedRectangleChanged();
}

void AppStore::reportError(const QString &message)
{
    setError(message);
}

void AppStore::clearMessages()
{
    setError(QString());
    setStatus(QString());
}

void AppStore::loadInitialState()
{
    if (!loadMap()) {
        if (m_errorMessage.isEmpty())
            setStatus(QStringLiteral("Search for an address to create the property map."));
        setMode(QStringLiteral("setup"));
        return;
    }

    QString error;
    if (!m_database->ensureDraft(&error)) {
        setError(error);
        setMode(QStringLiteral("setup"));
        return;
    }

    refreshRecords();
    if (!loadDraft(true)) {
        setError(QStringLiteral("The draft could not be loaded."));
        setMode(QStringLiteral("setup"));
        return;
    }

    setMode(QStringLiteral("draft"));
    setStatus(QStringLiteral("Restored the unfinished draft."));
}

bool AppStore::loadMap()
{
    QString error;
    const auto info = m_database->map(&error);
    if (!info) {
        if (!error.isEmpty())
            setError(error);
        m_hasMap = false;
        m_map = {};
        return false;
    }

    m_map = *info;
    m_hasMap = true;
    return true;
}

bool AppStore::loadDraft(bool loadRectangles)
{
    QString error;
    if (!m_database->ensureDraft(&error)) {
        setError(error);
        return false;
    }
    const auto record = m_database->draft(&error);
    if (!record) {
        setError(error.isEmpty() ? QStringLiteral("The draft could not be loaded.") : error);
        return false;
    }

    m_draftId = record->id;
    m_draftDate = record->date;
    m_draftTag = record->tag;
    m_draftNotes = record->notes;
    m_draftColor = record->color.isEmpty() ? QString::fromLatin1(Database::DefaultColor)
                                           : record->color;
    emit draftDateChanged();
    emit draftTagChanged();
    emit draftNotesChanged();
    emit draftColorChanged();
    emit draftContentChanged();
    emit canCommitChanged();
    emit currentRectangleColorChanged();

    if (loadRectangles)
        loadDraftRectangles();
    return true;
}

void AppStore::loadDraftRectangles()
{
    if (m_draftId <= 0)
        return;

    QString error;
    const QVector<Database::Rect> stored = m_database->rectangles(m_draftId, &error);
    if (!error.isEmpty()) {
        setError(error);
        return;
    }

    QVector<MapRectangle> rectangles;
    rectangles.reserve(stored.size());
    for (const Database::Rect &rect : stored) {
        MapRectangle item;
        item.x = rect.x;
        item.y = rect.y;
        item.w = rect.w;
        item.h = rect.h;
        rectangles.append(item);
    }
    m_rectangles.setRectangles(rectangles);
    m_draftRectangleCount = rectangles.size();
    m_selectedRectangleIndex = -1;
    emit selectedRectangleChanged();
    emit rectangleCountChanged();
    emit draftContentChanged();
    emit canCommitChanged();
}

void AppStore::loadRecordRectangles(int recordId)
{
    QString error;
    const QVector<Database::Rect> stored = m_database->rectangles(recordId, &error);
    if (!error.isEmpty()) {
        setError(error);
        return;
    }

    QVector<MapRectangle> rectangles;
    rectangles.reserve(stored.size());
    for (const Database::Rect &rect : stored) {
        MapRectangle item;
        item.x = rect.x;
        item.y = rect.y;
        item.w = rect.w;
        item.h = rect.h;
        rectangles.append(item);
    }
    m_rectangles.setRectangles(rectangles);
    m_selectedRectangleIndex = -1;
    emit selectedRectangleChanged();
    emit rectangleCountChanged();
    emit canCommitChanged();
}

void AppStore::refreshRecords()
{
    QString error;
    const QVector<Database::Record> records = m_database->committedRecords(&error);
    if (!error.isEmpty()) {
        setError(error);
        return;
    }
    m_records.setRecords(records);
    emit recordCountChanged();
}

void AppStore::setMode(const QString &mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    emit modeChanged();
    emit canCommitChanged();
    emit canSaveEditChanged();
    emit currentRectangleColorChanged();
}

void AppStore::persistDraftFields()
{
    if (!m_database || !m_database->isOpen() || m_draftId <= 0)
        return;

    Database::Record fields;
    fields.date = m_draftDate;
    fields.tag = m_draftTag;
    fields.color = m_draftColor;
    fields.notes = m_draftNotes;

    QString error;
    if (!m_database->saveDraftFields(m_draftId, fields, &error))
        setError(error);
}

void AppStore::persistDraftRectangles()
{
    if (!m_database || !m_database->isOpen() || m_draftId <= 0)
        return;

    const QVector<MapRectangle> rectangles = m_rectangles.rectangles();
    QVector<Database::Rect> stored;
    stored.reserve(rectangles.size());
    for (const MapRectangle &item : rectangles) {
        Database::Rect rect;
        rect.x = item.x;
        rect.y = item.y;
        rect.w = item.w;
        rect.h = item.h;
        stored.append(rect);
    }

    QString error;
    if (!m_database->replaceRectangles(m_draftId, stored, &error))
        setError(error);
}

void AppStore::setStatus(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void AppStore::setError(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void AppStore::resetViewedRecord()
{
    const bool hadRecord = m_viewedRecordId >= 0 || !m_viewedRecord.isEmpty();
    m_viewedRecordId = -1;
    m_viewedRecord.clear();
    if (hadRecord) {
        emit viewedRecordChanged();
        emit currentRectangleColorChanged();
    }
}

QVariantMap AppStore::recordToMap(const Database::Record &record) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), record.id);
    map.insert(QStringLiteral("date"), record.date);
    map.insert(QStringLiteral("tag"), record.tag);
    map.insert(QStringLiteral("color"), record.color);
    map.insert(QStringLiteral("notes"), record.notes);
    map.insert(QStringLiteral("createdAt"), record.createdAt);
    return map;
}

QString AppStore::today()
{
    return QDate::currentDate().toString(Qt::ISODate);
}

double AppStore::clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}
