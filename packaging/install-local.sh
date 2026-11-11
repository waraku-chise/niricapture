#!/usr/bin/env bash
# Build and install NiriCapture for the current Arch/CachyOS user.
set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "Refusing to run as root; sudo is used only for Arch packages." >&2
  exit 1
fi

check=false
case ${1:-} in
  '') ;;
  --check) check=true ;;
  *)
    echo "Usage: $0 [--check]" >&2
    exit 2
    ;;
esac

[[ -f /etc/arch-release ]] || {
  echo "Unsupported system: this installer targets Arch Linux/CachyOS." >&2
  exit 1
}

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
prefix=${NIRICAPTURE_PREFIX:-${HOME}/.local}
build_dir=${XDG_CACHE_HOME:-${HOME}/.cache}/niricapture/build
manifest=${build_dir}/install_manifest.txt

packages=(
  base-devel cmake ninja pkgconf
  qt6-base qt6-declarative layer-shell-qt
  grim wl-clipboard libnotify
  tesseract tesseract-data-eng
)

announce() { printf '==> %s\n' "$*"; }

for path in CMakeLists.txt README.md niricapture.desktop niricapture-editor.desktop \
            packaging/niricapture-keybinds.kdl NOTICE \
            LICENSES/MIT-nirisnap.txt; do
  [[ -f ${repo_root}/${path} ]] || {
    echo "Missing source file: ${repo_root}/${path}" >&2
    exit 1
  }
done

install_paths=(
  "${prefix}/bin/niricapture"
  "${prefix}/share/applications/niricapture.desktop"
  "${prefix}/share/applications/niricapture-editor.desktop"
  "${prefix}/share/niricapture/niricapture-keybinds.kdl"
  "${prefix}/share/licenses/niricapture"
  "${prefix}/share/doc/niricapture"
)
for path in "${install_paths[@]}"; do
  [[ ! -e ${path} && ! -L ${path} ]] || {
    echo "Refusing to overwrite existing install path: ${path}" >&2
    echo "Run packaging/uninstall-local.sh first, or use a different prefix." >&2
    exit 1
  }
done

announce "Arch packages: ${packages[*]}"
announce "Build directory: ${build_dir}"
announce "Install prefix: ${prefix}"
if command -v dms >/dev/null 2>&1; then
  announce "DMS CLI found: $(command -v dms)"
else
  announce "DMS CLI not found; region/fullscreen will fall back to grim"
fi
if [[ -f /usr/share/tessdata/chi_sim.traineddata ]]; then
  announce "Chinese OCR model found"
else
  announce "Optional Chinese OCR model: sudo pacman -S tesseract-data-chi_sim"
fi
printf '%s\n' "Will not edit Niri or DMS configuration."

if "${check}"; then
  announce "Preflight passed (no files written)"
  printf 'Would run: /usr/bin/sudo -- /usr/bin/pacman -S --needed'
  printf ' %q' "${packages[@]}"
  printf '\n'
  printf 'Would build in %s and install under %s\n' "${build_dir}" "${prefix}"
  exit 0
fi

[[ -x /usr/bin/sudo && -x /usr/bin/pacman ]] || {
  echo "/usr/bin/sudo or /usr/bin/pacman not found." >&2
  exit 1
}
for command in cmake ninja niri; do
  command -v "${command}" >/dev/null || {
    echo "${command} not found." >&2
    exit 1
  }
done

announce "Installing build and runtime dependencies"
/usr/bin/sudo -- /usr/bin/pacman -S --needed "${packages[@]}"

announce "Building NiriCapture"
cmake -S "${repo_root}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${prefix}"
cmake --build "${build_dir}" --parallel
QT_QPA_PLATFORM=offscreen "${build_dir}/niricapture-smoke" \
  "${build_dir}/niricapture-smoke-output"
QT_QPA_PLATFORM=offscreen "${build_dir}/niricapture-desktop-smoke"

announce "Installing NiriCapture"
cmake --install "${build_dir}"

cat <<EOF

Installed NiriCapture under ${prefix}.

Next steps (manual; nothing was edited):
  1. Add one of the bindings from
     ${prefix}/share/niricapture/niricapture-keybinds.kdl
     to your Niri binds block (or an included file).
  2. Run: niri validate
  3. Press Print for region capture, Shift+Print for native window capture.
  4. Optional Chinese OCR: sudo pacman -S tesseract-data-chi_sim

Uninstall: ${repo_root}/packaging/uninstall-local.sh --prefix "${prefix}"
EOF
