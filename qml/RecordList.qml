import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PoisonLog 1.0

Frame {
    id: root
    padding: 0

    background: Rectangle {
        color: "#ffffff"
        radius: 8
        border.color: "#dfe6e1"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Records"
                font.pixelSize: 18
                font.bold: true
                color: "#24382a"
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: App.recordCount
                color: "#6a7b6e"
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Newest first"
            font.pixelSize: 11
            color: "#87968b"
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: App.recordsModel

            delegate: ItemDelegate {
                id: delegate
                width: list.width
                height: 78
                highlighted: App.mode === "record" && App.viewedRecordId === model.recordId
                onClicked: App.selectRecord(model.recordId)

                background: Rectangle {
                    radius: 6
                    color: delegate.highlighted ? "#e3f1e6"
                         : delegate.hovered ? "#f4f7f5" : "transparent"
                }

                contentItem: Item {
                    Rectangle {
                        id: swatch
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        width: 6
                        radius: 3
                        color: model.color
                    }

                    Column {
                        anchors.left: swatch.right
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3

                        Text {
                            width: parent.width
                            text: (model.date || "") + "   " + (model.tag || "")
                            font.pixelSize: 13
                            font.bold: true
                            color: "#2c3e30"
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: model.notes && model.notes.length > 0
                                  ? model.notes.replace(/\n/g, " ")
                                  : "No notes"
                            font.pixelSize: 12
                            color: "#6c7c70"
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 30
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "Saved treatments will appear here."
                color: "#94a197"
            }
        }
    }
}
