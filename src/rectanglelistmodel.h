#pragma once

#include <QAbstractListModel>
#include <QVector>

struct MapRectangle {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double rotation = 0.0;
    QString color;
};

class RectangleListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        XRole = Qt::UserRole + 1,
        YRole,
        WRole,
        HRole,
        RotationRole,
        ColorRole,
    };

    explicit RectangleListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QVector<MapRectangle> rectangles() const;
    void setRectangles(const QVector<MapRectangle> &rectangles);
    void addRectangle(const MapRectangle &rect);
    void setRectangle(int index, const MapRectangle &rect);
    void removeRectangle(int index);
    void clear();

signals:
    void countChanged();

private:
    QVector<MapRectangle> m_rectangles;
};
