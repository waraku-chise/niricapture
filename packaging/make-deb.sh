#!/usr/bin/env bash
# Build a Debian package from an existing CMake build tree.
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-${repo_root}/build}
out_dir=${2:-${repo_root}/dist}
version=$(sed -n 's/^project(niricapture VERSION \([^ ]*\).*/\1/p' \
  "${repo_root}/CMakeLists.txt" | head -n 1)
[[ -n ${version} ]] || { echo "Could not read project version" >&2; exit 1; }
[[ -x ${build_dir}/niricapture ]] || {
  echo "Missing ${build_dir}/niricapture; run cmake --build first." >&2
  exit 1
}
command -v dpkg-deb >/dev/null || {
  echo "dpkg-deb not found. On Arch: sudo pacman -S --needed dpkg" >&2
  exit 1
}

stage=$(mktemp -d)
trap 'rm -rf -- "${stage}"' EXIT
root=${stage}/root
mkdir -p "${root}/DEBIAN" "${out_dir}"

DESTDIR= cmake --install "${build_dir}" --prefix "${root}/usr" \
  >/dev/null

install -Dm644 "${repo_root}/NOTICE" \
  "${root}/usr/share/doc/niricapture/NOTICE"
install -Dm644 "${repo_root}/LICENSES/MIT-nirisnap.txt" \
  "${root}/usr/share/doc/niricapture/MIT-nirisnap.txt"
install -Dm644 "${repo_root}/LICENSES/Apache-2.0.txt" \
  "${root}/usr/share/doc/niricapture/Apache-2.0.txt"
install -Dm644 "${repo_root}/LICENSE" \
  "${root}/usr/share/doc/niricapture/copyright"
chmod 0755 "${root}/usr/bin/niricapture"
find "${root}" -type d -exec chmod 0755 {} +

installed_kb=$(du -sk "${root}/usr" | awk '{print $1}')
cat >"${root}/DEBIAN/control" <<CONTROL
Package: niricapture
Version: ${version}
Section: graphics
Priority: optional
Architecture: amd64
Maintainer: NiriCapture contributors <niricapture@users.noreply.github.com>
Installed-Size: ${installed_kb}
Depends: libc6 (>= 2.35), libstdc++6 (>= 12), libqt6core6 (>= 6.12) | libqt6core6t64 (>= 6.12), libqt6gui6 (>= 6.12) | libqt6gui6t64 (>= 6.12), libqt6widgets6 (>= 6.12) | libqt6widgets6t64 (>= 6.12), libqt6concurrent6 (>= 6.12) | libqt6concurrent6t64 (>= 6.12), libqt6dbus6 (>= 6.12) | libqt6dbus6t64 (>= 6.12), libqt6waylandclient6 (>= 6.12) | libqt6waylandclient6t64 (>= 6.12), libqt6quick6 (>= 6.12), libqt6quickwidgets6 (>= 6.12), qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, liblayershellqtinterface6, libwayland-client0, libxkbcommon0, libgl1, libegl1, libfontconfig1, libfreetype6, libharfbuzz0b, libpng16-16
Recommends: grim, wl-clipboard, libnotify-bin, tesseract-ocr, tesseract-ocr-eng, tesseract-ocr-chi-sim
Homepage: https://github.com/waraku-chise/niricapture
Description: Native Niri screenshot tool with optional DMS frozen frames
 NiriCapture freezes outputs through DMS when available (grim fallback), provides a
 multi-output region selection overlay, and edits annotations in place.
 Window capture uses Niri's compositor-side screenshot-window action, so
 occluded windows are rendered independently of the desktop.
CONTROL

deb="${out_dir}/niricapture_${version}_amd64.deb"
dpkg-deb --root-owner-group -Zxz --build "${root}" "${deb}" >/dev/null
printf '%s\n' "${deb}"
