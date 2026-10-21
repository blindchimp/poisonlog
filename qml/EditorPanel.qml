import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PoisonLog 1.0

Frame {
    id: root
    padding: 0

    property var colorPalette: [
        "#2e7d32", "#f9a825", "#ef6c00", "#c62828",
        "#1565c0", "#6a1b9a", "#00838f", "#4e342e"
    ]

    background: Rectangle {
        color: "#ffffff"
        radius: 8
        border.color: "#dfe6e1"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: App.mode === "draft" ? "Application details"
                    : App.mode === "record-edit" ? "Edit saved application"
                    : "Saved application"
                font.pixelSize: 18
                font.bold: true
                color: "#24382a"
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                visible: App.mode === "draft"
                text: "Draft"
                color: "#6b7c70"
                font.pixelSize: 11
            }
            Label {
                visible: App.mode === "record-edit"
                text: "Unsaved edits"
                color: "#b71c1c"
                font.pixelSize: 11
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: App.mode === "draft" ? 0
                : App.mode === "record-edit" ? 2
                : 1

            ColumnLayout {
                id: draftPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Connections {
                    target: App
                    function onDraftDateChanged() {
                        if (!dateField.activeFocus)
                            dateField.text = App.draftDate
                    }
                    function onDraftTagChanged() {
                        if (!tagField.activeFocus)
                            tagField.text = App.draftTag
                    }
                    function onDraftNotesChanged() {
                        if (!notesArea.activeFocus)
                            notesArea.text = App.draftNotes
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Date"
                        Layout.preferredWidth: 48
                        color: "#44554a"
                    }
                    TextField {
                        id: dateField
                        Layout.fillWidth: true
                        text: App.draftDate
                        placeholderText: "YYYY-MM-DD"
                        selectByMouse: true
                        onTextChanged: {
                            if (activeFocus && text !== App.draftDate)
                                App.draftDate = text
                        }
                    }
                    Button {
                        text: "Today"
                        onClicked: {
                            App.draftDate = Qt.formatDate(new Date(), "yyyy-MM-dd")
                            dateField.text = App.draftDate
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Tag"
                        Layout.preferredWidth: 48
                        color: "#44554a"
                    }
                    TextField {
                        id: tagField
                        Layout.fillWidth: true
                        text: App.draftTag
                        placeholderText: "e.g. glyphosate, fertilizer"
                        selectByMouse: true
                        onTextChanged: {
                            if (activeFocus && text !== App.draftTag)
                                App.draftTag = text
                        }
                    }
                }

                Label {
                    text: "Color for new rectangles"
                    color: "#44554a"
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: root.colorPalette
                        delegate: Rectangle {
                            width: 28
                            height: 28
                            radius: 14
                            color: modelData
                            border.width: App.draftColor.toLowerCase() === modelData ? 3 : 1
                            border.color: App.draftColor.toLowerCase() === modelData
                                          ? "#263238" : "#ffffff"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: App.draftColor = modelData
                            }
                        }
                    }
                }

                Label {
                    Layout.topMargin: 4
                    text: "Notes — what was applied, how much, and any other details"
                    color: "#44554a"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: notesArea
                        width: parent.width
                        text: App.draftNotes
                        placeholderText: "Describe the application…"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        onTextChanged: {
                            if (activeFocus && text !== App.draftNotes)
                                App.draftNotes = text
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: App.rectangleCount + " rectangle(s)"
                        color: "#5e7064"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Delete selected"
                        enabled: App.hasSelectedRectangle
                        onClicked: App.deleteSelectedRectangle()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Drag on the map to draw a rectangle. Drag a rectangle to move it, "
                          + "or its lower-right corner to resize it."
                    color: "#849287"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                Button {
                    Layout.fillWidth: true
                    text: "Commit application"
                    enabled: App.canCommit
                    highlighted: true
                    onClicked: App.commitDraft()
                }

                Button {
                    Layout.fillWidth: true
                    text: "Reset draft"
                    onClicked: resetDraftDialog.open()
                }

                Label {
                    Layout.fillWidth: true
                    text: "Changes are saved automatically."
                    color: "#849287"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            ColumnLayout {
                id: recordPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        radius: 7
                        color: App.viewedRecord.color || "#2e7d32"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: App.viewedRecord.tag || ""
                        font.pixelSize: 17
                        font.bold: true
                        color: "#24382a"
                        elide: Text.ElideRight
                    }
                    Label {
                        text: App.viewedRecord.date || ""
                        color: "#5e7064"
                    }
                }

                Label {
                    text: "Notes"
                    color: "#44554a"
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        width: parent.width
                        text: App.viewedRecord.notes || ""
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: App.draftHasContent
                    text: "Your unfinished draft is still saved."
                    color: "#6b7c70"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                Button {
                    Layout.fillWidth: true
                    text: "Edit record"
                    highlighted: true
                    onClicked: App.startRecordEdit()
                }

                Button {
                    Layout.fillWidth: true
                    text: "Edit new/uncommitted"
                    onClicked: App.resumeDraft()
                }

                Button {
                    Layout.fillWidth: true
                    text: "Delete record"
                    onClicked: deleteRecordDialog.open()
                }
            }

            ColumnLayout {
                id: editPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Connections {
                    target: App
                    function onEditDateChanged() {
                        if (!editDateField.activeFocus)
                            editDateField.text = App.editDate
                    }
                    function onEditTagChanged() {
                        if (!editTagField.activeFocus)
                            editTagField.text = App.editTag
                    }
                    function onEditNotesChanged() {
                        if (!editNotesArea.activeFocus)
                            editNotesArea.text = App.editNotes
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Date"
                        Layout.preferredWidth: 48
                        color: "#44554a"
                    }
                    TextField {
                        id: editDateField
                        Layout.fillWidth: true
                        text: App.editDate
                        placeholderText: "YYYY-MM-DD"
                        selectByMouse: true
                        onTextChanged: {
                            if (activeFocus && text !== App.editDate)
                                App.editDate = text
                        }
                    }
                    Button {
                        text: "Today"
                        onClicked: {
                            App.editDate = Qt.formatDate(new Date(), "yyyy-MM-dd")
                            editDateField.text = App.editDate
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Tag"
                        Layout.preferredWidth: 48
                        color: "#44554a"
                    }
                    TextField {
                        id: editTagField
                        Layout.fillWidth: true
                        text: App.editTag
                        placeholderText: "e.g. glyphosate, fertilizer"
                        selectByMouse: true
                        onTextChanged: {
                            if (activeFocus && text !== App.editTag)
                                App.editTag = text
                        }
                    }
                }

                Label {
                    text: "Color for new rectangles"
                    color: "#44554a"
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: root.colorPalette
                        delegate: Rectangle {
                            width: 28
                            height: 28
                            radius: 14
                            color: modelData
                            border.width: App.editColor.toLowerCase() === modelData ? 3 : 1
                            border.color: App.editColor.toLowerCase() === modelData
                                          ? "#263238" : "#ffffff"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: App.editColor = modelData
                            }
                        }
                    }
                }

                Label {
                    Layout.topMargin: 4
                    text: "Notes — what was applied, how much, and any other details"
                    color: "#44554a"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: editNotesArea
                        width: parent.width
                        text: App.editNotes
                        placeholderText: "Describe the application…"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        onTextChanged: {
                            if (activeFocus && text !== App.editNotes)
                                App.editNotes = text
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: App.rectangleCount + " rectangle(s)"
                        color: "#5e7064"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Delete selected"
                        enabled: App.hasSelectedRectangle
                        onClicked: App.deleteSelectedRectangle()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Drag on the map to draw a rectangle. Drag a rectangle to move it, "
                          + "or its lower-right corner to resize it."
                    color: "#849287"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Button {
                        text: "Cancel"
                        onClicked: App.cancelRecordEdit()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Save changes"
                        enabled: App.canSaveEdit
                        highlighted: true
                        onClicked: App.saveRecordEdit()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Unsaved edits are forgotten if you cancel or save elsewhere."
                    color: "#849287"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: App.errorMessage.length > 0
            text: App.errorMessage
            color: "#b71c1c"
            wrapMode: Text.WordWrap
            font.pixelSize: 12
        }
    }

    Dialog {
        id: resetDraftDialog
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
        id: deleteRecordDialog
        anchors.centerIn: parent
        modal: true
        title: "Delete this record?"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: App.deleteViewedRecord()

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "The saved application and its rectangles on the map will be permanently deleted. This cannot be undone."
        }
    }
}
