#!/usr/bin/env bash
set -euo pipefail

# Build FontForgeGUI and headless libfontforge on Ubuntu / Debian using CMake + Ninja.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SRC_DIR="${REPO_ROOT}/src"

BUILD_DIR="build"
BUILD_TYPE="RelWithDebInfo"
FONTFORGE_DIR=""
INSTALL_DEPS=false
SKIP_TESTS=false
CLEAN=false

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --install-deps           Run scripts/install_dependencies.sh before building
  --build-dir <path>       Build directory (default: build)
  --build-type <type>      CMake build type: Debug|Release|RelWithDebInfo|MinSizeRel (default: RelWithDebInfo)
  --fontforge-dir <path>   Override FONTFORGE_SOURCE_DIR (default: ../FontForge)
  --skip-tests             Skip running CTest after building
  --clean                  Remove build directory before configuring
  -h, --help               Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-deps)
      INSTALL_DEPS=true
      shift
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || { echo "Missing value for --build-dir" >&2; exit 2; }
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      [[ $# -ge 2 ]] || { echo "Missing value for --build-type" >&2; exit 2; }
      BUILD_TYPE="$2"
      shift 2
      ;;
    --fontforge-dir)
      [[ $# -ge 2 ]] || { echo "Missing value for --fontforge-dir" >&2; exit 2; }
      FONTFORGE_DIR="$2"
      shift 2
      ;;
    --skip-tests)
      SKIP_TESTS=true
      shift
      ;;
    --clean)
      CLEAN=true
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

case "${BUILD_TYPE}" in
  Debug|Release|RelWithDebInfo|MinSizeRel) ;;
  *)
    echo "Invalid --build-type '${BUILD_TYPE}'. Expected Debug, Release, RelWithDebInfo, or MinSizeRel." >&2
    exit 2
    ;;
esac

if [[ "${BUILD_DIR}" = /* ]]; then
  FULL_BUILD_DIR="${BUILD_DIR}"
else
  FULL_BUILD_DIR="${REPO_ROOT}/${BUILD_DIR}"
fi

if [[ "${INSTALL_DEPS}" == true ]]; then
  echo "==> Installing Ubuntu dependencies"
  "${SCRIPT_DIR}/install_dependencies.sh"
fi

if [[ "${CLEAN}" == true && -d "${FULL_BUILD_DIR}" ]]; then
  echo "==> Removing build directory '${FULL_BUILD_DIR}'"
  rm -rf "${FULL_BUILD_DIR}"
fi

CMAKE_ARGS=(
  -G Ninja
  -S "${SRC_DIR}"
  -B "${FULL_BUILD_DIR}"
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  -DBUILD_TESTING=ON
)

if [[ -n "${FONTFORGE_DIR}" ]]; then
  RESOLVED_FF="$(cd "${FONTFORGE_DIR}" && pwd)"
  CMAKE_ARGS+=("-DFONTFORGE_SOURCE_DIR=${RESOLVED_FF}")
fi

echo "==> Configuring CMake (Ninja / ${BUILD_TYPE})"
cmake "${CMAKE_ARGS[@]}"

echo "==> Building FontForgeGUI"
cmake --build "${FULL_BUILD_DIR}" --parallel

if [[ "${SKIP_TESTS}" == false ]]; then
  echo "==> Running CTest suite"
  QT_QPA_PLATFORM=offscreen ctest --test-dir "${FULL_BUILD_DIR}" --output-on-failure
fi

echo "==> Done! Run: ${FULL_BUILD_DIR}/FontForgeGUI"

