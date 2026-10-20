#include "recordlistmodel.h"

RecordListModel::RecordListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RecordListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant RecordListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};

    const Database::Record &record = m_records.at(index.row());
    switch (role) {
    case RecordIdRole:
        return record.id;
    case DateRole:
        return record.date;
    case TagRole:
        return record.tag;
    case NotesRole:
        return record.notes;
    case ColorRole:
        return record.color;
    case CreatedAtRole:
        return record.createdAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> RecordListModel::roleNames() const
{
    return {
        { RecordIdRole, "recordId" },
        { DateRole, "date" },
        { TagRole, "tag" },
        { NotesRole, "notes" },
        { ColorRole, "color" },
        { CreatedAtRole, "createdAt" },
    };
}

int RecordListModel::count() const
{
    return m_records.size();
}

void RecordListModel::setRecords(const QVector<Database::Record> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
    emit countChanged();
}

void RecordListModel::clear()
{
    setRecords({});
}
