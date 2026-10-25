// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "rectanglelistmodel.h"

RectangleListModel::RectangleListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RectangleListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rectangles.size();
}

QVariant RectangleListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rectangles.size())
        return {};

    const MapRectangle &rect = m_rectangles.at(index.row());
    switch (role) {
    case XRole:
        return rect.x;
    case YRole:
        return rect.y;
    case WRole:
        return rect.w;
    case HRole:
        return rect.h;
    case RotationRole:
        return rect.rotation;
    case ColorRole:
        return rect.color;
    default:
        return {};
    }
}

QHash<int, QByteArray> RectangleListModel::roleNames() const
{
    return {
        { XRole, "rx" },
        { YRole, "ry" },
        { WRole, "rw" },
        { HRole, "rh" },
        { RotationRole, "rRotation" },
        { ColorRole, "rColor" },
    };
}

int RectangleListModel::count() const
{
    return m_rectangles.size();
}

QVector<MapRectangle> RectangleListModel::rectangles() const
{
    return m_rectangles;
}

void RectangleListModel::setRectangles(const QVector<MapRectangle> &rectangles)
{
    beginResetModel();
    m_rectangles = rectangles;
    endResetModel();
    emit countChanged();
}

void RectangleListModel::addRectangle(const MapRectangle &rect)
{
    beginInsertRows(QModelIndex(), m_rectangles.size(), m_rectangles.size());
    m_rectangles.append(rect);
    endInsertRows();
    emit countChanged();
}

void RectangleListModel::setRectangle(int index, const MapRectangle &rect)
{
    if (index < 0 || index >= m_rectangles.size())
        return;
    m_rectangles[index] = rect;
    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, { XRole, YRole, WRole, HRole, RotationRole, ColorRole });
}

void RectangleListModel::removeRectangle(int index)
{
    if (index < 0 || index >= m_rectangles.size())
        return;
    beginRemoveRows(QModelIndex(), index, index);
    m_rectangles.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

void RectangleListModel::clear()
{
    setRectangles({});
}
