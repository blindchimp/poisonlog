import QtQuick

Item {
    id: root
    objectName: "mapCanvas"
    clip: true

    property real centerLat: 0
    property real centerLon: 0
    property real groundSpanMeters: 70
    property real zoomFactor: 1
    property string attribution: "USGS, USDA — The National Map: Orthoimagery"
    property bool active: true

    readonly property real effectiveGroundSpanMeters: groundSpanMeters / Math.max(zoomFactor, 1e-6)
    readonly property real effectiveZoom: {
        const mppAtZoomZero = 156543.03392 * Math.cos(centerLat * Math.PI / 180)
        return Math.log(Math.max(1e-9, mppAtZoomZero * Math.max(width, 1) / effectiveGroundSpanMeters))
            / Math.LN2
    }
    readonly property int tileZoom: Math.max(1, Math.min(19, Math.floor(effectiveZoom)))
    readonly property real tileScale: Math.pow(2, effectiveZoom - tileZoom)
    readonly property real tileDisplaySize: 256 * tileScale
    readonly property real centerWorldX: lonToWorldX(centerLon, effectiveZoom)
    readonly property real centerWorldY: latToWorldY(centerLat, effectiveZoom)
    readonly property real tileCenterWorldX: lonToWorldX(centerLon, tileZoom)
    readonly property real tileCenterWorldY: latToWorldY(centerLat, tileZoom)
    readonly property real tileHalfWidthWorld: (width / 2) / Math.max(tileScale, 1e-9)
    readonly property real tileHalfHeightWorld: (height / 2) / Math.max(tileScale, 1e-9)
    readonly property int visibleMinTileX: Math.floor((tileCenterWorldX - tileHalfWidthWorld) / 256)
    readonly property int visibleMaxTileX: Math.floor((tileCenterWorldX + tileHalfWidthWorld) / 256)
    readonly property int visibleMinTileY: Math.floor((tileCenterWorldY - tileHalfHeightWorld) / 256)
    readonly property int visibleMaxTileY: Math.floor((tileCenterWorldY + tileHalfHeightWorld) / 256)
    property var tileItems: []
    property bool tilesReady: false

    function normalizeLon(lon) {
        return ((lon + 180) % 360 + 360) % 360 - 180
    }

    function lonToWorldX(lon, zoom) {
        return (normalizeLon(lon) + 180) / 360 * 256 * Math.pow(2, zoom)
    }

    function latToWorldY(lat, zoom) {
        const clamped = Math.max(-85.05112878, Math.min(85.05112878, lat))
        const radians = clamped * Math.PI / 180
        return (1 - Math.log(Math.tan(radians) + 1 / Math.cos(radians)) / Math.PI)
            / 2 * 256 * Math.pow(2, zoom)
    }

    function computeTiles() {
        if (!active || width <= 0 || height <= 0 || !isFinite(effectiveZoom))
            return []

        const zoom = tileZoom
        const minX = visibleMinTileX
        const maxX = visibleMaxTileX
        const minY = visibleMinTileY
        const maxY = visibleMaxTileY
        const tileCount = 1 << zoom
        const result = []

        for (let tileY = minY; tileY <= maxY; ++tileY) {
            if (tileY < 0 || tileY >= tileCount)
                continue
            for (let tileX = minX; tileX <= maxX; ++tileX) {
                const sourceX = ((tileX % tileCount) + tileCount) % tileCount
                result.push({
                    sourceX: sourceX,
                    tileX: tileX,
                    tileY: tileY
                })
            }
        }
        return result
    }

    function zoomIn() {
        zoomFactor = Math.min(8, zoomFactor * 2)
    }

    function zoomOut() {
        zoomFactor = Math.max(0.5, zoomFactor / 2)
    }

    function panBy(dx, dy) {
        if (!active || width <= 0 || height <= 0)
            return

        const zoom = effectiveZoom
        const worldPixels = 256 * Math.pow(2, zoom)
        const centerX = lonToWorldX(centerLon, zoom) - dx
        const centerY = latToWorldY(centerLat, zoom) - dy
        const newLon = normalizeLon(centerX / worldPixels * 360 - 180)
        const mercatorY = Math.PI - 2 * Math.PI * centerY / worldPixels
        const newLat = Math.atan(Math.sinh(mercatorY)) * 180 / Math.PI

        centerLat = Math.max(-85, Math.min(85, newLat))
        centerLon = newLon
    }

    function evaluateTileReadiness() {
        let ready = repeater.count > 0
        for (let i = 0; i < repeater.count; ++i) {
            const image = repeater.itemAt(i)
            if (!image || image.status !== Image.Ready) {
                ready = false
                break
            }
        }
        if (tilesReady !== ready)
            tilesReady = ready
    }

    function rebuildTiles() {
        console.warn("rebuild", width, height, tileZoom, centerLat, centerLon, visibleMinTileX, visibleMaxTileX, visibleMinTileY, visibleMaxTileY)
        tileItems = computeTiles()
    }

    onTileItemsChanged: tilesReady = false
    onVisibleMinTileXChanged: rebuildTiles()
    onVisibleMaxTileXChanged: rebuildTiles()
    onVisibleMinTileYChanged: rebuildTiles()
    onVisibleMaxTileYChanged: rebuildTiles()
    onTileZoomChanged: rebuildTiles()
    onWidthChanged: rebuildTiles()
    onHeightChanged: rebuildTiles()
    onActiveChanged: rebuildTiles()
    Component.onCompleted: rebuildTiles()

    Repeater {
        id: repeater
        model: root.tileItems
        delegate: Image {
            required property var modelData

            x: modelData.tileX * root.tileDisplaySize - root.centerWorldX + root.width / 2
            y: modelData.tileY * root.tileDisplaySize - root.centerWorldY + root.height / 2
            width: root.tileDisplaySize
            height: root.tileDisplaySize
            source: root.active
                ? "image://tiles/" + root.tileZoom + "/" + modelData.sourceX + "/" + modelData.tileY
                : ""
            fillMode: Image.Stretch
            asynchronous: true
            cache: true
            smooth: true
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.active
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.ClosedHandCursor

        property real lastX: 0
        property real lastY: 0

        onPressed: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y
        }
        onPositionChanged: (mouse) => {
            if (!pressed)
                return
            root.panBy(mouse.x - lastX, mouse.y - lastY)
            lastX = mouse.x
            lastY = mouse.y
        }
    }

    Timer {
        interval: 80
        repeat: true
        running: root.active
        onTriggered: root.evaluateTileReadiness()
    }

    Rectangle {
        z: 10
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        width: attributionText.implicitWidth + 10
        height: attributionText.implicitHeight + 4
        radius: 2
        color: "#e6ffffff"

        Text {
            id: attributionText
            anchors.centerIn: parent
            text: root.attribution
            font.pixelSize: 10
            color: "#304437"
        }
    }
}
