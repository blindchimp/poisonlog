import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PoisonLog 1.0

ApplicationWindow {
    id: window
    width: 1400
    height: 900
    minimumWidth: 1050
    minimumHeight: 720
    visible: true
    title: "Poison Log"
    color: "#f2f5f3"

    header: ToolBar {
        visible: App.hasMap

        background: Rectangle {
            color: "#eef3ef"
            Rectangle {
                width: parent.width
                height: 1
                anchors.bottom: parent.bottom
                color: "#d7e0d9"
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Label {
                text: "Poison Log"
                font.pixelSize: 20
                font.bold: true
                color: "#1b5e20"
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 24
                color: "#d7e0d9"
            }

            Label {
                Layout.maximumWidth: 460
                text: App.mapAddress
                elide: Text.ElideRight
                color: "#4a5d4f"
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                Layout.maximumWidth: 320
                text: App.errorMessage.length > 0 ? App.errorMessage : App.statusMessage
                color: App.errorMessage.length > 0 ? "#b71c1c" : "#647568"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Button {
                text: "Edit new/uncommitted"
                visible: App.mode === "record"
                onClicked: App.resumeDraft()
            }

            Button {
                text: "Clear draft"
                visible: App.draftHasContent || App.mode === "draft"
                enabled: App.draftHasContent
                onClicked: clearDraftDialog.open()
            }

            ToolButton {
                text: "Settings"
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: "Backup settings"
                onClicked: settingsDialog.open()
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: App.hasMap ? 0 : 1

        Item {
            id: mainPage

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 14

                RecordList {
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MapView {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, parent.height)
                        height: width
                    }
                }

                EditorPanel {
                    Layout.preferredWidth: 370
                    Layout.fillHeight: true
                }
            }
        }

        MapSetupPage {
            id: setupPage
        }
    }

    Shortcut {
        sequences: [StandardKey.Delete]
        enabled: App.mode === "draft" && App.hasSelectedRectangle
        onActivated: App.deleteSelectedRectangle()
    }

    Dialog {
        id: clearDraftDialog
        anchors.centerIn: parent
        modal: true
        title: "Discard the unfinished draft?"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: App.clearDraft()

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "All unfinished notes and rectangles will be deleted. Committed records will not be changed."
        }
    }

    Dialog {
        id: settingsDialog
        anchors.centerIn: parent
        modal: true
        title: "Backup settings"
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 520

        FolderDialog {
            id: backupFolderDialog
            title: "Choose the backup folder"
            onAccepted: {
                let path = selectedFolder.toString().replace(/^file:\/\//, "")
                path = path.replace(/^\/([A-Za-z]:)/, "$1")
                backupFolderField.text = decodeURIComponent(path)
            }
        }

        onAccepted: App.backupFolder = backupFolderField.text

        ColumnLayout {
            width: parent.width
            spacing: 8

            Label {
                text: "Backups are created every time the application is closed. " +
                      "The folder will contain the database and the map image, " +
                      "ready to be copied to another machine."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: "#53645a"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: backupFolderField
                    Layout.fillWidth: true
                    text: App.backupFolder
                    selectByMouse: true
                }

                Button {
                    text: "Browse…"
                    onClicked: backupFolderDialog.open()
                }
            }
        }
    }
}
