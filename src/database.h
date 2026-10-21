#pragma once

#include <QString>
#include <QVector>

#include <optional>

class QSqlDatabase;

class Database
{
public:
    struct MapInfo {
        int id = 1;
        QString address;
        double centerLat = 0.0;
        double centerLon = 0.0;
        double groundSpanMeters = 70.0;
        QString imagePath;
        int imageWidth = 768;
        int imageHeight = 768;
        QString attribution;
        QString createdAt;
    };

    struct Record {
        int id = 0;
        int mapId = 1;
        QString status;
        QString date;
        QString tag;
        QString color;
        QString notes;
        QString createdAt;
        QString updatedAt;
    };

    struct Rect {
        double x = 0.0;
        double y = 0.0;
        double w = 0.0;
        double h = 0.0;
        int sort = 0;
    };

    static constexpr const char *DefaultColor = "#2e7d32";

    explicit Database(const QString &path);
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(QString *error);
    bool isOpen() const;

    std::optional<MapInfo> map(QString *error = nullptr) const;
    bool saveMap(const MapInfo &info, QString *error);

    std::optional<Record> draft(QString *error = nullptr) const;
    bool ensureDraft(QString *error);
    bool saveDraftFields(int draftId, const Record &fields, QString *error);

    QVector<Record> committedRecords(QString *error = nullptr) const;
    std::optional<Record> record(int id, QString *error = nullptr) const;
    QVector<Rect> rectangles(int recordId, QString *error = nullptr) const;
    bool replaceRectangles(int recordId, const QVector<Rect> &rectangles, QString *error);

    bool commitDraft(int draftId, const Record &fields, int *committedId, QString *error);
    bool clearDraft(int draftId, const Record &defaults, QString *error);
    bool updateRecord(int recordId, const Record &fields, const QVector<Rect> &rectangles,
                      QString *error);

private:
    bool ensureSchema(QString *error);
    bool exec(const QString &sql, QString *error) const;

    QString m_path;
    QString m_connectionName;
    QSqlDatabase *m_db = nullptr;
};
