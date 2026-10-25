import QtQuick
import PoisonLog 1.0

Item {
    id: root
    objectName: "mapView"
    clip: true

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function normalizeDelta(value) {
        return value - 360 * Math.round(value / 360)
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: "#26342b"
    }

    Image {
        id: mapImage
        anchors.fill: parent
        source: App.mapImageUrl
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: true
    }

    Text {
        anchors.centerIn: parent
        visible: mapImage.status === Image.Error
        text: "The saved map image could not be loaded."
        color: "#ffffff"
    }

    MouseArea {
        id: createArea
        anchors.fill: parent
        enabled: App.mode === "draft" || App.mode === "record-edit"
        acceptedButtons: Qt.LeftButton
        z: 0

        property bool drawing: false
        property real startX: 0
        property real startY: 0

        function finish(x, y) {
            const nx = clamp(Math.min(startX, x), 0, 1)
            const ny = clamp(Math.min(startY, y), 0, 1)
            const nw = clamp(Math.abs(x - startX), 0, 1 - nx)
            const nh = clamp(Math.abs(y - startY), 0, 1 - ny)
            if (nw >= 0.01 && nh >= 0.01)
                App.addDraftRectangle(nx, ny, nw, nh)
        }

        onPressed: (mouse) => {
            App.selectRectangle(-1)
            startX = clamp(mouse.x / width, 0, 1)
            startY = clamp(mouse.y / height, 0, 1)
            rubber.x = startX * width
            rubber.y = startY * height
            rubber.width = 0
            rubber.height = 0
            rubber.visible = true
            drawing = true
        }

        onPositionChanged: (mouse) => {
            if (!drawing)
                return
            const x = clamp(mouse.x / width, 0, 1)
            const y = clamp(mouse.y / height, 0, 1)
            rubber.x = Math.min(startX, x) * width
            rubber.y = Math.min(startY, y) * height
            rubber.width = Math.abs(x - startX) * width
            rubber.height = Math.abs(y - startY) * height
        }

        onReleased: (mouse) => {
            if (!drawing)
                return
            drawing = false
            rubber.visible = false
            finish(clamp(mouse.x / width, 0, 1), clamp(mouse.y / height, 0, 1))
        }

        onCanceled: {
            drawing = false
            rubber.visible = false
        }
    }

    Repeater {
        id: rectangleRepeater
        model: App.rectanglesModel

        delegate: Rectangle {
            id: rectItem
            required property int index
            required property real rx
            required property real ry
            required property real rw
            required property real rh
            required property real rRotation
            required property string rColor

            x: rx * root.width
            y: ry * root.height
            width: rw * root.width
            height: rh * root.height
            z: 1
            color: "transparent"
            border.width: App.selectedRectangleIndex === index ? 3 : 2
            border.color: rColor
            radius: 1
            transform: Rotation {
                origin.x: rectItem.width / 2
                origin.y: rectItem.height / 2
                angle: rectItem.rRotation
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                color: rectItem.rColor
                opacity: 0.22
            }

            MouseArea {
                id: moveArea
                anchors.fill: parent
                enabled: App.mode === "draft" || App.mode === "record-edit"
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeAllCursor

                property real pressX: 0
                property real pressY: 0
                property real origX: 0
                property real origY: 0

                onPressed: (mouse) => {
                    App.selectRectangle(index)
                    const point = mapToItem(root, mouse.x, mouse.y)
                    pressX = point.x
                    pressY = point.y
                    origX = rectItem.rx
                    origY = rectItem.ry
                }

                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const point = mapToItem(root, mouse.x, mouse.y)
                    const nx = root.clamp(origX + (point.x - pressX) / root.width,
                                          0, 1 - rectItem.rw)
                    const ny = root.clamp(origY + (point.y - pressY) / root.height,
                                          0, 1 - rectItem.rh)
                    App.updateDraftRectangle(index, nx, ny, rectItem.rw, rectItem.rh, false)
                }

                onReleased: App.saveDraftRectangles()
            }

            Rectangle {
                width: 14
                height: 14
                radius: 3
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: App.mode === "draft" || App.mode === "record-edit"
                color: rectItem.rColor
                border.color: "#ffffff"
                border.width: 2

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -5
                    enabled: App.mode === "draft" || App.mode === "record-edit"
                    cursorShape: Qt.SizeFDiagCursor

                    property real pressX: 0
                    property real pressY: 0
                    property real origW: 0
                    property real origH: 0

                    onPressed: (mouse) => {
                        App.selectRectangle(index)
                        const point = mapToItem(root, mouse.x, mouse.y)
                        pressX = point.x
                        pressY = point.y
                        origW = rectItem.rw
                        origH = rectItem.rh
                    }

                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        const point = mapToItem(root, mouse.x, mouse.y)
                        const nw = root.clamp(origW + (point.x - pressX) / root.width,
                                              0.01, 1 - rectItem.rx)
                        const nh = root.clamp(origH + (point.y - pressY) / root.height,
                                              0.01, 1 - rectItem.ry)
                        App.updateDraftRectangle(index, rectItem.rx, rectItem.ry, nw, nh, false)
                    }

                    onReleased: App.saveDraftRectangles()
                }
            }

            Rectangle {
                id: rotationHandle
                width: 14
                height: 14
                radius: 7
                x: -24
                y: parent.height + 10
                visible: App.mode === "draft" || App.mode === "record-edit"
                color: rectItem.rColor
                border.color: "#ffffff"
                border.width: 2

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -5
                    enabled: App.mode === "draft" || App.mode === "record-edit"
                    cursorShape: Qt.CrossCursor

                    property real centerX: 0
                    property real centerY: 0
                    property real prevAngle: 0
                    property real currentRotation: 0

                    onPressed: (mouse) => {
                        App.selectRectangle(index)
                        const point = mapToItem(root, mouse.x, mouse.y)
                        centerX = rectItem.rx * root.width + rectItem.width / 2
                        centerY = rectItem.ry * root.height + rectItem.height / 2
                        prevAngle = Math.atan2(point.y - centerY, point.x - centerX)
                        currentRotation = rectItem.rRotation
                    }

                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        const point = mapToItem(root, mouse.x, mouse.y)
                        const angle = Math.atan2(point.y - centerY, point.x - centerX)
                        const delta = root.normalizeDelta((angle - prevAngle) * 180 / Math.PI)
                        prevAngle = angle
                        currentRotation += delta
                        App.updateDraftRectangleRotation(index, currentRotation, false)
                    }

                    onReleased: App.saveDraftRectangles()
                }
            }
        }
    }

    Rectangle {
        id: rubber
        visible: false
        z: 2
        color: "transparent"
        border.color: App.currentRectangleColor
        border.width: 2

        Rectangle {
            anchors.fill: parent
            color: App.currentRectangleColor
            opacity: 0.2
        }
    }

    Rectangle {
        z: 10
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        width: attribution.implicitWidth + 10
        height: attribution.implicitHeight + 4
        radius: 2
        color: "#e6ffffff"

        Text {
            id: attribution
            anchors.centerIn: parent
            text: App.mapAttribution
            font.pixelSize: 10
            color: "#304437"
        }
    }
}
