#!/usr/bin/env bash
set -euo pipefail

# Install build and runtime dependencies for FontForgeGUI and headless libfontforge
# on Debian / Ubuntu systems.

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
  SUDO=""
else
  SUDO="sudo"
fi

${SUDO} apt-get update
${SUDO} apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  qt6-base-dev \
  qt6-svg-dev \
  libfreetype-dev \
  libxml2-dev \
  zlib1g-dev \
  libwoff-dev \
  libspiro-dev \
  libharfbuzz-dev \
  libpng-dev \
  libjpeg-dev \
  libtiff-dev \
  libgif-dev \
  libunibreak-dev

