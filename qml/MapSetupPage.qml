// Copyright (c) 2026-present, Dwyco, Inc.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PoisonLog 1.0

Item {
    id: page
    objectName: "mapSetupPage"

    property bool hasCenter: false
    property string chosenAddress: ""
    property bool saving: false

    function chooseCandidate(candidate) {
        canvas.centerLat = candidate.lat
        canvas.centerLon = candidate.lon
        page.chosenAddress = candidate.displayName
        page.hasCenter = true
    }

    function runSearch() {
        if (addressField.text.trim().length === 0)
            return
        App.searchAddress(addressField.text)
    }

    function saveMap() {
        if (!page.hasCenter || !canvas.tilesReady || page.saving)
            return

        page.saving = true
        App.clearMessages()
        canvas.grabToImage(function(result) {
            if (!result) {
                page.saving = false
                App.reportError("The map image could not be captured.")
                return
            }
            const path = App.newMapImagePath()
            if (!result.saveToFile(path)) {
                page.saving = false
                App.reportError("The map image could not be saved.")
                return
            }
            App.finishMapSetup(page.chosenAddress, canvas.centerLat, canvas.centerLon,
                               canvas.effectiveGroundSpanMeters, path, 768, 768)
            page.saving = false
        }, Qt.size(768, 768))
    }

    Connections {
        target: App
        function onSearchResultsChanged() {
            if (App.searchResults.length > 0)
                page.chooseCandidate(App.searchResults[0])
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 20
        spacing: 10

        Label {
            text: "Set up the property map"
            font.pixelSize: 26
            font.bold: true
            color: "#1b5e20"
        }

        Label {
            Layout.fillWidth: true
            text: "Search for the property address, then drag the USGS aerial image so the main structure sits under the crosshair. "
                  + "The default square is about one acre; use +/− to zoom, then save the image."
            wrapMode: Text.WordWrap
            color: "#4b5f50"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: addressField
                Layout.fillWidth: true
                placeholderText: "Street address, city, state, postal code"
                selectByMouse: true
                onAccepted: page.runSearch()
            }

            Button {
                text: App.searching ? "Searching…" : "Search"
                enabled: !App.searching && addressField.text.trim().length > 0
                onClicked: page.runSearch()
            }

            BusyIndicator {
                running: App.searching
                visible: App.searching
                implicitWidth: 32
                implicitHeight: 32
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(138, resultsList.contentHeight + 12)
            visible: App.searchResults.length > 0
            padding: 4

            ListView {
                id: resultsList
                anchors.fill: parent
                clip: true
                model: App.searchResults
                delegate: ItemDelegate {
                    width: resultsList.width
                    text: modelData.displayName
                    onClicked: page.chooseCandidate(modelData)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                id: mapHolder
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height)
                height: width
                visible: page.hasCenter

                MapCanvas {
                    id: canvas
                    anchors.fill: parent
                    active: page.hasCenter
                    groundSpanMeters: 70
                    attribution: "USGS, USDA — The National Map: Orthoimagery"
                }

                Item {
                    anchors.fill: parent
                    z: 50

                    Rectangle {
                        anchors.centerIn: parent
                        width: 28
                        height: 2
                        color: "#c62828"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 2
                        height: 28
                        color: "#c62828"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 9
                        height: 9
                        radius: 5
                        color: "transparent"
                        border.color: "#ffffff"
                        border.width: 2
                    }
                }

                Column {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 8
                    spacing: 4
                    z: 60

                    Button {
                        text: "+"
                        font.pixelSize: 20
                        implicitWidth: 38
                        implicitHeight: 38
                        padding: 0
                        focusPolicy: Qt.NoFocus
                        enabled: canvas.zoomFactor < 8
                        onClicked: canvas.zoomIn()
                    }

                    Button {
                        text: "−"
                        font.pixelSize: 20
                        implicitWidth: 38
                        implicitHeight: 38
                        padding: 0
                        focusPolicy: Qt.NoFocus
                        enabled: canvas.zoomFactor > 0.5
                        onClicked: canvas.zoomOut()
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    z: 51
                    visible: !canvas.tilesReady
                    text: "Loading aerial imagery…"
                    padding: 6
                    color: "#ffffff"
                    background: Rectangle {
                        color: "#99000000"
                        radius: 4
                    }
                }
            }

            Rectangle {
                visible: !page.hasCenter
                anchors.centerIn: parent
                width: Math.min(parent.width, 520)
                height: 110
                radius: 8
                color: "#eef2ef"
                border.color: "#cfd9d1"

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Search for an address to load the map."
                    color: "#5b6d5f"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: page.hasCenter ? page.chosenAddress : ""
                elide: Text.ElideRight
                maximumLineCount: 1
                color: "#3f5545"
            }

            Label {
                visible: page.hasCenter
                text: canvas.centerLat.toFixed(5) + ", " + canvas.centerLon.toFixed(5)
                font.family: "monospace"
                font.pixelSize: 11
                color: "#68796c"
            }

            BusyIndicator {
                running: page.saving
                visible: page.saving
                implicitWidth: 28
                implicitHeight: 28
            }

            Button {
                text: "Save map"
                enabled: page.hasCenter && canvas.tilesReady && !page.saving
                onClicked: page.saveMap()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: App.errorMessage.length > 0
            text: App.errorMessage
            color: "#b71c1c"
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: App.errorMessage.length === 0 && App.statusMessage.length > 0
            text: App.statusMessage
            color: "#5b6d5f"
            elide: Text.ElideRight
        }
    }
}
