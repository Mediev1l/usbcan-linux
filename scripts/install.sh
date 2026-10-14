#!/usr/bin/env bash
# Build and install usbcan-linux for the current user.
#
#   scripts/install.sh            build (Release) and install to ~/.local
#   scripts/install.sh deps       install the Ubuntu 24.04 build dependencies (sudo)
#   scripts/install.sh uninstall  remove what `install` put in place
#
# PREFIX=/some/where scripts/install.sh   installs somewhere else.
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SOURCE_DIR/build/install"

FILES=(
    "bin/usbcan-linux"
    "share/applications/usbcan-linux.desktop"
    "share/icons/hicolor/scalable/apps/usbcan-linux.svg"
)

do_deps() {
    sudo apt install -y \
        cmake ninja-build g++ \
        qt6-base-dev qt6-serialport-dev qt6-declarative-dev qt6-declarative-dev-tools \
        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
        qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtquick-dialogs \
        qml6-module-qtqml-workerscript qml6-module-qt-labs-settings qml6-module-qttest \
        libgtest-dev libgmock-dev
}

do_install() {
    cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DUSBCAN_BUILD_TESTS=OFF
    cmake --build "$BUILD_DIR"
    cmake --install "$BUILD_DIR"
    command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" || true

    echo
    echo "Installed to $PREFIX. Start it from the application menu or run: $PREFIX/bin/usbcan-linux"
    if ! id -nG | grep -qw dialout; then
        echo "Note: for the Waveshare adapter you need the dialout group:"
        echo "  sudo usermod -aG dialout \$USER   (then log out and back in)"
    fi
}

do_uninstall() {
    for f in "${FILES[@]}"; do
        rm -fv "$PREFIX/$f"
    done
    command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" || true
    echo "Saved messages and settings are kept in ~/.local/share/usbcan-linux and ~/.config/usbcan-linux."
}

case "${1:-install}" in
    install) do_install ;;
    deps) do_deps ;;
    uninstall) do_uninstall ;;
    *) echo "usage: $0 [install|deps|uninstall]" >&2; exit 2 ;;
esac
