#!/usr/bin/env bash
# Build poisonlog in Release mode and package it as an AppImage in /tmp.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP=poisonlog
QT_DIR="${QT_DIR:-/home/dwight/Qt/6.12.0/gcc_64}"
BUILD_DIR="$REPO_DIR/build-appimage"
APPDIR=/tmp/$APP.AppDir

# --- toolchain ---
if ! cmake --version | grep -q "3.2[5-9]\|3\.[3-9]"; then
    CMAKE_BIN=/home/dwight/Qt/Tools/CMake/bin/cmake
else
    CMAKE_BIN="$(command -v cmake)"
fi

# --- tools (download missing ones into /tmp/tools) ---
TOOLS_DIR=/tmp/tools
mkdir -p "$TOOLS_DIR"
if ! command -v linuxdeploy >/dev/null; then
    LINUXDEPLOY="$TOOLS_DIR/linuxdeploy"
    if [ ! -x "$LINUXDEPLOY" ]; then
        curl -sL -o "$LINUXDEPLOY" \
            https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
        chmod +x "$LINUXDEPLOY"
    fi
else
    LINUXDEPLOY="$(command -v linuxdeploy)"
fi
QTPLUGIN="$TOOLS_DIR/linuxdeploy-plugin-qt"
if [ ! -x "$QTPLUGIN" ]; then
    curl -sL -o "$QTPLUGIN" \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x "$QTPLUGIN"
fi

# --- build ---
"$CMAKE_BIN" -S "$REPO_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DCMAKE_BUILD_TYPE=Release
"$CMAKE_BIN" --build "$BUILD_DIR"

# --- stage AppDir ---
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
cp "$BUILD_DIR/$APP" "$APPDIR/usr/bin/"

cat > "$APPDIR/$APP.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=PoisonLog
Exec=$APP
Icon=$APP
Categories=Office;
EOF

# minimal icon (plain PNG), required for a valid desktop entry
ICON_DIR="$APPDIR/usr/share/icons/hicolor/128x128/apps"
if command -v convert >/dev/null; then
    mkdir -p "$ICON_DIR"
    convert -size 128x128 xc:forestgreen "$ICON_DIR/$APP.png"
    cp "$ICON_DIR/$APP.png" "$APPDIR/$APP.png"
fi

# --- package ---
export PATH="$TOOLS_DIR/bin:$QT_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$QT_DIR/lib"
export QML_SOURCES_PATHS="$REPO_DIR/qml"

# linuxdeploy-plugin-qt copies every plugin of every linked module and fails on
# system libs that are missing (odbc, libpq, oracle, ...). The app only needs
# the sqlite driver, so expose a pruned view of the Qt plugin dir through a
# qmake shim that rewrites QT_INSTALL_PLUGINS.
STAGE_QT=/tmp/qt-stage
STAGE_PLUGINS="$STAGE_QT/plugins"
rm -rf "$STAGE_QT"
mkdir -p "$STAGE_PLUGINS"
for d in "$QT_DIR/plugins"/*; do
    base="$(basename "$d")"
    if [ "$base" = "sqldrivers" ]; then
        mkdir -p "$STAGE_PLUGINS/sqldrivers"
        ln -s "$QT_DIR/plugins/sqldrivers/libqsqlite.so" "$STAGE_PLUGINS/sqldrivers/"
    else
        ln -s "$d" "$STAGE_PLUGINS/$base"
    fi
done
mkdir -p "$TOOLS_DIR/bin"
cat > "$TOOLS_DIR/bin/qmake" <<EOF
#!/usr/bin/env bash
"$QT_DIR/bin/qmake" "\$@" | sed "s|$QT_DIR/plugins|$STAGE_PLUGINS|g"
EOF
chmod +x "$TOOLS_DIR/bin/qmake"
export QMAKE="$TOOLS_DIR/bin/qmake"

# ship Wayland platform plugins when present (for hybrid X11/Wayland systems)
for pl in libqwayland-egl.so libqwayland-generic.so; do
    if [ -f "$QT_DIR/plugins/platforms/$pl" ]; then
        EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:+$EXTRA_PLATFORM_PLUGINS;}$pl"
    fi
done
export EXTRA_PLATFORM_PLUGINS

VERSION="$(git -C "$REPO_DIR" describe --tags --always 2>/dev/null || echo 0.1)"
(
    cd /tmp
    "$LINUXDEPLOY" --appdir "$APPDIR" --desktop-file "$APPDIR/$APP.desktop" --plugin qt --output appimage >/tmp/${APP}_appimage.log 2>&1
)
# linuxdeploy names it after the .desktop file's Name (no version)
mv -f "/tmp/PoisonLog-x86_64.AppImage" "/tmp/${APP}-${VERSION}-x86_64.AppImage"
echo "AppImage: /tmp/${APP}-${VERSION}-x86_64.AppImage"
