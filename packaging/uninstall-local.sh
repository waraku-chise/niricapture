#!/usr/bin/env bash
# Remove files installed by packaging/install-local.sh. Never touches config.
set -euo pipefail

prefix=${NIRICAPTURE_PREFIX:-${HOME}/.local}
check=false

while (($#)); do
  case $1 in
    --check) check=true ;;
    --prefix)
      [[ $# -ge 2 ]] || { echo "--prefix needs a path" >&2; exit 2; }
      prefix=$2
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [--prefix PATH] [--check]"
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 2
      ;;
  esac
  shift
done

[[ ${prefix} == / || ${prefix} == /usr || ${prefix} == "${HOME}" ]] && {
  echo "Refusing to uninstall from ${prefix}; pass a scoped --prefix." >&2
  exit 1
}

paths=(
  "${prefix}/bin/niricapture"
  "${prefix}/share/applications/niricapture.desktop"
  "${prefix}/share/niricapture"
  "${prefix}/share/licenses/niricapture"
  "${prefix}/share/doc/niricapture"
)

announce() { printf '==> %s\n' "$*"; }

for path in "${paths[@]}"; do
  if [[ -e ${path} || -L ${path} ]]; then
    announce "Would remove ${path}"
  fi
done

if "${check}"; then
  announce "Uninstall preflight complete (no files removed)"
  exit 0
fi

for path in "${paths[@]}"; do
  if [[ -d ${path} && ! -L ${path} ]]; then
    rm -rf -- "${path}"
  elif [[ -e ${path} || -L ${path} ]]; then
    rm -f -- "${path}"
  fi
done

announce "NiriCapture removed from ${prefix}"
printf '%s\n' "Niri/DMS configuration and screenshots were not touched."
