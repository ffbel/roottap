#!/usr/bin/env bash
set -euo pipefail

# ===== CONFIG =====
STUDIO_VERSION="2025.2.2.8"
STUDIO_ARCHIVE="android-studio-${STUDIO_VERSION}-linux.tar.gz"

STUDIO_PREFIX="/opt/android-studio"
SDK_PREFIX="/opt/android-sdk"

PROFILE_FILE="${HOME}/.zshrc"
DESKTOP_FILE="/usr/share/applications/android-studio.desktop"
BIN_LINK="/usr/local/bin/android-studio"


echo "==> Installing Android Studio ${STUDIO_VERSION} system-wide"

# ===== ANDROID STUDIO =====
echo "==> Installing Android Studio to ${STUDIO_PREFIX}"

# rm -rf "${STUDIO_PREFIX}"

cd ~/Downloads

echo "Extracting..."
tar -xzf "${STUDIO_ARCHIVE}"
rm -f "${STUDIO_ARCHIVE}"

sudo mv android-studio "${STUDIO_PREFIX}"

sudo chown -R root:root "${STUDIO_PREFIX}"
sudo chmod -R a+rX "${STUDIO_PREFIX}"

# ===== ANDROID SDK =====
echo "==> Setting up Android SDK at ${SDK_PREFIX}"

sudo mkdir -p "${SDK_PREFIX}"
sudo chown -R root:plugdev "${SDK_PREFIX}"
sudo chmod -R 775 "${SDK_PREFIX}"

# ===== ENVIRONMENT =====
echo "==> Writing system environment variables (${PROFILE_FILE})"

echo "export ANDROID_SDK_ROOT=${SDK_PREFIX}" >> "${PROFILE_FILE}"
echo "export ANDROID_HOME=${SDK_PREFIX}" >> "${PROFILE_FILE}"
echo "export PATH=\$PATH:${SDK_PREFIX}/platform-tools:${SDK_PREFIX}/emulator" >> "${PROFILE_FILE}"

# cat > "${PROFILE_FILE}" <<EOF
# export ANDROID_SDK_ROOT=${SDK_PREFIX}
# export ANDROID_HOME=${SDK_PREFIX}
# export PATH=\$PATH:${SDK_PREFIX}/platform-tools:${SDK_PREFIX}/emulator
# EOF

# chmod 644 "${PROFILE_FILE}"

# ===== SYMLINK =====
echo "==> Creating launcher symlink ${BIN_LINK}"

sudo ln -sf "${STUDIO_PREFIX}/bin/studio.sh" "${BIN_LINK}"

# # ===== DESKTOP ENTRY =====
# echo "==> Installing desktop entry"

# cat > "${DESKTOP_FILE}" <<EOF
# [Desktop Entry]
# Name=Android Studio
# Comment=Android IDE
# Exec=${STUDIO_PREFIX}/bin/studio.sh
# Icon=${STUDIO_PREFIX}/bin/studio.svg
# Terminal=false
# Type=Application
# Categories=Development;IDE;
# StartupWMClass=jetbrains-studio
# EOF

# chmod 644 "${DESKTOP_FILE}"

# ===== DONE =====
echo
echo "✅ Android Studio installed system-wide"
echo
echo "Next steps:"
echo "  1) Log out / log in (or: source ${PROFILE_FILE})"
echo "  2) Run: android-studio"
echo "  3) When prompted, set SDK path to: ${SDK_PREFIX}"
echo
echo "Verify:"
echo "  which android-studio"
echo "  echo \$ANDROID_SDK_ROOT"
