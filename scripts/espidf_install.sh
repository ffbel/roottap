#!/usr/bin/env bash
set -euo pipefail

IDF_HOME="${IDF_HOME:-$HOME/esp}"
IDF_PATH="${IDF_PATH:-$IDF_HOME/esp-idf}"
IDF_VERSION="${IDF_VERSION:-v5.2.2}"

mkdir -p "$IDF_HOME"

if [[ -d "$IDF_PATH/.git" ]]; then
  echo "ESP-IDF already exists at $IDF_PATH"
else
  echo "Cloning ESP-IDF into $IDF_PATH..."
  git clone --recursive https://github.com/espressif/esp-idf.git "$IDF_PATH"
fi

cd "$IDF_PATH"
echo "Checking out ESP-IDF $IDF_VERSION..."
git fetch --tags
git checkout "$IDF_VERSION"
git submodule update --init --recursive

echo "Installing ESP-IDF tools for esp32s3..."
./install.sh esp32s3
echo "source $IDF_PATH/export.sh > /dev/null" >> ${HOME}/.zshrc
. $IDF_PATH/export.sh && idf.py add-dependency "espressif/esp_tinyusb^1.5.0"


echo 

echo
echo "Done."
