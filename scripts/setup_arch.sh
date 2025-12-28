#!/usr/bin/env bash
set -euo pipefail

echo "[1/6] Installing base packages..."
sudo pacman -Syu --noconfirm
sudo pacman -S --needed --noconfirm \
  git cmake ninja \
  python python-pip python-virtualenv \
  base-devel gperf ccache dfu-util libusb \
  android-tools \
  jdk17-openjdk \
  wget tar

echo "[2/6] Setting up groups (plugdev for adb; uucp/lock for serial)..."
sudo groupadd -f plugdev
sudo usermod -aG plugdev,uucp,lock "$USER"

echo "[3/6] Installing udev rules for Android (adb without sudo)..."
sudo tee /etc/udev/rules.d/51-android.rules >/dev/null <<'EOF'
# Common Android vendors
SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666", GROUP="plugdev"   # Google
SUBSYSTEM=="usb", ATTR{idVendor}=="04e8", MODE="0666", GROUP="plugdev"   # Samsung
SUBSYSTEM=="usb", ATTR{idVendor}=="2a70", MODE="0666", GROUP="plugdev"   # OnePlus/Oppo (varies)
SUBSYSTEM=="usb", ATTR{idVendor}=="12d1", MODE="0666", GROUP="plugdev"   # Huawei
SUBSYSTEM=="usb", ATTR{idVendor}=="22b8", MODE="0666", GROUP="plugdev"   # Motorola
SUBSYSTEM=="usb", ATTR{idVendor}=="0bb4", MODE="0666", GROUP="plugdev"   # HTC
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "[4/6] Done. IMPORTANT: log out/in (or reboot) for group changes to apply."
echo "    After relogin: run 'adb devices' to verify."

echo "[5/6] ESP-IDF: you still need to install ESP-IDF (see scripts/espidf_install.sh)."
echo "[6/6] Android Studio: install via tarball (see scripts/android_studio_install.sh)."
