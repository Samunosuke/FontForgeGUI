#!/usr/bin/env bash
set -euo pipefail

# Clone or update the upstream FontForge repository into ../FontForge
# (sibling directory of FontForgeGUI, used by CMake's FONTFORGE_SOURCE_DIR).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TARGET_DIR="$(cd "${REPO_ROOT}/.." && pwd)/FontForge"
REPO_URL="https://github.com/fontforge/fontforge.git"
BRANCH=""
SHALLOW=true
UPDATE=false

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --target <path>   Destination directory (default: ../FontForge)
  --branch <name>   Specific branch or tag to clone
  --full            Perform a full git clone (default: --depth 1)
  --update          If target exists, pull latest changes (--ff-only)
  -h, --help        Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --target)
      [[ $# -ge 2 ]] || { echo "Missing value for --target" >&2; exit 2; }
      TARGET_DIR="$2"
      shift 2
      ;;
    --branch)
      [[ $# -ge 2 ]] || { echo "Missing value for --branch" >&2; exit 2; }
      BRANCH="$2"
      shift 2
      ;;
    --full)
      SHALLOW=false
      shift
      ;;
    --update)
      UPDATE=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if ! command -v git >/dev/null 2>&1; then
  echo "Error: git is required but not installed." >&2
  exit 1
fi

if [[ -d "${TARGET_DIR}" ]]; then
  if [[ ! -d "${TARGET_DIR}/.git" ]]; then
    echo "Error: '${TARGET_DIR}' exists and is not a git repository." >&2
    exit 1
  fi
  if [[ "${UPDATE}" == false ]]; then
    echo "FontForge already exists at '${TARGET_DIR}'."
    echo "Re-run with --update to pull latest upstream changes."
    exit 0
  fi
  if [[ -n "$(git -C "${TARGET_DIR}" status --porcelain)" ]]; then
    echo "Error: '${TARGET_DIR}' has uncommitted local changes; skipping pull." >&2
    exit 1
  fi
  echo "==> Updating FontForge in '${TARGET_DIR}'"
  git -C "${TARGET_DIR}" pull --ff-only
  exit 0
fi

mkdir -p "$(dirname "${TARGET_DIR}")"

CLONE_ARGS=(git clone)
if [[ "${SHALLOW}" == true ]]; then
  CLONE_ARGS+=(--depth 1)
fi
if [[ -n "${BRANCH}" ]]; then
  CLONE_ARGS+=(--branch "${BRANCH}")
fi
CLONE_ARGS+=("${REPO_URL}" "${TARGET_DIR}")

echo "==> Cloning FontForge from ${REPO_URL} into '${TARGET_DIR}'"
"${CLONE_ARGS[@]}"
echo "==> Done! FontForge is ready at '${TARGET_DIR}'"

